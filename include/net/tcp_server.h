#pragma once
#include "tcp_connection.h"
#include "thread_pool.h"
#include <unordered_map>
#include <string>
#include <atomic>

namespace rpc {
namespace net {

class EventLoop;

class TcpServer {
public:
    TcpServer(EventLoop* loop, const std::string& host, uint16_t port);
    ~TcpServer();

    void set_thread_num(int n);
    void set_connection_callback(ConnectionCallback cb)  { conn_cb_ = std::move(cb); }
    void set_message_callback(MessageCallback cb)        { msg_cb_  = std::move(cb); }
    void set_write_complete_callback(WriteCompleteCallback cb){ wc_cb_ = std::move(cb); }

    void start();

private:
    void handle_accept();
    void new_connection(int sockfd, const std::string& peer_addr);
    void remove_connection(const TcpConnectionPtr& conn);
    void remove_connection_in_loop(const TcpConnectionPtr& conn);

    EventLoop*   loop_;      // acceptor loop
    int          listen_fd_;
    std::unique_ptr<Channel> accept_channel_;

    std::unique_ptr<ThreadPool> io_thread_pool_;
    std::vector<EventLoop*>     io_loops_;

    std::atomic<uint64_t> next_conn_id_{1};
    std::unordered_map<uint64_t, TcpConnectionPtr> connections_;

    ConnectionCallback    conn_cb_;
    MessageCallback       msg_cb_;
    WriteCompleteCallback wc_cb_;

    std::atomic<bool> started_{false};
};

} // namespace net
} // namespace rpc
