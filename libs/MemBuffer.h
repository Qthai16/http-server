/*
 * File:   MemBuf.h
 * Author: thaipq
 *
 * Created on Wed Jul 02 2025 10:12:58 PM
 */

#ifndef LIBS_MEMBUFFER_H
#define LIBS_MEMBUFFER_H

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

// todo: maxCap = 65KB, if max cap reach, create new MemBuf and link to it

namespace libs {
    class MemBuf {
    public:
        enum class Ownership {
            UNSET,
            VIEW,
            OWN,
        };

    public:
        MemBuf() : buf_(nullptr), wrPos_(0), rdPos_(0), cap_(0), ownType_(Ownership::UNSET) {}
        explicit MemBuf(std::size_t size) : buf_(nullptr), wrPos_(0), rdPos_(0), cap_(size), ownType_(Ownership::OWN) {
            buf_ = (char *) malloc(size);
        }

        ~MemBuf() {
            cleanup();
        }

        explicit operator std::string() const {
            // std::string conversion
            if (rd_avail() == 0)
                return {};
            return std::string(rd_pos(), rd_avail());
        }

        void assertReadExceed() {
            if (rdPos_ > wrPos_)
                throw std::runtime_error("read exceed");
        }

        void assertWriteExceed(size_t len) {
            if (len > wr_avail())
                throw std::runtime_error("write exceed");
        }

        void reserve(size_t n) {
            if (n <= cap_)
                return;
            if (ownType_ == Ownership::VIEW)
                throw std::runtime_error("reserve on view buffer");
            auto newbuf = (char *) realloc(buf_, n);
            if (newbuf == nullptr)
                throw std::runtime_error("failed alloc");
            ownType_ = Ownership::OWN;
            buf_ = newbuf;
            cap_ = n;
        }

        void cleanup() {
            if (ownType_ == Ownership::OWN && buf_ != nullptr) {
                free(buf_);
                buf_ = nullptr;
                cap_ = 0;
            }
        }

        bool inited() const {
            return buf_ != nullptr && cap_ > 0;
        }

        bool empty() const {
            return rdPos_ == wrPos_;
        }

        bool full() const {
            return wrPos_ == cap_;
        }

        void extendBuffer(size_t len) {
            if (ownType_ == Ownership::VIEW || len <= wr_avail())
                return;
            auto newCap = (cap_ == 0) ? len : cap_ * 2; // todo: std::min(cap_ * 2, MAX_BUFFER_CAP)
            if (newCap - wrPos_ < len) // if still not enough
                newCap = wrPos_ + len; // this is dangerous, because we alloc new buffer with no cap
            auto newbuf = (char *) realloc(buf_, newCap); // same with malloc if buf_ == NULL
            if (newbuf == nullptr)
                throw std::runtime_error("failed alloc");
            ownType_ = Ownership::OWN;
            buf_ = newbuf;
            cap_ = newCap;
        }

        size_t write(const char *data, size_t len) {
            if (data == nullptr || len == 0)
                return 0;
            if (ownType_ == Ownership::VIEW)
                throw std::runtime_error("write on view buffer");
            extendBuffer(len);
            memcpy(buf_ + wrPos_, data, len);
            incWrPos(len);
            return len;
        }

        size_t write(std::string_view val) {
            return write(val.data(), val.size());
        }

        size_t read(char *outbuf, size_t len) {
            assertReadExceed();
            if (outbuf == nullptr || len == 0 || empty())
                return 0;
            len = std::min(len, rd_avail());
            memcpy(outbuf, buf_ + rdPos_, len);
            incRdPos(len);
            return len;
        }

        char* wr_pos() {
            return buf_ + wrPos_;
        }

        char* rd_pos() const {
            return buf_ + rdPos_;
        }

        void incWrPos(size_t cnt) {
            assertWriteExceed(cnt);
            wrPos_ += cnt;
        }

        void incRdPos(size_t cnt) {
            cnt = std::min(cnt, rd_avail());
            rdPos_ += cnt;
        }

        void get_view(char **data, size_t *len) {
            *data = buf_ + rdPos_;
            *len = wrPos_ - rdPos_;
        }

        std::string_view str_view() const {
            return std::string_view(rd_pos(), rd_avail());
        }

        size_t rd_avail() const {
            return wrPos_ - rdPos_;
        }

        size_t cap() const {
            return cap_;
        }

        // size_t size() const {
        //     return wrPos_;
        // }

        size_t wr_avail() const {
            return cap_ - wrPos_;
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
}// namespace libs


#endif // LIBS_MEMBUFFER_H
