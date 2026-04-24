#include "net/buffer.h"
#include <sys/uio.h>
#include <cerrno>
#include <unistd.h>

namespace rpc {
namespace net {

ssize_t Buffer::read_fd(int fd, int* saved_errno) {
    // Use an on-stack buffer for the second iovec so we can read everything
    // the kernel has buffered in one syscall even if our Buffer is small.
    char extra[65536];
    iovec vec[2];
    const size_t writable_bytes = writable();

    vec[0].iov_base = begin_write();
    vec[0].iov_len  = writable_bytes;
    vec[1].iov_base = extra;
    vec[1].iov_len  = sizeof(extra);

    const int iovcnt = (writable_bytes < sizeof(extra)) ? 2 : 1;
    const ssize_t n  = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable_bytes) {
        has_written(static_cast<size_t>(n));
    } else {
        write_idx_ = buf_.size();
        append(extra, static_cast<size_t>(n) - writable_bytes);
    }
    return n;
}

} // namespace net
} // namespace rpc
