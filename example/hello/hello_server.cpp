#include "rpc/rpc_server.h"
#include "hello.pb.h"
#include <iostream>

class HelloServiceImpl : public example::HelloService {
public:
    void SayHello(google::protobuf::RpcController* ctrl,
                  const example::HelloRequest*     req,
                  example::HelloResponse*          resp,
                  google::protobuf::Closure*        done) override
    {
        std::cout << "[server] SayHello: " << req->name() << "\n";
        resp->set_message("Hello, " + req->name() + "!");
        done->Run();
    }

    void SayBye(google::protobuf::RpcController* ctrl,
                const example::HelloRequest*     req,
                example::HelloResponse*          resp,
                google::protobuf::Closure*        done) override
    {
        std::cout << "[server] SayBye: " << req->name() << "\n";
        resp->set_message("Goodbye, " + req->name() + "!");
        done->Run();
    }
};

int main() {
    rpc::RpcServer server("0.0.0.0", 9090, /*io_threads=*/4);

    HelloServiceImpl service;
    server.register_service(&service);

    std::cout << "HelloService listening on :9090\n";
    server.start();
}
