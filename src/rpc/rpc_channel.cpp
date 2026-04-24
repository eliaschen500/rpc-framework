#include "rpc/rpc_channel.h"
#include "rpc/rpc_controller.h"
#include "proto/rpc.pb.h"
#include <google/protobuf/descriptor.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace rpc {

// Bare-minimum blocking IO for the client channel.
// The server uses the full Reactor stack; the client keeps it simple
// with a dedicated reader thread + mutex-gated writes.

RpcChannel::RpcChannel()
    : codec_([this](const net::TcpConnectionPtr& conn, std::string frame){
          on_message(conn, std::move(frame));
      })
{}

RpcChannel::~RpcChannel() {
    disconnect();
}

bool RpcChannel::connect(const std::string& host, uint16_t port,
                         int timeout_ms) {
    sockfd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sockfd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = ::inet_addr(host.c_str());

    // Set non-blocking for connect timeout.
    int flags = ::fcntl(sockfd_, F_GETFL, 0);
    ::fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);

    int ret = ::connect(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        ::close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    if (ret != 0) {
        pollfd pfd{sockfd_, POLLOUT, 0};
        if (::poll(&pfd, 1, timeout_ms) <= 0) {
            ::close(sockfd_);
            sockfd_ = -1;
            return false;
        }
        int err = 0;
        socklen_t len = sizeof(err);
        ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            ::close(sockfd_);
            sockfd_ = -1;
            return false;
        }
    }

    // Restore blocking mode for simplicity.
    ::fcntl(sockfd_, F_SETFL, flags);
    connected_.store(true);

    // Start a dedicated reader thread.
    std::thread([this]{
        net::Buffer buf;
        while (connected_.load()) {
            int saved = 0;
            ssize_t n = buf.read_fd(sockfd_, &saved);
            if (n <= 0) {
                connected_.store(false);
                // Wake up any pending calls with an error.
                std::lock_guard<std::mutex> lk(mu_);
                for (auto& [id, call] : pending_calls_) {
                    call->controller->SetFailed("connection closed");
                    call->done_flag = true;
                    call->cv.notify_all();
                }
                break;
            }
            // Reuse codec to parse frames (pass a dummy conn ptr).
            while (buf.readable() >= codec::kHeaderSize) {
                uint32_t magic_net, body_len_net;
                std::memcpy(&magic_net,    buf.peek(),     4);
                std::memcpy(&body_len_net, buf.peek() + 4, 4);
                if (ntohl(magic_net) != codec::kMagicNumber) {
                    connected_.store(false);
                    return;
                }
                size_t body_len = ntohl(body_len_net);
                if (buf.readable() < codec::kHeaderSize + body_len) break;
                buf.retrieve(codec::kHeaderSize);
                std::string frame = buf.retrieve_as_string(body_len);
                on_message(nullptr, std::move(frame));
            }
        }
    }).detach();

    return true;
}

void RpcChannel::disconnect() {
    if (connected_.exchange(false)) {
        ::shutdown(sockfd_, SHUT_RDWR);
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

bool RpcChannel::is_connected() const {
    return connected_.load();
}

void RpcChannel::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    google::protobuf::RpcController*          controller,
    const google::protobuf::Message*          request,
    google::protobuf::Message*                response,
    google::protobuf::Closure*                done)
{
    if (!is_connected()) {
        controller->SetFailed("not connected");
        if (done) done->Run();
        return;
    }

    uint32_t request_id = next_request_id_.fetch_add(1);

    // Build RpcRequest frame.
    rpc_proto::RpcRequest rpc_req;
    rpc_req.set_service_name(method->service()->full_name());
    rpc_req.set_method_name(method->name());
    rpc_req.set_request_id(request_id);

    std::string args;
    request->SerializeToString(&args);
    rpc_req.set_args_data(args);

    std::string body;
    rpc_req.SerializeToString(&body);

    auto pending = std::make_shared<PendingCall>();
    pending->controller = controller;
    pending->response   = response;
    pending->done       = done;

    {
        std::lock_guard<std::mutex> lk(mu_);
        pending_calls_[request_id] = pending;
    }

    // Encode and write: [Magic:4][BodyLen:4][Body]
    uint32_t magic_net    = htonl(codec::kMagicNumber);
    uint32_t body_len_net = htonl(static_cast<uint32_t>(body.size()));
    std::string wire;
    wire.append(reinterpret_cast<char*>(&magic_net),    4);
    wire.append(reinterpret_cast<char*>(&body_len_net), 4);
    wire.append(body);

    {
        std::lock_guard<std::mutex> lk(conn_mu_);
        ssize_t written = ::write(sockfd_, wire.data(), wire.size());
        if (written < 0) {
            std::lock_guard<std::mutex> lk2(mu_);
            pending_calls_.erase(request_id);
            controller->SetFailed("write failed");
            if (done) done->Run();
            return;
        }
    }

    if (done) {
        // Async: closure will be called from the reader thread.
        return;
    }

    // Synchronous: wait for response.
    std::unique_lock<std::mutex> lk(mu_);
    pending->cv.wait(lk, [&pending]{ return pending->done_flag; });
}

void RpcChannel::on_message(const net::TcpConnectionPtr& /*conn*/,
                             std::string frame) {
    rpc_proto::RpcResponse resp;
    if (!resp.ParseFromString(frame)) return;

    std::shared_ptr<PendingCall> call;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = pending_calls_.find(resp.request_id());
        if (it == pending_calls_.end()) return;
        call = it->second;
        pending_calls_.erase(it);
    }

    if (resp.success()) {
        call->response->ParseFromString(resp.result_data());
    } else {
        call->controller->SetFailed(resp.error_msg());
    }

    {
        std::lock_guard<std::mutex> lk(mu_);
        call->done_flag = true;
        call->cv.notify_all();
    }

    if (call->done) call->done->Run();
}

void RpcChannel::on_connection(const net::TcpConnectionPtr& conn) {
    connected_.store(conn->connected());
    conn_cv_.notify_all();
}

} // namespace rpc
