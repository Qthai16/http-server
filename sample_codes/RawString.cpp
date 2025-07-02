class RawString {
    RawString() : mem_(nullptr), size_(0), cap_(0) {}
    RawString(const RawString& o) : : mem_(nullptr), size_(0), cap_(0) {
        if (o.mem_ && o.size_) { // other have data
            mem_ = (char*) malloc(o.cap_);
            size_ = o.size_;
            cap_ = o.cap_;
            memcpy(mem_, o.mem_, o.size_);
            return;
        }
        assert(o.mem_ == nullptr && o.size_ == 0);
        // other mem_ nullptr
        mem_ = nullptr;
        size_ = 0;
        cap_ = 0;
    }

    std::size_t calSize(std::size_t s) {
        if (cap_ == 0) return s;
        auto newsize = cap_;
        while (newsize < s) {
            newsize = newsize*2;
        }
        return newsize;
    }

    RawString& RawString(const RawString& o) {
        if (o.mem_ && o.size) {
            if (mem_ == nullptr){ // not init, malloc
                mem_ = (char*) malloc(o.cap_);
            } else if (cap_ < o.size_) { // not enough size, realloc
                cap_ = calSize(o.size)
                mem_ = (char*) realloc(mem_, cap_);
            }
            memcpy(mem_, o.mem_, o.size_);
            size_ = o.size_;
        }
    }

private:
    char* mem_;
    size_t size_;
    size_t cap_;
}