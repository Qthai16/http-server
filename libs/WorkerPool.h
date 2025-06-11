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

// todo: able to limit pending jobs, sleep if pending jobs is at max

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
            : handler(std::move(handle)), requestStop(false), completeAll(true), onWorkCnt{0}, nDecreaseWorker(0) {
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
            workerCv.notify_all();
            if (!workers.empty()) {
                completeAll = completeAllJob;
                requestStop = true;
                cv.notify_all();
                for (auto i = 0; i < workers.size(); i++) {
                    auto &p = workers[i];
                    if (p) {
                        p->join();
                        delete p;
                        printf("[stop] workers %d stopped\n", i);
                    }
                }
                workers.clear();
            }
        }

        bool push(T j) {
            std::unique_lock<std::mutex> lock(mtx);
            if (expr_unlikely(requestStop))
                return false;
            jobqueue.push(std::move(j));
            if (onWorkCnt.load(std::memory_order_acquire) < nWorker) {// some worker is idle, notify it
                lock.unlock();
                cv.notify_one();
            }
            return true;
            // // always notify
            // lock.unlock();
            // cv.notify_one();
            // return true;
        }

        size_t pendingJobs() const {
            std::unique_lock<std::mutex> lock(mtx);
            return jobqueue.size();
        }

        size_t totalJobs() const {
            std::unique_lock<std::mutex> lock(mtx);
            return jobqueue.size() + onWorkCnt.load(std::memory_order_acquire);
        }

        size_t workerSize() const {
            std::unique_lock<std::mutex> lock(mtx);
            return nWorker;
        }

        void setWorkerSize(int n) {
            std::unique_lock<std::mutex> lock(mtx);
            if (n == nWorker) return;
            if (n > nWorker) {
                addWorkers(n);
            } else {
                nDecreaseWorker = nWorker - n;
                auto decCnt = nDecreaseWorker;
                lock.unlock();
                cv.notify_all();
                removeWorkers(n, decCnt);
            }
        }

        void modifyJobQueue(const std::function<void(NotifyQueueWorker::container_type &)> &cb) {
            std::lock_guard<std::mutex> lock(mtx);
            while (onWorkCnt.load(std::memory_order_acquire) > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            assert(onWorkCnt.load(std::memory_order_acquire) == 0);
            cb(jobqueue);
        }

    private:
        void removeWorkerAt(int idx) {
            // caller always lock
            if (requestStop) return;
            assert(workers[idx]);
            workers[idx]->join();
            delete workers[idx];
            workers[idx] = nullptr;
            --nWorker;
            printf("[removeWorkerAt] workers %d stopped\n", idx);
        }

        void removeWorkers(int newNWorker, int decCnt) {
            std::unique_lock<std::mutex> lock(mtx);
            if (requestStop || nWorker == newNWorker) return;
            workerCv.wait(lock, [&]() {
                return requestStop || nDecreaseWorker == 0;
            });
            if (requestStop) return;
            // printf("%d, %zu, %d\n", decCnt, deadWorkerIds.size(), nDecreaseWorker);
            assert(decCnt == deadWorkerIds.size() && nDecreaseWorker == 0);
            // cleanup dead workers
            for (const auto &idx: deadWorkerIds) {
                removeWorkerAt(idx);
            }
            deadWorkerIds.clear();
            assert(nWorker == newNWorker);
        }

        void addWorkers(int newNWorker) {
            // caller always lock
            int k = newNWorker - nWorker;
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
            nWorker = newNWorker;
        }

        void run(int idx) {
            printf("[run] worker %d running\n", idx);
            do {
                std::unique_lock<std::mutex> lock(mtx);
                if (expr_unlikely(nDecreaseWorker > 0)) {
                    --nDecreaseWorker;
                    deadWorkerIds.push_back(idx);
                    if (nDecreaseWorker == 0) {
                        lock.unlock();
                        workerCv.notify_all();
                    }
                    return;
                }
                if (jobqueue.size()) {
                    T j = std::move(jobqueue.front());
                    jobqueue.pop();
                    onWorkCnt.fetch_add(1, std::memory_order_acq_rel);// increase in lock
                    lock.unlock();
                    handler(j);
                    onWorkCnt.fetch_sub(1, std::memory_order_acq_rel);
                    continue;
                }
                cv.wait(lock, [&] {
                    return jobqueue.size() || requestStop || nDecreaseWorker;
                });
                if (expr_unlikely(requestStop)) break;
                if (expr_unlikely(nDecreaseWorker > 0)) {
                    --nDecreaseWorker;
                    deadWorkerIds.push_back(idx);
                    if (nDecreaseWorker == 0) {
                        lock.unlock();
                        workerCv.notify_all();
                    }
                    return;
                }
                if (jobqueue.empty()) continue;
                T j = std::move(jobqueue.front());
                jobqueue.pop();
                onWorkCnt.fetch_add(1, std::memory_order_acq_rel);
                lock.unlock();
                handler(j);
                onWorkCnt.fetch_sub(1, std::memory_order_acq_rel);
            } while (expr_likely(!requestStop));

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
        std::condition_variable workerCv;// synchronize worker changes
        mutable std::mutex mtx;
        std::vector<std::thread *> workers;
        std::vector<int> deadWorkerIds;
        std::atomic<int64_t> onWorkCnt;
        int nDecreaseWorker;
        int nWorker;
        bool completeAll;
        bool requestStop;
    };
}// namespace libs

#endif// LIBS_WORKERPOOL_H
