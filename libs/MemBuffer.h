/*
 * File:   MemBuf.h
 * Author: thaipq
 *
 * Created on Wed Jul 02 2025 10:12:58 PM
 */

#ifndef LIBS_MEMBUFFER_H
#define LIBS_MEMBUFFER_H

#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace libs {
    class MemBuf {
    public:
        enum class Ownership {
            VIEW,
            OWN,
        };

    public:
        MemBuf() : buf_(nullptr), wrPos_(0), rdPos_(0), cap_(0), ownType_(Ownership::VIEW) {}
        explicit MemBuf(std::size_t size) : buf_(nullptr), wrPos_(0), rdPos_(0), cap_(size), ownType_(Ownership::OWN) {
            buf_ = (char *) malloc(size);
        }

        ~MemBuf() {
            cleanup();
        }

        void cleanup() {
            if (ownType_ == Ownership::OWN) {
                free(buf_);
            }
            buf_ = nullptr;
            cap_ = 0;
        }

        bool empty() const {
            return rdPos_ == wrPos_;
        }

        bool full() const {
            return wrPos_ == cap_;
        }

        size_t write(const char *data, size_t len) {
            if (data == nullptr || len == 0)
                return 0;
            if (wrPos_ + len >= cap_) {
                auto newsize = cap_ * 2;
                auto newbuf = (char *) realloc(buf_, newsize);
                buf_ = newbuf;
                cap_ = newsize;
            }
            memcpy(buf_ + wrPos_, data, len);
            wrPos_ += len;
            return len;
        }

        size_t read(char *outbuf, size_t len) {
            if (outbuf == nullptr || len == 0)
                return 0;
            if (rdPos_ >= wrPos_)
                return 0;
            if (len > wrPos_ - rdPos_)
                len = wrPos_ - rdPos_;
            memcpy(outbuf, buf_ + rdPos_, len);
            rdPos_ += len;
            return len;
        }

        char* wr_pos() {
            return buf_ + wrPos_;
        }

        char* rd_pos() {
            return buf_ + rdPos_;
        }

        void incWrPos(size_t cnt) {
            if (wrPos_ + cnt >= cap_) {
                wrPos_ = cap_;
                return;
            }
            wrPos_ += cnt;
        }

        void incRdPos(size_t cnt) {
            if (rdPos_ + cnt >= wrPos_) {
                rdPos_ = wrPos_;
                return;
            }
            rdPos_ += cnt;
        }

        void get_view(char **data, size_t *len) {
            *data = buf_ + rdPos_;
            *len = wrPos_ - rdPos_;
        }

        size_t size() const {
            return wrPos_ - rdPos_;
        }

        size_t cap() const {
            return cap_;
        }

        void resetBuf() {
            wrPos_ = 0;
            rdPos_ = 0;
        }

        void resetBuf(char *outbuf, size_t len) {// view only
            cleanup();
            buf_ = outbuf;
            cap_ = len;
            wrPos_ = 0;
            rdPos_ = 0;
            ownType_ = Ownership::VIEW;
        }
        // todo: array of buffer. append view

    protected:
        char *buf_;
        size_t wrPos_;
        size_t rdPos_;
        size_t cap_;
        Ownership ownType_;
    };

    // FixMemBuf use a fixed size allocated memory when write, and wrap over
    // class FixMemBuf : public MemBuf {
    //     FixMemBuf() : MemBuf() {}
    //     explicit FixMemBuf(std::size_t size) : MemBuf(size) {}
    //     ~FixMemBuf() override {}

    //     size_t write(const char *data, size_t len) override {
    //         if (data == nullptr || len == 0)
    //             return 0;
    //         if (wrPos_ + len >= size_) {
    //             auto last = size_ - wrPos_;
                
    //         }
    //         memcpy(buf_ + wrPos_, data, len);
    //         wrPos_ += len;
    //         return len;
    //     }

    //     size_t read(char *outbuf, size_t len) override {
            
    //     }
    // };

    // AutoMemBuf auto allocate more memory when write, up to max buffer size
    // class AutoMemBuf : public MemBuf {
    // public:
    //     AutoMemBuf() : MemBuf() {}
    //     explicit AutoMemBuf(std::size_t size) : MemBuf(size) {}
    //     ~AutoMemBuf() override {}
    // public:
    //     bool empty() const override {
    //         return rdPos_ == wrPos_;
    //     }

    //     bool full() const override {
    //         return wrPos_ - rdPos_ == size_;
    //     }

    //     size_t write(const char *data, size_t len) override {
    //         if (data == nullptr || len == 0)
    //             return 0;
    //         if (wrPos_ + len >= size_) {
    //             auto newsize = size_ * 2;
    //             auto newbuf = (char *) realloc(buf_, newsize);
    //             buf_ = newbuf;
    //             size_ = newsize;
    //         }
    //         memcpy(buf_ + wrPos_, data, len);
    //         wrPos_ += len;
    //         return len;
    //     }

    //     size_t read(char *outbuf, size_t len) override {
    //         if (outbuf == nullptr || len == 0)
    //             return 0;
    //         if (rdPos_ >= wrPos_)
    //             return 0;
    //         if (len > wrPos_ - rdPos_)
    //             len = wrPos_ - rdPos_;
    //         memcpy(outbuf, buf_ + rdPos_, len);
    //         rdPos_ += len;
    //         return len;
    //     }
    // };
}// namespace libs


#endif // LIBS_MEMBUFFER_H
