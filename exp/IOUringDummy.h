#pragma once

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>

#include <liburing.h>
#include <stdexcept>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>
#include <atomic>
#include <condition_variable>

// todo: add an timeout entry to sqe, when timeout expired
// todo: accept on socket, connect socket
// todo: read/write on socket

// todo: use AND mask as power of two instead of mod by queue_depth
// todo: use vector<IOData> as a ring buffer

// use buffer group of IOData

#define COPY_CTOR_DELETE(classname)        \
    classname(const classname &) = delete; \
    classname &operator=(const classname &) = delete;
#define MOVE_CTOR_DELETE(classname)   \
    classname(classname &&) = delete; \
    classname &operator=(classname &&) = delete

namespace libs {
    enum IOType : uint8_t {
        UNINIT,
        READ,
        WRITE,
        PROV_BUF,
    };
    struct IOData {
        using CallbackFn = std::function<void(ssize_t)>;

        int fd{-1};
        off_t offset{0};
        struct iovec iov;
        IOType type{IOType::UNINIT};
        bool cleanup{false};
        CallbackFn cb{nullptr};
    };
    class IOUringDummy {
    public:
        IOUringDummy(unsigned int size) : qd_(size) {
            unsigned int flag = 0;
            auto ret = io_uring_queue_init(size, &ring, flag);
            if (ret < 0) {
                fprintf(stderr, "io_uring_queue_init failed: %s", strerror(-ret));
                throw std::runtime_error("io_uring_queue_init failed");
            }
            // ioDataVec_.resize(size);
        }
        COPY_CTOR_DELETE(IOUringDummy);
        MOVE_CTOR_DELETE(IOUringDummy);

        ~IOUringDummy() {
            // stop();
            // if (bgThread_) {
            //     bgThread_->join();
            // }
            io_uring_queue_exit(&ring);
        }

        bool check_supported(io_uring_op opcode) {
            struct io_uring_probe *probe;
            probe = io_uring_get_probe_ring(&ring);
            if (!probe)
                return false;
            auto supported = io_uring_opcode_supported(probe, opcode);
            io_uring_free_probe(probe);
            return supported;
        }

        void async_readv(int fd, off_t offset, void *buf, size_t size, IOData::CallbackFn cb = [](ssize_t){}) {
            auto data = new_read_io_data(fd, offset, buf, size);
            if (cb) data->cb = cb;
            prep_readv(data);
        }

        void async_writev(int fd, off_t offset, void *buf, size_t size, IOData::CallbackFn cb = [](ssize_t){}) {
            auto data = new_write_io_data(fd, offset, buf, size);
            if (cb) data->cb = cb;
            prep_writev(data);
        }

        int pendings() const {
            return pendingCnt_.load(std::memory_order_acquire);
        }

        size_t readv(int fd, off_t offset, void *buf, size_t size) {
            size_t ret = 0;
            async_readv(fd, offset, buf, size, [&ret](ssize_t res){ ret = res; });
            submit_and_wait();
            return ret;
        }

        size_t writev(int fd, off_t offset, void *buf, size_t size) {
            size_t ret = 0;
            async_writev(fd, offset, buf, size, [&ret](ssize_t res){ ret = res; });
            submit_and_wait();
            return ret;
        }

        // unsigned int waitnr = 1
        void submit_and_wait() {
            // todo: with timeout
            while (pendingCnt_.load()) {
                io_uring_submit_and_wait(&ring, 1);
                struct io_uring_cqe *cqe;
                auto ret = io_uring_wait_cqe(&ring, &cqe);
                if (ret < 0) {
                    fprintf(stderr, "wait cqe failed: %d\n", ret);
                    return;
                }
                auto data = (IOData *) io_uring_cqe_get_data(cqe);
                if (cqe->res < 0) {// error
                    if (cqe->res == -EAGAIN) {
                        prep_readv(data);
                        io_uring_cqe_seen(&ring, cqe);
                        continue;
                    }
                    goto cleanup; // other error
                } else if (cqe->res != data->iov.iov_len) {// short read/write, update and re-enqueue
                    data->iov.iov_base = (char *) data->iov.iov_base + cqe->res;
                    data->iov.iov_len -= cqe->res;
                    data->offset += cqe->res;
                    prep_readv(data);// prepare again
                    io_uring_cqe_seen(&ring, cqe);
                    continue;
                }
                cleanup:
                if (data->cb)
                    data->cb(cqe->res);
                if (data->cleanup) delete data;
                io_uring_cqe_seen(&ring, cqe);
                pendingCnt_.fetch_sub(1, std::memory_order_release);
            }
        }

        // int submit_all_and_wait() {
        //     // fprintf(stdout, "submit and wait: read: %d, write: %d\n", pendingRead_.load(), pendingWrite_.load());
        //     int readFailed{0}, writeFailed{0};
        //     while (pendingRead_.load(std::memory_order_acquire) || pendingWrite_.load(std::memory_order_acquire)) {
        //         io_uring_submit(&ring);
        //         struct io_uring_cqe *cqe;
        //         auto ret = io_uring_wait_cqe(&ring, &cqe);
        //         if (ret < 0) {
        //             fprintf(stderr, "wait_cqe failed: %d\n", ret);
        //             return ret;
        //         }
        //         auto data = (IOData *) io_uring_cqe_get_data(cqe);
        //         if (cqe->res < 0) {// error
        //             if (cqe->res == -EAGAIN) {
        //                 prep_readv(data);
        //                 io_uring_submit(&ring);
        //                 io_uring_cqe_seen(&ring, cqe);
        //                 continue;
        //             }
        //             fprintf(stderr, "cqe failed: %s\n", strerror(-cqe->res));
        //             if (data->type == IOType::READ) {
        //                 readFailed++;
        //             } else if (data->type == IOType::WRITE) {
        //                 writeFailed++;
        //             }
        //             goto cleanup;
        //         } else if (cqe->res != data->iov.iov_len) {// short read/write, update and re-enqueue
        //             data->iov.iov_base = (char *) data->iov.iov_base + cqe->res;
        //             data->iov.iov_len -= cqe->res;
        //             data->offset += cqe->res;
        //             prep_readv(data);// prepare again
        //             io_uring_submit(&ring);
        //             io_uring_cqe_seen(&ring, cqe);
        //             continue;
        //         }
        //     cleanup:
        //         if (data->type == IOType::READ) {
        //             pendingRead_.fetch_sub(1, std::memory_order_release);
        //         } else if (data->type == IOType::WRITE) {
        //             pendingWrite_.fetch_sub(1, std::memory_order_release);
        //         }
        //         if (data->cleanup) delete data;
        //         io_uring_cqe_seen(&ring, cqe);
        //     }
        //     return 0;
        //     // fprintf(stdout, "failed read/write: %d/%d\n", readFailed, writeFailed);
        // }

        // void stop(bool wait = true) {
        //     std::unique_lock lock(stopMtx_);
        //     if (!stop_.exchange(true, std::memory_order_release)) {
        //         if (!wait) return;
        //         stopCv_.wait(lock, [this] { return isStopped; });
        //     }
        // }

        // void run_event_loop(bool background = true) {
        //     if (background) {
        //         bgThread_.reset(new std::thread([this]() {
        //             event_loop();
        //         }));
        //     } else {
        //         event_loop();
        //     }
        // }

    private:
        IOData *new_io_data(IOType type, int fd, off_t off, void *buf, size_t size) {
            auto iodata = new IOData();
            iodata->cleanup = true;
            iodata->fd = fd;
            iodata->iov.iov_base = buf;
            iodata->iov.iov_len = size;
            iodata->offset = off;
            iodata->type = type;
            return iodata;
        }

        IOData *new_read_io_data(int fd, off_t off, void *buf, size_t size) {
            return new_io_data(IOType::READ, fd, off, buf, size);
        }

        IOData *new_write_io_data(int fd, off_t off, void *buf, size_t size) {
            return new_io_data(IOType::WRITE, fd, off, buf, size);
        }

        void prep_readv(IOData *data) {
            // data must remain valid until the request has been succesfully submitted
            struct io_uring_sqe *sqe;
            do {
                sqe = io_uring_get_sqe(&ring);
                if (sqe) break;
                submit_and_wait();
            } while (!sqe);
            io_uring_prep_readv(sqe, data->fd, &data->iov, 1, data->offset);
            io_uring_sqe_set_data(sqe, data);
            pendingCnt_.fetch_add(1, std::memory_order_release);
        }

        void prep_writev(IOData *data) {
            struct io_uring_sqe *sqe;
            do {
                sqe = io_uring_get_sqe(&ring);
                if (sqe) break;
                submit_and_wait();
            } while (!sqe);
            io_uring_prep_writev(sqe, data->fd, &data->iov, 1, data->offset);
            io_uring_sqe_set_data(sqe, data);
            pendingCnt_.fetch_add(1, std::memory_order_release);
        }

        // void event_loop() {
        //     int processed{0};
        //     struct io_uring_cqe *cqe;
        //     unsigned head;// head of the ring buffer, unused
        //     unsigned processCnt{0};
        //     int ret{0};
        //     while (!stop_.load(std::memory_order_acquire)) {
        //         io_uring_for_each_cqe(&ring, head, cqe) {
        //         }
        //         io_uring_cq_advance(&ring, processCnt);
        //     }
        //     // continue to drain the cqe
        //     while (true) {
        //         ret = io_uring_peek_cqe(&ring, &cqe);
        //         if (ret == -EAGAIN) {
        //             break;
        //         }
        //     }
        //     {
        //         std::lock_guard lock(stopMtx_);
        //         isStopped = true;
        //     }
        //     printf("processed events: %d\n", processCnt);
        //     stopCv_.notify_all();
        // }

    private:
        struct io_uring ring;
        const unsigned int qd_;
        std::atomic_int pendingCnt_;
        // std::unique_ptr<std::thread> bgThread_;
        // bool isStopped{false};
        // std::atomic_bool stop_{false};
        // std::atomic_int pendingRead_;
        // std::atomic_int pendingWrite_;
        // std::mutex stopMtx_;
        // std::condition_variable stopCv_;
        // std::vector<IOData> ioDataVec_; // should be ring buffer
        // int ioDataIdx_{0};
    };
}// namespace libs
