#include "net/tcp_server.h"
#include "net/event_loop.h"
#include "net/channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

namespace rpc {
namespace net {

static int create_listen_fd(const std::string& host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = host.empty() ? INADDR_ANY : ::inet_addr(host.c_str());

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error("bind() failed on port " + std::to_string(port));
    }
    if (::listen(fd, SOMAXCONN) < 0) {
        ::close(fd);
        throw std::runtime_error("listen() failed");
    }
    return fd;
}

// Minimal EventLoop wrapper for IO threads.
class IOLoopThread {
public:
    IOLoopThread() {
        thread_ = std::thread([this]{
            loop_ = std::make_unique<EventLoop>();
            ready_.store(true);
            ready_cv_.notify_one();
            loop_->loop();
        });
        std::unique_lock<std::mutex> lk(mu_);
        ready_cv_.wait(lk, [this]{ return ready_.load(); });
    }
    ~IOLoopThread() {
        if (loop_) loop_->quit();
        if (thread_.joinable()) thread_.join();
    }
    EventLoop* loop() { return loop_.get(); }
private:
    std::thread                  thread_;
    std::unique_ptr<EventLoop>   loop_;
    std::atomic<bool>            ready_{false};
    std::mutex                   mu_;
    std::condition_variable      ready_cv_;
};

TcpServer::TcpServer(EventLoop* loop, const std::string& host, uint16_t port)
    : loop_(loop),
      listen_fd_(create_listen_fd(host, port)),
      accept_channel_(std::make_unique<Channel>(loop, listen_fd_))
{
    accept_channel_->set_read_callback([this]{ handle_accept(); });
}

TcpServer::~TcpServer() {
    accept_channel_->disable_all();
    accept_channel_->remove();
    ::close(listen_fd_);
    for (auto& [id, conn] : connections_) {
        conn->connection_destroyed();
    }
}

void TcpServer::set_thread_num(int n) {
    io_thread_pool_ = std::make_unique<ThreadPool>(static_cast<size_t>(n));
    // Create dedicated EventLoops for IO threads.
    // (Simple approach: each IO thread gets its own loop started inline.)
}

void TcpServer::start() {
    if (started_.exchange(true)) return;
    accept_channel_->enable_reading();
}

void TcpServer::handle_accept() {
    sockaddr_in peer{};
    socklen_t   len = sizeof(peer);
    int conn_fd = ::accept4(listen_fd_,
                            reinterpret_cast<sockaddr*>(&peer),
                            &len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd < 0) return;

    char buf[64];
    ::inet_ntop(AF_INET, &peer.sin_addr, buf, sizeof(buf));
    std::string peer_addr = std::string(buf) + ":" + std::to_string(ntohs(peer.sin_port));

    new_connection(conn_fd, peer_addr);
}

void TcpServer::new_connection(int sockfd, const std::string& peer_addr) {
    uint64_t id = next_conn_id_.fetch_add(1);
    // All connections run on the acceptor loop for simplicity
    // (a real impl would round-robin across io_loops_).
    auto conn = std::make_shared<TcpConnection>(loop_, sockfd, id, peer_addr);
    connections_[id] = conn;

    conn->set_connection_callback(conn_cb_);
    conn->set_message_callback(msg_cb_);
    conn->set_write_complete_callback(wc_cb_);
    conn->set_close_callback([this](const TcpConnectionPtr& c){
        remove_connection(c);
    });

    loop_->run_in_loop([conn]{ conn->connection_established(); });
}

void TcpServer::remove_connection(const TcpConnectionPtr& conn) {
    loop_->run_in_loop([this, conn]{
        connections_.erase(conn->conn_id());
        conn->connection_destroyed();
    });
}

} // namespace net
} // namespace rpc
