/*
 * File:   JoinableThread_2.h
 * Author: thaipq
 *
 * Created on Mon Jun 09 2025 4:07:22 PM
 */

#ifndef LIBS_JOINABLETHREAD_2_H
#define LIBS_JOINABLETHREAD_2_H

#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>

#include "Defines.h"

namespace libs {
    class JoinableThread_2 : public std::enable_shared_from_this<JoinableThread_2> {
    public:
        typedef std::shared_ptr<JoinableThread_2> pointer_type;

    public:
        JoinableThread_2() = delete;
        JoinableThread_2(const JoinableThread_2 &) = delete;
        JoinableThread_2 &operator=(const JoinableThread_2 &) = delete;
        JoinableThread_2(JoinableThread_2 &&) = delete;
        JoinableThread_2 &operator=(JoinableThread_2 &&) = delete;

        explicit JoinableThread_2(std::function<void(JoinableThread_2::pointer_type)> runable)
            : mtx(), cv(), handler(std::move(runable)),
              th(nullptr),
              reqWakeUp(false), reqStop(false) {
        }

        std::shared_ptr<JoinableThread_2> getPtr() {
            return shared_from_this();
        }

        void start() {
            std::shared_ptr<JoinableThread_2> selfRef = shared_from_this();
            th = std::make_unique<std::thread>(&JoinableThread_2::run, selfRef);
        }

        ~JoinableThread_2() {
            stop();
        }

        void stop() {
            printf("[JoinableThread_2] stopping\n");
            reqStop = true;
            cv.notify_all();
            if (th.get()) {
                if (th->joinable()) th->join();
                th.reset();
            }
        }

        void detach() {
            if (th.get())
                th->detach();
        }

        template<typename Rep, typename Period>
        void sleep(const std::chrono::duration<Rep, Period> &d) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, d, [&] {
                return reqStop || reqWakeUp;
            });
            if (expr_unlikely(reqWakeUp)) reqWakeUp = false;
        }

        bool isStopped() {
            return reqStop.load(std::memory_order_acquire);
        }

        void stopAsync() {
            if (!reqStop.exchange(true, std::memory_order_acq_rel))
                cv.notify_all();
        }

        void wakeupAsync() {
            if (!reqWakeUp.exchange(true, std::memory_order_acq_rel))
                cv.notify_one();
        }

    private:
        static void run(std::shared_ptr<JoinableThread_2> self) {
            self->getHandler()(self);
        }

        std::function<void(JoinableThread_2::pointer_type)> getHandler() const {
            return handler;
        }

    private:
        std::mutex mtx;
        std::condition_variable cv;
        std::function<void(JoinableThread_2::pointer_type)> handler;
        std::unique_ptr<std::thread> th;
        std::atomic_bool reqWakeUp;
        std::atomic_bool reqStop;
    };
}// namespace libs

#endif// LIBS_JOINABLETHREAD_2_H
