/*
 * File:   MemBuf.h
 * Author: thaipq
 *
 * Created on Wed Jul 02 2025 10:12:58 PM
 */

#ifndef SAMPLE_CODES_MEMBUF_H
#define SAMPLE_CODES_MEMBUF_H

#include <cstdlib>
#include <cstdio>
#include <cstring>

class MemBuf {
public:
    enum class Ownership {
        VIEW,
        OWN,
    };

public:
    MemBuf() : buf_(nullptr), wrPos_(0), rdPos_(0), size_(0), ownType_(Ownership::VIEW) {}
    explicit MemBuf(std::size_t size) : buf_(nullptr), wrPos_(0), rdPos_(0), size_(size), ownType_(Ownership::OWN) {
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
        size_ = 0;
    }

    size_t write(const char *data, size_t len) {
        if (data == nullptr || len == 0)
            return 0;
        if (wrPos_ + len >= size_) {
            auto newsize = size_ * 2;
            auto newbuf = (char *) realloc(buf_, newsize);
            if (buf_ != newbuf) {
                memcpy(newbuf, buf_, wrPos_);
            }
            buf_ = newbuf;
            size_ = newsize;
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

    void increaseWr(size_t cnt) {
        wrPos_ += cnt;
    }

    void get_view(char **data, size_t *len) {
        *data = buf_ + rdPos_;
        *len = wrPos_ - rdPos_;
    }

    char *rawbuf() {
        return buf_;
    }

    size_t cap() const {
        return size_;
    }

    void resetBuf() {
        wrPos_ = 0;
        rdPos_ = 0;
    }

    void resetBuf(char *outbuf, size_t len) {// view only
        cleanup();
        buf_ = outbuf;
        size_ = len;
        wrPos_ = 0;
        rdPos_ = 0;
        ownType_ = Ownership::VIEW;
    }

private:
    char *buf_;
    size_t wrPos_;
    size_t rdPos_;
    size_t size_;
    Ownership ownType_;
};

#endif// SAMPLE_CODES_MEMBUF_H
