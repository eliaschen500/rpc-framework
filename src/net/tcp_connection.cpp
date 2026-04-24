#include "net/tcp_connection.h"
#include "net/channel.h"
#include "net/event_loop.h"
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>

namespace rpc {
namespace net {

TcpConnection::TcpConnection(EventLoop* loop, int sockfd, uint64_t conn_id,
                             std::string peer_addr)
    : loop_(loop),
      sockfd_(sockfd),
      conn_id_(conn_id),
      peer_addr_(std::move(peer_addr)),
      channel_(std::make_unique<Channel>(loop, sockfd))
{
    channel_->set_read_callback([this]{ handle_read(); });
    channel_->set_write_callback([this]{ handle_write(); });
    channel_->set_close_callback([this]{ handle_close(); });
    channel_->set_error_callback([this]{ handle_error(); });
    // Keep-alive at socket level.
    int opt = 1;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
}

TcpConnection::~TcpConnection() {
    ::close(sockfd_);
}

void TcpConnection::connection_established() {
    state_.store(kConnected);
    channel_->tie(shared_from_this());
    channel_->enable_reading();
    if (conn_cb_) conn_cb_(shared_from_this());
}

void TcpConnection::connection_destroyed() {
    if (state_.load() == kConnected) {
        state_.store(kDisconnected);
        channel_->disable_all();
        if (conn_cb_) conn_cb_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::send(const std::string& data) {
    if (state_.load() != kConnected) return;
    if (loop_->is_in_loop_thread()) {
        send_in_loop(data.data(), data.size());
    } else {
        // Copy data for cross-thread send.
        loop_->run_in_loop([this, data]{
            send_in_loop(data.data(), data.size());
        });
    }
}

void TcpConnection::send(Buffer* buf) {
    if (state_.load() != kConnected) return;
    std::string data = buf->retrieve_all_as_string();
    loop_->run_in_loop([this, data]{
        send_in_loop(data.data(), data.size());
    });
}

void TcpConnection::send_in_loop(const char* data, size_t len) {
    loop_->assert_in_loop_thread();
    if (state_.load() == kDisconnected) return;

    ssize_t written = 0;
    // Write directly if output buffer is empty (happy path).
    if (!channel_->is_writing() && output_buf_.readable() == 0) {
        written = ::write(sockfd_, data, len);
        if (written < 0) {
            if (errno != EWOULDBLOCK) {
                handle_error();
                return;
            }
            written = 0;
        } else if (static_cast<size_t>(written) == len) {
            if (wc_cb_) wc_cb_(shared_from_this());
            return;
        }
    }
    // Buffer remaining bytes and enable write event.
    output_buf_.append(data + written, len - static_cast<size_t>(written));
    if (!channel_->is_writing()) {
        channel_->enable_writing();
    }
}

void TcpConnection::shutdown() {
    if (state_.load() == kConnected) {
        state_.store(kDisconnecting);
        loop_->run_in_loop([this]{ shutdown_in_loop(); });
    }
}

void TcpConnection::shutdown_in_loop() {
    if (!channel_->is_writing()) {
        ::shutdown(sockfd_, SHUT_WR);
    }
}

void TcpConnection::force_close() {
    if (state_.load() == kConnected || state_.load() == kDisconnecting) {
        state_.store(kDisconnecting);
        loop_->queue_in_loop([self = shared_from_this()]{ self->handle_close(); });
    }
}

void TcpConnection::handle_read() {
    int saved_errno = 0;
    ssize_t n = input_buf_.read_fd(sockfd_, &saved_errno);
    if (n > 0) {
        if (msg_cb_) msg_cb_(shared_from_this(), &input_buf_);
    } else if (n == 0) {
        handle_close();
    } else {
        errno = saved_errno;
        handle_error();
    }
}

void TcpConnection::handle_write() {
    if (!channel_->is_writing()) return;
    ssize_t n = ::write(sockfd_,
                        output_buf_.peek(),
                        output_buf_.readable());
    if (n > 0) {
        output_buf_.retrieve(static_cast<size_t>(n));
        if (output_buf_.readable() == 0) {
            channel_->disable_writing();
            if (wc_cb_) wc_cb_(shared_from_this());
            if (state_.load() == kDisconnecting) shutdown_in_loop();
        }
    }
}

void TcpConnection::handle_close() {
    state_.store(kDisconnected);
    channel_->disable_all();
    TcpConnectionPtr guard = shared_from_this();
    if (conn_cb_)   conn_cb_(guard);
    if (close_cb_)  close_cb_(guard);
}

void TcpConnection::handle_error() {
    // Silently close; in production, log errno here.
    handle_close();
}

} // namespace net
} // namespace rpc
