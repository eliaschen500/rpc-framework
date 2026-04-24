#include "codec/codec.h"
#include "codec/protocol.h"
#include <arpa/inet.h>
#include <cstring>

namespace rpc {
namespace codec {

Codec::Codec(MessageCallback msg_cb)
    : msg_cb_(std::move(msg_cb)) {}

void Codec::on_message(const net::TcpConnectionPtr& conn, net::Buffer* buf) {
    while (buf->readable() >= kHeaderSize) {
        // Peek at magic number.
        uint32_t magic;
        std::memcpy(&magic, buf->peek(), sizeof(magic));
        magic = ntohl(magic);

        if (magic != kMagicNumber) {
            if (err_cb_) err_cb_(conn, "bad magic number");
            conn->force_close();
            return;
        }

        // Peek at body length.
        uint32_t body_len_net;
        std::memcpy(&body_len_net, buf->peek() + 4, sizeof(body_len_net));
        const size_t body_len = ntohl(body_len_net);

        if (body_len > kMaxBodySize) {
            if (err_cb_) err_cb_(conn, "body too large");
            conn->force_close();
            return;
        }

        if (buf->readable() < kHeaderSize + body_len) {
            // Incomplete frame — wait for more data.
            return;
        }

        // Consume header.
        buf->retrieve(kHeaderSize);
        // Extract body.
        std::string frame = buf->retrieve_as_string(body_len);
        msg_cb_(conn, std::move(frame));
    }
}

void Codec::send(const net::TcpConnectionPtr& conn,
                 const google::protobuf::Message& msg) {
    std::string body;
    if (!msg.SerializeToString(&body)) {
        return;
    }
    send_raw(conn, body);
}

void Codec::send_raw(const net::TcpConnectionPtr& conn,
                     const std::string& body) {
    const uint32_t magic_net    = htonl(kMagicNumber);
    const uint32_t body_len_net = htonl(static_cast<uint32_t>(body.size()));

    net::Buffer buf;
    buf.append(reinterpret_cast<const char*>(&magic_net),    4);
    buf.append(reinterpret_cast<const char*>(&body_len_net), 4);
    buf.append(body);
    conn->send(&buf);
}

} // namespace codec
} // namespace rpc
