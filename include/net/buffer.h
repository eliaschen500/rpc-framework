#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <sys/types.h>

namespace rpc {
namespace net {

// Contiguous byte buffer with cheap prepend and O(1) append.
// Layout: [prependable | readable | writable]
class Buffer {
public:
    static constexpr size_t kPrependSize = 8;
    static constexpr size_t kInitialSize = 4096;

    explicit Buffer(size_t initial_size = kInitialSize)
        : buf_(kPrependSize + initial_size),
          read_idx_(kPrependSize),
          write_idx_(kPrependSize) {}

    size_t readable()    const { return write_idx_ - read_idx_; }
    size_t writable()    const { return buf_.size() - write_idx_; }
    size_t prependable() const { return read_idx_; }

    const char* peek() const { return begin() + read_idx_; }

    void ensure_writable(size_t len) {
        if (writable() < len) grow(len);
    }

    char* begin_write() { return begin() + write_idx_; }

    void has_written(size_t len) { write_idx_ += len; }

    void append(const char* data, size_t len) {
        ensure_writable(len);
        std::memcpy(begin_write(), data, len);
        has_written(len);
    }

    void append(const std::string& str) { append(str.data(), str.size()); }

    void retrieve(size_t len) {
        assert(len <= readable());
        read_idx_ += len;
        if (read_idx_ == write_idx_) reset();
    }

    std::string retrieve_as_string(size_t len) {
        assert(len <= readable());
        std::string res(peek(), len);
        retrieve(len);
        return res;
    }

    std::string retrieve_all_as_string() { return retrieve_as_string(readable()); }

    // Reads directly from fd using scatter read (stack buffer trick).
    ssize_t read_fd(int fd, int* saved_errno);

private:
    char*       begin()       { return buf_.data(); }
    const char* begin() const { return buf_.data(); }

    void reset() { read_idx_ = write_idx_ = kPrependSize; }

    void grow(size_t len) {
        if (writable() + prependable() >= len + kPrependSize) {
            size_t readable_bytes = readable();
            std::memmove(begin() + kPrependSize, peek(), readable_bytes);
            read_idx_  = kPrependSize;
            write_idx_ = read_idx_ + readable_bytes;
        } else {
            buf_.resize(write_idx_ + len);
        }
    }

    std::vector<char> buf_;
    size_t read_idx_;
    size_t write_idx_;
};

} // namespace net
} // namespace rpc
