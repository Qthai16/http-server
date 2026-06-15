#include <gtest/gtest.h>

#include "libs/MemBuffer.h"

using libs::MemBuf;

// --- construction ---

TEST(MemBuf, default_ctor_is_empty_and_uninited) {
    MemBuf buf;
    EXPECT_FALSE(buf.inited());
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.cap(), 0u);
    EXPECT_EQ(buf.wr_avail(), 0u);
    EXPECT_EQ(buf.rd_avail(), 0u);
}

TEST(MemBuf, sized_ctor_allocates) {
    MemBuf buf(64);
    EXPECT_TRUE(buf.inited());
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.cap(), 64u);
    EXPECT_EQ(buf.wr_avail(), 64u);
    EXPECT_EQ(buf.rd_avail(), 0u);
    EXPECT_NE(buf.wr_pos(), nullptr);
}

// --- write / read ---

TEST(MemBuf, write_and_read_roundtrip) {
    MemBuf buf(32);
    const char src[] = "hello world";
    EXPECT_EQ(buf.write(src, 11), 11u);
    EXPECT_EQ(buf.rd_avail(), 11u);

    char dst[16] = {};
    EXPECT_EQ(buf.read(dst, 16), 11u);
    EXPECT_EQ(std::string(dst, 11), "hello world");
    EXPECT_TRUE(buf.empty());
}

TEST(MemBuf, write_grows_unset_buffer) {
    MemBuf buf;
    EXPECT_EQ(buf.cap(), 0u);
    buf.write("abc", 3);
    EXPECT_GE(buf.cap(), 3u);
    EXPECT_EQ(buf.rd_avail(), 3u);
}

TEST(MemBuf, write_exact_fit_no_overalloc) {
    MemBuf buf(8);
    buf.write("12345678", 8);
    EXPECT_TRUE(buf.full());
    EXPECT_EQ(buf.cap(), 8u); // no growth
}

TEST(MemBuf, write_grows_by_doubling) {
    MemBuf buf(8);
    buf.write("12345678", 8); // fill exactly
    buf.write("x", 1);       // triggers growth
    EXPECT_GE(buf.cap(), 16u);
    EXPECT_EQ(buf.rd_avail(), 9u);
}

TEST(MemBuf, write_null_data_returns_zero) {
    MemBuf buf(16);
    EXPECT_EQ(buf.write(nullptr, 8), 0u);
    EXPECT_EQ(buf.write("data", 0), 0u);
    EXPECT_TRUE(buf.empty());
}

TEST(MemBuf, write_on_view_buffer_throws) {
    char backing[32] = {};
    MemBuf buf;
    buf.resetBuf(backing, sizeof(backing));
    EXPECT_THROW(buf.write("x", 1), std::runtime_error);
}

TEST(MemBuf, write_string_view_overload) {
    MemBuf buf(32);
    std::string_view sv = "hello";
    buf.write(sv);
    EXPECT_EQ(buf.rd_avail(), 5u);
    EXPECT_EQ(buf.str_view(), sv);
}

TEST(MemBuf, read_partial) {
    MemBuf buf(16);
    buf.write("abcdefgh", 8);

    char out[4] = {};
    EXPECT_EQ(buf.read(out, 4), 4u);
    EXPECT_EQ(std::string(out, 4), "abcd");
    EXPECT_EQ(buf.rd_avail(), 4u);
}

TEST(MemBuf, read_more_than_available_returns_avail) {
    MemBuf buf(16);
    buf.write("abc", 3);
    char out[64] = {};
    EXPECT_EQ(buf.read(out, 64), 3u);
    EXPECT_EQ(std::string(out, 3), "abc");
}

TEST(MemBuf, read_from_empty_returns_zero) {
    MemBuf buf(16);
    char out[8] = {};
    EXPECT_EQ(buf.read(out, 8), 0u);
}

TEST(MemBuf, read_null_outbuf_returns_zero) {
    MemBuf buf(16);
    buf.write("data", 4);
    EXPECT_EQ(buf.read(nullptr, 4), 0u);
}

// --- reserve ---

TEST(MemBuf, reserve_on_unset_allocates) {
    MemBuf buf;
    buf.reserve(128);
    EXPECT_GE(buf.cap(), 128u);
    EXPECT_TRUE(buf.inited());
    EXPECT_TRUE(buf.empty());
}

TEST(MemBuf, reserve_less_than_cap_is_noop) {
    MemBuf buf(64);
    buf.reserve(32);
    EXPECT_EQ(buf.cap(), 64u);
}

TEST(MemBuf, reserve_expands_and_preserves_data) {
    MemBuf buf(8);
    buf.write("hello", 5);
    buf.reserve(256);
    EXPECT_GE(buf.cap(), 256u);
    EXPECT_EQ(buf.rd_avail(), 5u);
    EXPECT_EQ(buf.str_view(), "hello");
}

TEST(MemBuf, reserve_on_view_throws) {
    char backing[32] = {};
    MemBuf buf;
    buf.resetBuf(backing, sizeof(backing));
    EXPECT_THROW(buf.reserve(64), std::runtime_error);
}

// --- incWrPos / incRdPos ---

TEST(MemBuf, inc_wr_pos_direct_write_pattern) {
    MemBuf buf(16);
    memcpy(buf.wr_pos(), "direct", 6);
    buf.incWrPos(6);
    EXPECT_EQ(buf.rd_avail(), 6u);
    EXPECT_EQ(buf.str_view(), "direct");
}

TEST(MemBuf, inc_wr_pos_overflow_asserts) {
    MemBuf buf(8);
    EXPECT_DEATH(buf.incWrPos(9), "");
}

TEST(MemBuf, inc_rd_pos_clamps_to_avail) {
    MemBuf buf(16);
    buf.write("abc", 3);
    buf.incRdPos(100); // more than available
    EXPECT_TRUE(buf.empty());
}

// --- resetBuf ---

TEST(MemBuf, reset_buf_clears_positions_keeps_alloc) {
    MemBuf buf(32);
    buf.write("hello", 5);
    buf.resetBuf();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.cap(), 32u); // allocation intact
}

TEST(MemBuf, reset_buf_view_sets_view_ownership) {
    char backing[16] = "existing";
    MemBuf buf(64);
    buf.resetBuf(backing, sizeof(backing));
    EXPECT_EQ(buf.cap(), 16u);
    EXPECT_TRUE(buf.empty()); // wrPos reset to 0
    EXPECT_THROW(buf.write("x", 1), std::runtime_error);
}

TEST(MemBuf, reset_buf_view_over_own_frees_old_alloc) {
    // ASan will catch double-free or leak if cleanup is wrong
    char backing[8] = {};
    MemBuf buf(64);
    buf.write("data", 4);
    buf.resetBuf(backing, sizeof(backing)); // should free the 64-byte alloc
    EXPECT_EQ(buf.cap(), 8u);
}

// --- str_view ---

TEST(MemBuf, str_view_reflects_unread_region) {
    MemBuf buf(32);
    buf.write("abcdef", 6);
    buf.incRdPos(2); // consume "ab"
    EXPECT_EQ(buf.str_view(), "cdef");
}

TEST(MemBuf, str_view_empty_after_full_read) {
    MemBuf buf(16);
    buf.write("hi", 2);
    char out[4];
    buf.read(out, 4);
    EXPECT_EQ(buf.str_view(), "");
}

// --- multiple writes across growth ---

TEST(MemBuf, multiple_writes_accumulate_correctly) {
    MemBuf buf;
    buf.write("foo", 3);
    buf.write("bar", 3);
    buf.write("baz", 3);
    EXPECT_EQ(buf.rd_avail(), 9u);
    EXPECT_EQ(buf.str_view(), "foobarbaz");
}

TEST(MemBuf, interleaved_read_write) {
    MemBuf buf(4);
    buf.write("abcd", 4);
    char out[4] = {};
    buf.read(out, 2);
    EXPECT_EQ(std::string(out, 2), "ab");
    buf.resetBuf();
    buf.write("xy", 2);
    EXPECT_EQ(buf.str_view(), "xy");
}
