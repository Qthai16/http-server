/*
 * File:   JoinableThread.h
 * Author: thaipq
 *
 * Created on Tue May 13 2025 2:32:40 PM
 */

#ifndef LIBS_JOINABLETHREAD_H
#define LIBS_JOINABLETHREAD_H

#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>

#include "Defines.h"

namespace libs {

    class JoinableThread {
    public:
        JoinableThread() = delete;
        JoinableThread(const JoinableThread &) = delete;
        JoinableThread &operator=(const JoinableThread &) = delete;
        JoinableThread(JoinableThread &&) = delete;
        JoinableThread &operator=(JoinableThread &&) = delete;

        explicit JoinableThread(std::function<void(JoinableThread *)> run)
            : reqStop(false), handler(std::move(run)), reqWakeUp(false) {
            th.reset(new std::thread(&JoinableThread::run, this));
        }

        ~JoinableThread() {
            stop();
        }

        void stop() {
            printf("[JoinableThread] stopping\n");
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
        void run() {
            handler(this);
        }

    private:
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_ptr<std::thread> th;
        std::function<void(JoinableThread *)> handler;
        std::atomic_bool reqWakeUp;
        std::atomic_bool reqStop;
    };
}// namespace libs

#endif// LIBS_JOINABLETHREAD_H