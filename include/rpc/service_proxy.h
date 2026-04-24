#pragma once
#include "rpc_channel.h"
#include "rpc_controller.h"
#include <memory>
#include <string>

namespace rpc {

// RAII wrapper that owns an RpcChannel and the generated Stub.
// Usage:
//   ServiceProxy<HelloService> proxy("127.0.0.1", 8080);
//   proxy->SayHello(&ctrl, &req, &resp, nullptr);
template <typename ServiceType>
class ServiceProxy {
public:
    ServiceProxy(const std::string& host, uint16_t port,
                 int timeout_ms = 3000)
        : channel_(std::make_unique<RpcChannel>()),
          stub_(std::make_unique<typename ServiceType::Stub>(channel_.get()))
    {
        if (!channel_->connect(host, port, timeout_ms)) {
            throw std::runtime_error("RpcChannel: connect failed to "
                                     + host + ":" + std::to_string(port));
        }
    }

    ~ServiceProxy() {
        channel_->disconnect();
    }

    // Non-copyable, movable.
    ServiceProxy(const ServiceProxy&)            = delete;
    ServiceProxy& operator=(const ServiceProxy&) = delete;
    ServiceProxy(ServiceProxy&&)                 = default;
    ServiceProxy& operator=(ServiceProxy&&)      = default;

    typename ServiceType::Stub* operator->() { return stub_.get(); }
    typename ServiceType::Stub& operator*()  { return *stub_; }

    RpcChannel* channel() { return channel_.get(); }

private:
    std::unique_ptr<RpcChannel>                    channel_;
    std::unique_ptr<typename ServiceType::Stub>    stub_;
};

} // namespace rpc
