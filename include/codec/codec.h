#pragma once
#include "protocol.h"
#include "net/buffer.h"
#include "net/tcp_connection.h"
#include <functional>
#include <string>
#include <google/protobuf/message.h>

namespace rpc {
namespace codec {

// Handles framing (magic + length prefix) to solve TCP packet
// splitting / sticking problems. Calls message_cb_ once a complete
// frame is available.
class Codec {
public:
    using MessageCallback = std::function<void(
        const net::TcpConnectionPtr&, std::string frame)>;
    using ErrorCallback = std::function<void(
        const net::TcpConnectionPtr&, const char* reason)>;

    explicit Codec(MessageCallback msg_cb);

    void set_error_callback(ErrorCallback cb) { err_cb_ = std::move(cb); }

    // Called by TcpConnection when data arrives.
    void on_message(const net::TcpConnectionPtr& conn, net::Buffer* buf);

    // Encodes a protobuf message into a framed buffer and sends it.
    static void send(const net::TcpConnectionPtr& conn,
                     const google::protobuf::Message& msg);

    // Encodes a raw byte string into a framed buffer and sends it.
    static void send_raw(const net::TcpConnectionPtr& conn,
                         const std::string& body);

private:
    MessageCallback msg_cb_;
    ErrorCallback   err_cb_;
};

} // namespace codec
} // namespace rpc
