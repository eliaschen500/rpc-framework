#include "rpc/rpc_server.h"
#include "rpc/rpc_controller.h"
#include "proto/rpc.pb.h"
#include <google/protobuf/descriptor.h>
#include <iostream>

namespace rpc {

RpcServer::RpcServer(const std::string& host, uint16_t port, int io_threads)
    : server_(&loop_, host, port),
      codec_([this](const net::TcpConnectionPtr& conn, std::string frame){
          on_frame(conn, std::move(frame));
      }),
      worker_pool_(static_cast<size_t>(io_threads))
{
    server_.set_connection_callback([this](const net::TcpConnectionPtr& conn){
        on_connection(conn);
    });
    server_.set_message_callback([this](const net::TcpConnectionPtr& conn,
                                        net::Buffer* buf){
        on_message(conn, buf);
    });
}

RpcServer::~RpcServer() {
    loop_.quit();
}

void RpcServer::register_service(google::protobuf::Service* service) {
    const auto* desc = service->GetDescriptor();
    services_[desc->full_name()] = service;
}

void RpcServer::start() {
    server_.start();
    std::cout << "[RpcServer] listening ...\n";
    loop_.loop();   // blocks until quit()
}

void RpcServer::quit() {
    loop_.quit();
}

void RpcServer::on_connection(const net::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::cout << "[RpcServer] new connection from " << conn->peer_addr() << "\n";
    } else {
        std::cout << "[RpcServer] connection closed: " << conn->peer_addr() << "\n";
    }
}

void RpcServer::on_message(const net::TcpConnectionPtr& conn, net::Buffer* buf) {
    codec_.on_message(conn, buf);
}

void RpcServer::on_frame(const net::TcpConnectionPtr& conn, std::string frame) {
    rpc_proto::RpcRequest req;
    if (!req.ParseFromString(frame)) {
        std::cerr << "[RpcServer] failed to parse RpcRequest\n";
        return;
    }

    auto it = services_.find(req.service_name());
    if (it == services_.end()) {
        rpc_proto::RpcResponse resp;
        resp.set_request_id(req.request_id());
        resp.set_success(false);
        resp.set_error_msg("service not found: " + req.service_name());
        codec::Codec::send(conn, resp);
        return;
    }

    google::protobuf::Service* service = it->second;
    const auto* method = service->GetDescriptor()->FindMethodByName(req.method_name());
    if (!method) {
        rpc_proto::RpcResponse resp;
        resp.set_request_id(req.request_id());
        resp.set_success(false);
        resp.set_error_msg("method not found: " + req.method_name());
        codec::Codec::send(conn, resp);
        return;
    }

    google::protobuf::Message* request_msg =
        service->GetRequestPrototype(method).New();
    if (!request_msg->ParseFromString(req.args_data())) {
        rpc_proto::RpcResponse resp;
        resp.set_request_id(req.request_id());
        resp.set_success(false);
        resp.set_error_msg("failed to parse request args");
        codec::Codec::send(conn, resp);
        delete request_msg;
        return;
    }

    uint32_t request_id = req.request_id();
    worker_pool_.submit([this, conn, service, method, request_msg, request_id]{
        dispatch_request(conn, request_id, service, method, request_msg);
    });
}

void RpcServer::dispatch_request(
    const net::TcpConnectionPtr& conn,
    uint32_t request_id,
    google::protobuf::Service*              service,
    const google::protobuf::MethodDescriptor* method,
    google::protobuf::Message*              request)
{
    std::unique_ptr<google::protobuf::Message> req_guard(request);
    std::unique_ptr<google::protobuf::Message> response(
        service->GetResponsePrototype(method).New());

    RpcController controller;

    bool done = false;
    std::mutex mu;
    std::condition_variable cv;

    google::protobuf::Closure* closure = google::protobuf::NewCallback(
        [&]{
            std::lock_guard<std::mutex> lk(mu);
            done = true;
            cv.notify_one();
        });

    service->CallMethod(method, &controller, request, response.get(), closure);

    {
        std::unique_lock<std::mutex> lk(mu);
        cv.wait(lk, [&]{ return done; });
    }

    rpc_proto::RpcResponse resp;
    resp.set_request_id(request_id);

    if (controller.Failed()) {
        resp.set_success(false);
        resp.set_error_msg(controller.ErrorText());
    } else {
        std::string serialized;
        response->SerializeToString(&serialized);
        resp.set_success(true);
        resp.set_result_data(serialized);
    }

    // Send from the IO loop, not from the worker thread.
    conn->loop()->run_in_loop([conn, resp]{
        codec::Codec::send(conn, resp);
    });
}

} // namespace rpc
