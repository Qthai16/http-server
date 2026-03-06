## Co-routine research
- a function is a coroutine if it contains one of following keywords:
  - co_return
  - co_await
  - co_yield

### coroutine_handle
- coroutine_handle manage the coroutine via methods: `resume`, `destroy`, `done`
- promise can be get via coroutine_handle
- a promise is passed to coroutine handle constructor to construct a coroutine handle
- coroutine handle is the value returned to the caller/resumer get when calling a coroutine. Some common operations are:
  - resume coroutine: `handle.resume()`
  - destroy coroutine (without resume it): `handle.destroy()`. Should be in dtor of RAII class
  - get a promise: `handle.promise()`
- `coroutine_handle<>` is a short-hand for `coroutine_handle<void>`

### Awaiter and Awaitable type
- `co_await`: to suspend the execution until resume
- `co_yield`: to suspend execution returning a value
- `co_return`: to complete execution returning a value
- A type that supports the `co_await` operator is called an Awaitable type.
- awaitable type determine the behaviour of `co_await` expression: what to be doned after coroutine suspended, after coroutine resumed, etc.
- A promise_type control the behavior of the coroutine, it may or may nor implement the `await_transform` method member, which changed the behaviour of the `co_await` expression
- an `awaitable_concept` must implement:
  - bool await_ready(): if true, continue the execution of coroutine, else suspend the coroutine
  - void await_suspend(coroutine_handle<>)
  - auto await_resume()

#### await_ready
- an optimization so that if the coroutine can continue to execute as sync method (return true), else its suspend and return back to caller

#### await_suspend
- `await_suspend` is the main logic for `co_await` expression
- `await_suspend` may return void, return bool or return a coroutine handle
  - if return void: unconditionally transfers execution back to the caller/resumer of the coroutine when the call to await_suspend() returns
  - if return bool: useful when the awaiter might start an async operation that can sometimes complete synchronously
    - return false: coroutine can continue the execution
    - return true:  suspend the coroutine and return to caller
  - if return coroutine handle: that handle is resumed by called to `handle.resume()`
- `await_suspend()` method receive a coroutine handle (awaiting coroutine) as argument. It means that multiple coroutine can concurrently `co_await` on the same condition to happen
- normally in a `await_suspend` function, it should only use local variables in `await_suspend`, and not use `this` to avoid use after free error

#### await resume
- `await_resume` use to return the value for `co_await` expresion. If no value is need, `await_resume` can return void
- await_resume is called before the coroutine resume, before the `Awaiter` object is destroyed

### Promise Type



### Minimal coroutine implementation
- Awaiter type: handle how `co_await` expresion does, implement: `await_ready`, `await_suspend`, `await_resume`
  - awaiter type should contain a member of list awaiting coroutine waiting for the operation completed
- Promise type: control coroutine creation, suspension, and return value (via get_return_object), implement: `initial_suspend`, `final_suspend`, `get_return_object`
- Wrapper type (Task): wrap std::coroutine_handle and promise_type, let caller control the coroutine lifetime (resume, destroy, check done)
