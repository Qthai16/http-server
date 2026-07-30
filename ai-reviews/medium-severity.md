# Medium Severity Bugs

---

## 1. `str_to_method` uses prefix matching — "GETHELLO" parses as GET
**`libs/http/HttpMessage.cpp:139–158`**

```cpp
if (strncmp(str.c_str(), "GET", 3) == 0) {   // matches any string starting with "GET"
    return {true, HTTPMethod::GET};
} else if (strncmp(str.c_str(), "PUT", 3) == 0) {  // also matches "PATCH" prefix if reordered
```

`strncmp` only checks a prefix. A malformed request-line like `GETHELLO / HTTP/1.1`
is silently accepted as GET. "PUT" (len 3) would also match "PUTX". Clients or
fuzzers sending garbage methods bypass method validation entirely.

**Fix:** after `strncmp` succeeds, verify the character immediately after the
method token is a space (the delimiter required by RFC 7230):
```cpp
auto check = [&](const char *method, size_t len, HTTPMethod m) -> bool {
    return strncmp(str.c_str(), method, len) == 0 && str[len] == ' ';
};
if (check("GET", 3, HTTPMethod::GET)) ...
```
Or use an exact `std::string` comparison against the token split by the caller.

---

## 2. Bare `\n` line ending not recognised — headers never complete, connection stalls
**`libs/http/HttpMessage.cpp:299–300`**

```cpp
cnt = getLine(buffer, bufsize, line);
if (cnt == 0 || line.empty()) break;   // bare \n → line is empty → break, foundTerminator never set
```

`getLine` pushes characters until it hits `\n`, but does not push `\n` itself.
A line terminated by bare `\n` (no `\r`) returns an empty `out` immediately.
The `line.empty()` check treats this as "no data" and breaks without setting
`foundTerminator`, so `_finishParseHeaders` stays false forever and the
connection stalls waiting for more data.

RFC 7230 requires `\r\n` but recommends servers tolerate bare `\n`. curl and
many HTTP libraries send only `\r\n` so this rarely fires in practice, but any
non-conforming client or a request piped through `nc` triggers it.

**Fix:** strip a trailing `\r` from `out` inside `getLine`, and treat an
all-whitespace line (after trim) as the terminator regardless of how many
bytes were consumed:
```cpp
// after pushing chars, before returning:
if (!out.empty() && out.back() == '\r') out.pop_back();
```
Then the existing `trimLine.empty()` check at line 306 correctly fires for
both `\r\n` and bare `\n` terminators.

---

## 3. `HTTPResponse::resetData()` sets `_httpCode` to `CODE_100`
**`libs/http/HttpMessage.cpp:521`**

```cpp
void HTTPResponse::resetData() {
    ...
    _httpCode = HTTPCode::CODE_100;   // wrong default for a recycled/fresh response
```

After a response is recycled (keep-alive or connection pool reuse), `_httpCode`
is `100 Continue` until a handler explicitly sets it. If a handler forgets to
call `http_code()`, the client receives `HTTP/1.1 100 Continue` as the final
response to a normal request. The correct reset default is `CODE_200`.

**Fix:**
```cpp
_httpCode = HTTPCode::CODE_200;
```

---

## 4. `100-continue` sent via unchecked blocking `::send` on non-blocking socket
**`libs/http/SimpleServer.cpp:258`**

```cpp
::send(fd, tmpEvent->res->get_buf()->rd_pos(),
       tmpEvent->res->get_buf()->rd_avail(), MSG_NOSIGNAL);
// return value not checked
```

The connection socket is `SOCK_NONBLOCK`. If the kernel send buffer is full,
`::send` returns -1 with `EAGAIN` and the `100 Continue` response is silently
dropped. The client then stalls indefinitely waiting for `100 Continue` before
sending the request body, and the server waits for a body that never comes.

**Fix:** buffer the `100 Continue` bytes and send them through the normal
`EPOLLOUT` path, or at minimum check the return value and retry:
```cpp
auto rv = ::send(fd, buf, len, MSG_NOSIGNAL);
if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    // queue for EPOLLOUT — simplest: prepend to response buffer
}
```
