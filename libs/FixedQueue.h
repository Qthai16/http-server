#ifndef SAMPLE_CODES_FIXEDQUEUE_CPP
#define SAMPLE_CODES_FIXEDQUEUE_CPP

#include <queue>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <stdexcept>

#include <memory>

std::shared_ptr<int> abc;

template<typename T>
class FixQueue {
public:
    explicit FixQueue(size_t cap, bool init = false) : cont_(nullptr), wr_(-1), rd_(-1), cap_(cap) {
        if (init) lazyInit();
    }
    ~FixQueue() {
        if (cont_ != nullptr) {
            delete[] cont_;
            cont_ = nullptr;
            cap_ = 0;
        }
    }
    size_t size() const {
        if (!cont_) return 0;  // Not initialized yet
        if (wr_ == rd_) return 0;
        return wr_ > rd_ ? wr_ - rd_ : cap_ - rd_ + wr_;
    }

    bool empty() const {
        if (!cont_) return true;  // Not initialized yet
        return rd_ == wr_;
    }

    T& front() {
        if (rd_ < 0) throw std::runtime_error("not initialized");
        if (empty()) throw std::runtime_error("queue is empty");
        return cont_[rd_];
    }

    T& back() {
        if (wr_ < 0) throw std::runtime_error("not initialized");
        if (empty()) throw std::runtime_error("queue is empty");
        return wr_ == 0 ? cont_[cap_ - 1] : cont_[wr_ - 1];
    }

    bool full() const {
        if (!cont_) return false;
        return (wr_ + 1) % cap_ == rd_;
    }

    void push(T &&v) {
        lazyInit();
        if (full()) {
            throw std::runtime_error("queue is full");
        }
        cont_[wr_] = std::move(v);
        wr_ = (wr_ + 1) % cap_;
    }

    void push(const T &v) {
        lazyInit();
        if (full()) {
            throw std::runtime_error("queue is full");
        }
        cont_[wr_] = v;
        wr_ = (wr_ + 1) % cap_;
    }

    std::pair<bool, T> pop() {
        lazyInit();
        if (empty()) return {false, T{}};
        auto nextrd = (rd_ + 1) % cap_;
        T val = cont_[rd_];
        rd_ = nextrd;
        return {true, std::move(val)};
    }

private:
    void lazyInit() {
        if (cont_ != nullptr) return;
        if (cap_ == 0) cap_ = 1;
        cont_ = new T[cap_];
        rd_ = 0;
        wr_ = 0;
    }

private:
    T *cont_;
    int wr_;
    int rd_;
    size_t cap_;
};

#endif// SAMPLE_CODES_FIXEDQUEUE_CPP
