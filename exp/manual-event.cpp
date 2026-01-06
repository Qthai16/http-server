#include <iostream>
#include <coroutine>
#include <list>
#include <utility>

// Awaitable type that suspends and resumes coroutines manually
class ManualEvent {
public:
    struct Awaiter {
        ManualEvent& event_;
        std::coroutine_handle<> handle_;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            handle_ = h;
            event_.waiters_.push_back(h);
        }
        void await_resume() const noexcept {}
    };

    Awaiter operator co_await() noexcept {
        return Awaiter{*this, {}};
    }

    void set() {
        // Resume all waiting coroutines
        for (auto h : waiters_) {
            if (h) h.resume();
        }
        waiters_.clear();
    }

private:
    std::list<std::coroutine_handle<>> waiters_;
};

// Simple coroutine task type
struct Task {
    struct promise_type {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> h_;
    explicit Task(std::coroutine_handle<promise_type> h) : h_(h) {}
    ~Task() { if (h_) h_.destroy(); }
    Task(Task&& other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = std::exchange(other.h_, {});
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    void resume() { if (h_ && !h_.done()) h_.resume(); }
    bool done() const { return h_.done(); }
};

// Example coroutine using ManualEvent
Task my_coroutine(ManualEvent& event, int id) {
    std::cout << "Coroutine " << id << " started, suspending...\n";
    co_await event; // Suspends here, handle stored in event
    std::cout << "Coroutine " << id << " resumed!\n";
    co_return;
}

int main() {
    ManualEvent event;

    auto t1 = my_coroutine(event, 1);
    auto t2 = my_coroutine(event, 2);

    std::cout << "Main: coroutines created and suspended.\n";
    // Resume all coroutines waiting on event
    event.set();

    std::cout << "Main: all coroutines resumed.\n";
    return 0;
}
