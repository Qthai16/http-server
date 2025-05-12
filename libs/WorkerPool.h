/*
 * File:   WorkerPool.h
 * Author: thaipq
 *
 * Created on Mon Apr 28 2025 2:37:53 PM
 */

#ifndef LIBS_WORKERPOOL_H
#define LIBS_WORKERPOOL_H

#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>

#include "Defines.h"

namespace libs {
    template<class T, class TQueue = std::queue<T>>
    class NotifyQueueWorker {
    public:
        using job_type = T;
        using container_type = TQueue;

    public:
        NotifyQueueWorker() = delete;
        NotifyQueueWorker(const NotifyQueueWorker &) = delete;
        NotifyQueueWorker &operator=(const NotifyQueueWorker &) = delete;
        NotifyQueueWorker(NotifyQueueWorker &&) = delete;
        NotifyQueueWorker &operator=(NotifyQueueWorker &&) = delete;

        explicit NotifyQueueWorker(size_t nworker, std::function<void(T &)> handle)
            : handler(std::move(handle)), requeststop(false), completeAll(true), onWorkCnt{0}, nDecreaseWorker(0) {
            // if (nworker < 1) nworker = 1;
            workers.resize(nworker);
            nWorker = nworker;
            for (int i = 0; i < nWorker; ++i) {
                workers[i] = new std::thread(&NotifyQueueWorker::run, this, i);
            }
        }

        ~NotifyQueueWorker() {
            stop();
        }

        void stop(bool completeAllJob = true) {
            if (!workers.empty()) {
                completeAll = completeAllJob;
                requeststop = true;
                cv.notify_all();
                for (auto i = 0; i < workers.size(); i++) {
                    auto& p = workers[i];
                    if (p) {
                        p->join();
                        delete p;
                        printf("stoped workers %d\n", i);
                    }
                }
                workers.clear();
            }
        }

        bool isJobPending() {
            std::lock_guard<std::mutex> guard(mtx);
            return (jobqueue.size() > 0 || onWorkCnt.load(std::memory_order_acquire) > 0);
        }

        bool push(T j) {
            std::unique_lock<std::mutex> lock(mtx);
            if (expr_unlikely(requeststop))
                return false;
            jobqueue.push(std::move(j));
            lock.unlock();
            cv.notify_one();
            return true;
        }

        size_t pendingJobs() const {
            std::unique_lock<std::mutex> lock(mtx);
            return jobqueue.size();
        }

        size_t jobSize() const {
            std::unique_lock<std::mutex> lock(mtx);
            return jobqueue.size() + onWorkCnt.load(std::memory_order_acquire);
        }

        bool empty() const {
            std::unique_lock<std::mutex> lock(mtx);
            return jobqueue.empty();
        }

        size_t workerSize() const {
            std::unique_lock<std::mutex> lock(mtx);
            return nWorker;
        }

        void asyncSetWorkerSize(int n) {
            std::unique_lock<std::mutex> lock(mtx);
            if (n == nWorker) return;
            if (n > nWorker) {
                int k = n - nWorker;
                for (int i = 0; i < workers.size(); ++i) {
                    if (workers[i] == nullptr) {
                        workers[i] = new std::thread(&NotifyQueueWorker::run, this, i);
                        --k;
                    }
                    if (k == 0) break;
                }
                if (k > 0) {
                    int i = workers.size();
                    for (int j = 0; j < k; ++j) {
                        workers.push_back(new std::thread(&NotifyQueueWorker::run, this, i + j));
                    }
                }
                nWorker = n;
            } else {
                nDecreaseWorker = nWorker - n;
                lock.unlock();
                cv.notify_all();
            }
        }

        void setWorkerSize(int n) {
            std::unique_lock<std::mutex> lock(mtx);
            if (n == nWorker) return;
            if (n > nWorker) {
                int k = n - nWorker;
                for (int i = 0; i < workers.size(); ++i) {
                    if (workers[i] == nullptr) {
                        workers[i] = new std::thread(&NotifyQueueWorker::run, this, i);
                        --k;
                    }
                    if (k == 0) break;
                }
                if (k > 0) {
                    int i = workers.size();
                    for (int j = 0; j < k; ++j) {
                        workers.push_back(new std::thread(&NotifyQueueWorker::run, this, i + j));
                    }
                }
                nWorker = n;
            } else {
                nDecreaseWorker = nWorker - n;
                lock.unlock();
                cv.notify_all();
                lock.lock();
                if (requeststop || nWorker == n) return;
                cv.wait(lock, [&, n] {
                    return requeststop || nWorker == n;
                });
                if (!requeststop) {
                    assert(nWorker == n && nDecreaseWorker == 0);
                }
            }
        }

        void modifyJobQueue(const std::function<void(NotifyQueueWorker::container_type &)> &cb) {
            std::lock_guard<std::mutex> lock(mtx);
            while (onWorkCnt.load(std::memory_order_acquire) > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            cb(jobqueue);
        }

    private:
        void delWorkerAt(int idx) {
            std::unique_lock<std::mutex> lock(mtx);
            if (requeststop) return;
            assert(workers[idx] != nullptr);
            workers[idx]->join();
            delete workers[idx];
            workers[idx] = nullptr;
            --nWorker;
            lock.unlock();
            cv.notify_all();
        }

        void run(int idx) {
            do {
                std::unique_lock<std::mutex> lock(mtx);
                if (expr_unlikely(nDecreaseWorker > 0)) {
                    --nDecreaseWorker;
                    std::thread(&NotifyQueueWorker::delWorkerAt, this, idx).detach();
                    lock.unlock();
                    return;
                }
                if (jobqueue.size()) {
                    T j = std::move(jobqueue.front());// todo: T&& ?
                    jobqueue.pop();
                    onWorkCnt.fetch_add(1, std::memory_order_acq_rel);
                    lock.unlock();
                    handler(j);
                    onWorkCnt.fetch_sub(1, std::memory_order_acq_rel);
                    continue;
                }
                cv.wait(lock, [&] {
                    return jobqueue.size() || requeststop || nDecreaseWorker;
                });
                if (expr_unlikely(requeststop)) break;
                if (expr_unlikely(nDecreaseWorker > 0)) {
                    --nDecreaseWorker;
                    std::thread(&NotifyQueueWorker::delWorkerAt, this, idx).detach();
                    lock.unlock();
                    return;
                }
                if (jobqueue.empty()) continue;
                T j = std::move(jobqueue.front());
                jobqueue.pop();
                onWorkCnt.fetch_add(1, std::memory_order_acq_rel);
                lock.unlock();
                handler(j);
                onWorkCnt.fetch_sub(1, std::memory_order_acq_rel);
            } while (expr_likely(!requeststop));

            if (completeAll) {
                do {
                    std::unique_lock<std::mutex> lock(mtx);
                    if (jobqueue.size()) {
                        T j = std::move(jobqueue.front());
                        jobqueue.pop();
                        onWorkCnt.fetch_add(1, std::memory_order_acq_rel);
                        lock.unlock();
                        handler(j);
                        onWorkCnt.fetch_sub(1, std::memory_order_acq_rel);
                        continue;
                    }
                    return;
                } while (true);
            }
        }

    private:
        TQueue jobqueue;
        std::function<void(T &)> handler;
        std::condition_variable cv;
        mutable std::mutex mtx;
        std::vector<std::thread *> workers;
        std::atomic<int64_t> onWorkCnt;
        int nDecreaseWorker;
        int nWorker;
        bool requeststop;
        bool completeAll;
    };
}// namespace libs

#endif// LIBS_WORKERPOOL_H
