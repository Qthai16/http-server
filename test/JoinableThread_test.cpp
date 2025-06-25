#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <future>

#include <gtest/gtest.h>

#include "libs/JoinableThread.h"
#include "libs/JoinableThread_2.h"

using namespace std::chrono_literals;

TEST(joinable_thread, test_join) {
    std::mutex mtx;
    std::condition_variable cv;
    auto i = 0;
    std::shared_ptr<libs::JoinableThread> t1 = std::make_shared<libs::JoinableThread>([&](libs::JoinableThread *self) {
        printf("handler started\n");
        do {
            printf("tick tack: %d\n", i++);
            self->sleep(100ms);
            std::unique_lock<std::mutex> lock(mtx);
            if (i == 3) {
                lock.unlock();
                printf("send notify\n");
                cv.notify_one();
            } else {
                lock.unlock();
            }
        } while (!self->isStopped());
        printf("handler stopped\n");
    });
    // printf("t1 use count: %ld\n", t1.use_count());
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] {
        return i >= 3;
    });
    lock.unlock();
    // t1->stop();
    t1->stopAsync();
    ASSERT_TRUE(t1->isStopped());
}

TEST(joinable_thread, test_lifetime) {
    // lifetime of JoinableThread depend on outer thread
    auto i = 0;
    std::shared_ptr<libs::JoinableThread> t1;
    std::thread outer1([&]() {
        t1 = std::make_shared<libs::JoinableThread>([&](libs::JoinableThread *self) {
            printf("handler started\n");
            do {
                printf("tick tack: %d\n", i++);
                self->sleep(100ms);
            } while (!self->isStopped());
            printf("handler stopped\n");
        });
    });
    std::this_thread::sleep_for(320ms);
    outer1.join();
    printf("join outer thread\n");
    ASSERT_GE(i, 3);
}

TEST(joinable_thread, test_detach) {
    // thread set promise value must own the promise
    std::future<int> fut;
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    auto t1 = std::make_shared<libs::JoinableThread_2>([&](libs::JoinableThread_2::pointer_type self) {
        printf("handler started\n");
        std::promise<int> prom;
        fut = std::move(prom.get_future());
        printf("wait lock\n");
        std::unique_lock<std::mutex> lock(mtx);
        ready = true;
        printf("got lock\n");
        lock.unlock();
        cv.notify_one();
        auto i = 0;
        do {
            printf("tick tack: %d, use count: %ld\n", i++, self.use_count());
            self->sleep(100ms);
            if (i == 10) {
                prom.set_value(i);
                break;
            }
        } while (!self->isStopped());
        printf("handler stopped\n");
    });
    t1->start();
    t1->detach();
    t1.reset();
    std::unique_lock<std::mutex> lock(mtx);
    printf("wait future ready\n");
    cv.wait(lock, [&](){ return ready; });
    lock.unlock();
    printf("wait future get\n");
    ASSERT_EQ(fut.get(), 10);
}