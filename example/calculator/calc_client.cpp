#include "rpc/service_proxy.h"
#include "rpc/rpc_controller.h"
#include "calculator.pb.h"
#include <iostream>
#include <iomanip>

static void call(example::CalcService::Stub& stub,
                 const char*  op,
                 double a, double b)
{
    rpc::RpcController    ctrl;
    example::CalcRequest  req;
    example::CalcResponse resp;
    req.set_a(a);
    req.set_b(b);

    if (std::string(op) == "Add")       stub.Add(&ctrl, &req, &resp, nullptr);
    else if (std::string(op) == "Sub")  stub.Sub(&ctrl, &req, &resp, nullptr);
    else if (std::string(op) == "Mul")  stub.Mul(&ctrl, &req, &resp, nullptr);
    else                                stub.Div(&ctrl, &req, &resp, nullptr);

    std::cout << std::fixed << std::setprecision(2);
    if (ctrl.Failed()) {
        std::cerr << op << "(" << a << ", " << b << ") RPC error: "
                  << ctrl.ErrorText() << "\n";
    } else if (!resp.error().empty()) {
        std::cerr << op << "(" << a << ", " << b << ") = ERROR: "
                  << resp.error() << "\n";
    } else {
        std::cout << op << "(" << a << ", " << b << ") = "
                  << resp.result() << "\n";
    }
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 9091;

    try {
        rpc::ServiceProxy<example::CalcService> proxy(host, port);
        auto& stub = *proxy;

        call(stub, "Add", 10.0, 3.0);
        call(stub, "Sub", 10.0, 3.0);
        call(stub, "Mul", 10.0, 3.0);
        call(stub, "Div", 10.0, 3.0);
        call(stub, "Div", 10.0, 0.0);   // division by zero

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
