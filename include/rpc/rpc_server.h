#pragma once
#include "net/tcp_server.h"
#include "net/event_loop.h"
#include "codec/codec.h"
#include <google/protobuf/service.h>
#include <unordered_map>
#include <string>

namespace rpc {

// Accepts TCP connections, decodes RPC frames, dispatches to registered
// google::protobuf::Service instances, and sends back responses.
class RpcServer {
public:
    RpcServer(const std::string& host, uint16_t port, int io_threads = 4);
    ~RpcServer();

    // Register a service. Does NOT take ownership.
    void register_service(google::protobuf::Service* service);

    void start();   // blocks until quit() is called
    void quit();

private:
    void on_connection(const net::TcpConnectionPtr& conn);
    void on_message(const net::TcpConnectionPtr& conn, net::Buffer* buf);
    void on_frame(const net::TcpConnectionPtr& conn, std::string frame);
    void dispatch_request(const net::TcpConnectionPtr& conn,
                          uint32_t request_id,
                          google::protobuf::Service*              service,
                          const google::protobuf::MethodDescriptor* method,
                          google::protobuf::Message*              request);

    net::EventLoop                          loop_;
    net::TcpServer                          server_;
    codec::Codec                            codec_;
    net::ThreadPool                         worker_pool_;

    std::unordered_map<std::string,
        google::protobuf::Service*>         services_;
};

} // namespace rpc
