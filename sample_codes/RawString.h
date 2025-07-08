/*
 * File:   RawStr.h
 * Author: thaipq
 *
 * Created on Wed Jul 02 2025 3:12:32 PM
 */

#ifndef SAMPLE_CODES_MEMBUF_H
#define SAMPLE_CODES_MEMBUF_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

class RawStr {
public:
    RawStr() : mem_(nullptr), size_(0), cap_(0) {}
    RawStr(const char *s) : mem_(nullptr), size_(0), cap_(0) {
        if (s == nullptr || *s == '\0') return;
        size_ = strlen(s);
        cap_ = size_ + 1;
        mem_ = (char *) malloc(cap_);
        memcpy(mem_, s, size_);
        mem_[size_] = '\0';
    }
    ~RawStr() {
        cleanup();
    }
    RawStr(const RawStr &o) noexcept : mem_(nullptr), size_(0), cap_(0) {
        if (o.mem_ != nullptr && o.size_ > 0) { // other have data
            mem_ = (char *) malloc(o.size_ + 1);// +1 for null terminator
            memcpy(mem_, o.mem_, o.size_);
            mem_[o.size_] = '\0';
            size_ = o.size_;
            cap_ = o.size_ + 1;
            return;
        }
    }
    RawStr &operator=(const RawStr &o) noexcept {
        if (this == &o) return *this;
        if (o.mem_ != nullptr && o.size_ > 0) {// other have data
            if (mem_ == nullptr) {
                mem_ = (char *) malloc(o.size_ + 1);// +1 for null terminator
                cap_ = o.size_ + 1;
            } else if (cap_ < o.size_ + 1) {
                auto newcap = allocMem(o.size_ + 1);
                mem_ = (char *) realloc(mem_, newcap);
                cap_ = newcap;
            }
            memcpy(mem_, o.mem_, o.size_);
            mem_[o.size_] = '\0';
            size_ = o.size_;
            return *this;
        }
        // other null
        cleanup();
        return *this;
    }

    RawStr(RawStr &&o) noexcept : mem_(nullptr), size_(0), cap_(0) {
        if (o.mem_ && o.size_) {
            std::swap(mem_, o.mem_);
            std::swap(size_, o.size_);
            std::swap(cap_, o.cap_);
            o.reset();
            return;
        }
        // other empty, do nothing
    }

    RawStr &operator=(RawStr &&o) noexcept {
        if (this == &o) return *this;
        if (o.mem_ && o.size_) {
            cleanup();
            std::swap(mem_, o.mem_);
            std::swap(size_, o.size_);
            std::swap(cap_, o.cap_);
            o.reset();
            return *this;
        }
        cleanup();
        return *this;
    }

    const char *c_str() const {
        if (mem_ == nullptr || size_ == 0) return "";
        // Ensure null termination
        if (mem_[size_] != '\0') {
            // This is dangerous - we're modifying in a const method
            // Better to always keep strings null-terminated
            const_cast<RawStr *>(this)->ensureNullTerminated();
        }
        return mem_;
    }

    void append(const char *s, size_t len) {
        if (s == nullptr || len == 0) return;
        if (size_ + len + 1 > cap_) {// +1 for null terminator
            auto newcap = allocMem(size_ + len + 1);
            if (mem_ == nullptr) {
                mem_ = (char *) malloc(newcap);
            } else {
                mem_ = (char *) realloc(mem_, newcap);
            }
            cap_ = newcap;
        }
        memcpy(mem_ + size_, s, len);
        size_ += len;
        mem_[size_] = '\0';
    }

    void append(const char *s) {
        append(s, s ? strlen(s) : 0);
    }

    RawStr &operator+=(const char *s) {
        append(s);
        return *this;
    }

    RawStr &operator+=(char c) {
        append(&c, 1);
        return *this;
    }

    const char *data() const {
        return mem_;
    }

    size_t size() const {
        return size_;
    }

    size_t capacity() const {
        return cap_;
    }

public:
    friend bool operator==(const RawStr &lhs, const RawStr &rhs) {
        if (lhs.size_ != rhs.size_) return false;
        if (lhs.mem_ == nullptr && rhs.mem_ == nullptr) return true;
        if (lhs.mem_ == nullptr || rhs.mem_ == nullptr) return false;
        return memcmp(lhs.mem_, rhs.mem_, lhs.size_) == 0;
    }

private:
    void cleanup() {
        if (mem_) {
            free(mem_);
            mem_ = nullptr;
            size_ = 0;
            cap_ = 0;
        }
    }

    void reset() {
        mem_ = nullptr;
        size_ = 0;
        cap_ = 0;
    }

    size_t allocMem(std::size_t size) {
        if (cap_ >= size) return cap_;
        auto newcap = cap_ != 0 ? cap_ : 256;
        while (newcap < size) {
            newcap *= 2;
        }
        return newcap;
    }

    void ensureNullTerminated() {
        if (cap_ <= size_) {
            auto newcap = allocMem(size_ + 1);
            mem_ = (char *) realloc(mem_, newcap);
            cap_ = newcap;
        }
        mem_[size_] = '\0';
    }


private:
    char *mem_;
    size_t size_;
    size_t cap_;
};

#endif// SAMPLE_CODES_MEMBUF_H
