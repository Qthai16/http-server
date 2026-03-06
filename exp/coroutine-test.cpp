#include <iostream>
#include <string>
#include <coroutine>
#include <utility>// For std::exchange

#include <list>
#include <atomic>
#include <coroutine>
#include <thread>
#include <vector>
#include <chrono>
#include <condition_variable>

using namespace std::chrono_literals;

class ManualSetCVEvent {
public:
    ManualSetCVEvent(bool initState = false) : mtx_(), cv_(), state_{initState} {}

    void set() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            state_.exchange(true, std::memory_order::release);
        }
        cv_.notify_all();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return state_.load(std::memory_order::acquire); });
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.exchange(false, std::memory_order::release);
    }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool state_;
};


class ManualSetEvent {
public:
    ManualSetEvent(bool initState = false) : waiters_(), state_{initState} {}

    void set() {
        // set the state if previous is false
        if (!state_.exchange(true, std::memory_order::release)) {
            for (auto &h: waiters_) {// resume all waiting
                h.resume();
            }
            waiters_.clear();
        }
    }

    void reset() {
        state_.store(false, std::memory_order::release);
    }

    bool is_set() const noexcept {
        return state_.load(std::memory_order::acquire);
    }

    auto operator co_await() noexcept {
        struct Awaiter {
            ManualSetEvent &event_;
            std::coroutine_handle<> handle_;

            bool await_ready() const noexcept {
                return event_.is_set();
            }

            void await_suspend(std::coroutine_handle<> h) {
                // not thread safe
                handle_ = h;
                event_.waiters_.push_back(h);
            }

            void await_resume() const noexcept {
                // no return value
            }
        };
        return Awaiter{*this, {}};
    }

private:
    std::list<std::coroutine_handle<>> waiters_;
    std::atomic_bool state_;// set should set this state
};

struct Task {
    struct promise_type;
    using coro_handle_type = std::coroutine_handle<promise_type>;
    struct promise_type {
        Task get_return_object() {// construct task from promise type (task is the return value when call coroutine)
            return Task{coro_handle_type::from_promise(*this)};
        }

        auto initial_suspend() {
            // start of the coroutine (before user code)
            return std::suspend_always{};
        }
        auto final_suspend() noexcept {
            // before destroying the coroutine
            return std::suspend_always{};
        }

        void return_void() {}// coroutine call promise.return_void() when co_return; is used
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> h_;

    explicit Task(std::coroutine_handle<promise_type> h) : h_(h) {}

    ~Task() {
        if (h_) {
            h_.destroy();
        }
    }

    // Move constructor and assignment
    Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (h_) {
                h_.destroy();
            }
            h_ = std::exchange(other.h_, {});
        }
        return *this;
    }

    // Delete copy constructor and assignment
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    // Resume the coroutine
    void resume() {
        if (h_ && !h_.done()) {
            h_.resume();
        }
    }

    // Check if coroutine is done
    bool done() const {
        return h_.done();
    }
};

Task waitForEvent(ManualSetEvent &event, int i) {
    std::cout << "start coroutine " << i << std::endl;
    co_await event;
    std::cout << "event set " << i << std::endl;
}

void blockingWait(ManualSetCVEvent &cvEvent, int i) {
    std::cout << "start wait " << i << std::endl;
    cvEvent.wait();
    std::cout << "event set " << i << std::endl;
}

int main(int argc, const char *argv[]) {
    ManualSetEvent event(false);
    std::vector<Task> tasks;
    for (auto i = 0; i < 5; ++i) {
        tasks.push_back(waitForEvent(event, i));
    }
    for (auto& t : tasks) {
        t.resume();
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    event.set();

    // ManualSetCVEvent cvEvent(false);
    // std::vector<std::thread> workers;
    // for (auto i = 0; i < 5; i++) {
    //     workers.emplace_back([&cvEvent](int i) {
    //         blockingWait(cvEvent, i);
    //     },
    //                          i);
    // }
    // std::this_thread::sleep_for(1s);
    // cvEvent.set();
    // for (auto &t: workers) {
    //     t.join();
    // }
    // workers.clear();
    return 0;
}

// Task simple_coroutine() {
//     std::cout << "Coroutine started" << std::endl;

//     std::cout << "Coroutine prepare to suspend (first time)" << std::endl;
//     co_await std::suspend_always();

//     std::cout << "Coroutine resumed! Now suspending again..." << std::endl;
//     co_await std::suspend_always();

//     std::cout << "Coroutine resumed again! Finishing..." << std::endl;
//     co_return;
// }

// int main(int argc, const char *argv[]) {
//     std::cout << "Creating coroutine..." << std::endl;

//     auto task = simple_coroutine();

//     std::cout << "Coroutine created. Starting execution..." << std::endl;
//     task.resume();// Start the coroutine

//     std::cout << "Main: First resume done. Calling resume again..." << std::endl;
//     task.resume();// Resume from first suspension

//     std::cout << "Main: Second resume done. Calling resume one more time..." << std::endl;
//     task.resume();// Resume from second suspension

//     std::cout << "Main: All done. Is coroutine finished? " << (task.done() ? "Yes" : "No") << std::endl;

//     return 0;
// }
