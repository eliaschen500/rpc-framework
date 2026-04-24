#pragma once
#include "buffer.h"
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace rpc {
namespace net {

class Channel;
class EventLoop;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback    = std::function<void(const TcpConnectionPtr&, Buffer*)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback      = std::function<void(const TcpConnectionPtr&)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, int sockfd, uint64_t conn_id,
                  std::string peer_addr);
    ~TcpConnection();

    void set_connection_callback(ConnectionCallback cb)     { conn_cb_    = std::move(cb); }
    void set_message_callback(MessageCallback cb)           { msg_cb_     = std::move(cb); }
    void set_write_complete_callback(WriteCompleteCallback cb){ wc_cb_    = std::move(cb); }
    void set_close_callback(CloseCallback cb)               { close_cb_   = std::move(cb); }

    void send(const std::string& data);
    void send(Buffer* buf);
    void shutdown();
    void force_close();

    void connection_established();
    void connection_destroyed();

    EventLoop*  loop()      const { return loop_; }
    uint64_t    conn_id()   const { return conn_id_; }
    bool        connected() const { return state_ == kConnected; }
    const std::string& peer_addr() const { return peer_addr_; }

private:
    enum State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    void handle_read();
    void handle_write();
    void handle_close();
    void handle_error();

    void send_in_loop(const char* data, size_t len);
    void shutdown_in_loop();

    EventLoop*               loop_;
    int                      sockfd_;
    uint64_t                 conn_id_;
    std::string              peer_addr_;
    std::atomic<State>       state_{kConnecting};

    std::unique_ptr<Channel> channel_;
    Buffer                   input_buf_;
    Buffer                   output_buf_;

    ConnectionCallback       conn_cb_;
    MessageCallback          msg_cb_;
    WriteCompleteCallback    wc_cb_;
    CloseCallback            close_cb_;
};

} // namespace net
} // namespace rpc
