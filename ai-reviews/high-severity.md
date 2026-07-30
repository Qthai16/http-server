# High Severity Bugs

---

## 1. Unbounded body growth — OOM on oversized `Content-Length`
**`libs/http/HttpMessage.cpp:343–348`**

```cpp
if (_finishParseHeaders) {
    auto len = recv_buf_.rd_avail();
    body_buf_.write(recv_buf_.rd_pos(), len);  // no cap vs _contentLength
    recv_buf_.incRdPos(len);
    _totalRead += len;
}
```

`body_buf_` grows without limit relative to `_contentLength`. A client claiming
`Content-Length: 4294967295` causes unbounded heap allocation until OOM or
`bad_alloc` crashes the server.

**Fix:** cap `len` to the remaining expected body bytes before writing:
```cpp
auto remaining = _contentLength - (_totalRead - _headerSize);
auto len = std::min(recv_buf_.rd_avail(), remaining);
```
Also reject requests upfront when `_contentLength` exceeds a configured max
(e.g. `if (_contentLength > MAX_BODY_SIZE) throw …` in `parse_headers` after
setting `_contentLength`).

---

## 2. `cleanupEvent` leaks fd and `ConnData` when `delete_fd` throws
**`libs/http/SimpleServer.cpp:313–324`**

```cpp
void IOWorker::cleanupEvent(EventBase *event) {
    auto connData = dynamic_cast<ConnData *>(event);
    handle_.delete_fd(event->_fd);   // throws std::runtime_error on epoll_ctl failure
    ::close(event->_fd);             // never reached on throw → fd leak
    if (connData) {
        stat_.decActiveConn();
        server_->pushCacheConn(connData);  // never reached → ConnData leak
        return;
    }
    delete event;
}
```

`EpollHandle::delete_fd` throws if `epoll_ctl(EPOLL_CTL_DEL)` returns -1, which
can happen under fd pressure. The throw unwinds past `::close` and
`pushCacheConn`, leaving an unclosed fd and a live `ConnData` with no owner.
Under load this silently exhausts the fd table.

**Fix:** log and continue on `epoll_ctl` failure — never throw in a cleanup path:
```cpp
void EpollHandle::delete_fd(int clientFd) {
    if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1)
        fprintf(stderr, "epoll_ctl DEL failed for fd %d: %s\n", clientFd, strerror(errno));
    // always proceed — caller must close fd and free ConnData
}
```

---

## 3. `modifyJobQueue` spins while holding `mtx`, blocking all workers
**`libs/WorkerPool.h:118–125`**

```cpp
void modifyJobQueue(const std::function<void(container_type &)> &cb) {
    std::lock_guard<std::mutex> lock(mtx);      // holds mtx for entire duration
    while (onWorkCnt.load(...) > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));  // spins here
    }
    cb(jobqueue);
}
```

While `modifyJobQueue` holds `mtx` and spins waiting for in-flight jobs to
finish, worker threads that complete their current job cannot re-acquire `mtx`
to pick up the next one. The pool stops processing new work for the entire
spin duration. Under high throughput this can stall the handler pool for tens
of milliseconds.

**Fix:** release `mtx` during the drain wait using a second condition variable
that workers signal when `onWorkCnt` transitions to zero:
```cpp
void modifyJobQueue(...) {
    std::unique_lock<std::mutex> lock(mtx);
    drainCv.wait(lock, [&]{ return onWorkCnt.load() == 0; });
    cb(jobqueue);
}
// in run(): after onWorkCnt.fetch_sub, if result == 0, drainCv.notify_all()
```
