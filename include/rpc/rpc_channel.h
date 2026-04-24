#pragma once
#include "net/buffer.h"
#include "net/tcp_connection.h"
#include "codec/codec.h"
#include <google/protobuf/service.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <condition_variable>

namespace rpc {

// Implements google::protobuf::RpcChannel.
// Manages the connection to one RPC server, multiplexes concurrent calls
// via request_id, and handles serialization / deserialization.
class RpcChannel : public google::protobuf::RpcChannel {
public:
    RpcChannel();
    ~RpcChannel() override;

    // Connect to host:port (blocking until connected or timeout).
    bool connect(const std::string& host, uint16_t port,
                 int timeout_ms = 3000);

    void disconnect();
    bool is_connected() const;

    // google::protobuf::RpcChannel interface.
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController*          controller,
                    const google::protobuf::Message*          request,
                    google::protobuf::Message*                response,
                    google::protobuf::Closure*                done) override;

private:
    struct PendingCall {
        google::protobuf::RpcController* controller;
        google::protobuf::Message*       response;
        google::protobuf::Closure*       done;
        std::condition_variable          cv;
        bool                             done_flag{false};
    };

    void on_message(const net::TcpConnectionPtr& conn, std::string frame);
    void on_connection(const net::TcpConnectionPtr& conn);

    int                                     sockfd_{-1};
    net::TcpConnectionPtr                   conn_;
    codec::Codec                            codec_;

    std::atomic<uint32_t>                   next_request_id_{1};
    std::mutex                              mu_;
    std::unordered_map<uint32_t,
        std::shared_ptr<PendingCall>>       pending_calls_;

    std::mutex                              conn_mu_;
    std::condition_variable                 conn_cv_;
    std::atomic<bool>                       connected_{false};
};

} // namespace rpc
