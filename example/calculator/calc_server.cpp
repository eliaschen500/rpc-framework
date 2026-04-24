#include "rpc/rpc_server.h"
#include "calculator.pb.h"
#include <iostream>

class CalcServiceImpl : public example::CalcService {
public:
    void Add(google::protobuf::RpcController*,
             const example::CalcRequest* req,
             example::CalcResponse*      resp,
             google::protobuf::Closure*  done) override {
        resp->set_result(req->a() + req->b());
        done->Run();
    }
    void Sub(google::protobuf::RpcController*,
             const example::CalcRequest* req,
             example::CalcResponse*      resp,
             google::protobuf::Closure*  done) override {
        resp->set_result(req->a() - req->b());
        done->Run();
    }
    void Mul(google::protobuf::RpcController*,
             const example::CalcRequest* req,
             example::CalcResponse*      resp,
             google::protobuf::Closure*  done) override {
        resp->set_result(req->a() * req->b());
        done->Run();
    }
    void Div(google::protobuf::RpcController*,
             const example::CalcRequest* req,
             example::CalcResponse*      resp,
             google::protobuf::Closure*  done) override {
        if (req->b() == 0.0) {
            resp->set_error("division by zero");
        } else {
            resp->set_result(req->a() / req->b());
        }
        done->Run();
    }
};

int main() {
    rpc::RpcServer server("0.0.0.0", 9091, 4);
    CalcServiceImpl svc;
    server.register_service(&svc);
    std::cout << "CalcService listening on :9091\n";
    server.start();
}
