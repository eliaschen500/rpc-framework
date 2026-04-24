#include "rpc/service_proxy.h"
#include "rpc/rpc_controller.h"
#include "hello.pb.h"
#include <iostream>

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 9090;

    try {
        rpc::ServiceProxy<example::HelloService> proxy(host, port);

        // ── SayHello ────────────────────────────────────
        {
            rpc::RpcController  ctrl;
            example::HelloRequest  req;
            example::HelloResponse resp;
            req.set_name("World");

            proxy->SayHello(&ctrl, &req, &resp, nullptr);

            if (ctrl.Failed()) {
                std::cerr << "RPC failed: " << ctrl.ErrorText() << "\n";
            } else {
                std::cout << "SayHello → " << resp.message() << "\n";
            }
        }

        // ── SayBye ──────────────────────────────────────
        {
            rpc::RpcController  ctrl;
            example::HelloRequest  req;
            example::HelloResponse resp;
            req.set_name("World");

            proxy->SayBye(&ctrl, &req, &resp, nullptr);

            if (ctrl.Failed()) {
                std::cerr << "RPC failed: " << ctrl.ErrorText() << "\n";
            } else {
                std::cout << "SayBye → " << resp.message() << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
