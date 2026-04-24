#pragma once
#include "zookeeper_client.h"
#include "load_balancer.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace rpc {
namespace registry {

// ZooKeeper-backed service registry.
// ZNode layout:
//   /rpc/<service_name>/<host:port>   (ephemeral, data = weight as string)
class ServiceRegistry {
public:
    explicit ServiceRegistry(const std::string& zk_hosts,
                             LBStrategy strategy = LBStrategy::RoundRobin);
    ~ServiceRegistry() = default;

    // Provider side: register this host:port under service_name.
    // Creates an ephemeral node that disappears when the process exits.
    bool register_service(const std::string& service_name,
                          const std::string& host,
                          uint16_t           port,
                          int                weight = 1);

    // Consumer side: discover and pick one endpoint.
    // Caches the endpoint list locally and refreshes on ZK watcher events.
    ServiceEndpoint discover(const std::string& service_name);

    // Remove an explicit registration (e.g. graceful shutdown).
    bool unregister_service(const std::string& service_name,
                            const std::string& host,
                            uint16_t           port);

private:
    void refresh(const std::string& service_name);
    void on_children_changed(const std::string& path);

    std::unique_ptr<ZooKeeperClient>                      zk_;
    std::unique_ptr<LoadBalancer>                         lb_;

    std::mutex                                            mu_;
    std::unordered_map<std::string,
        std::vector<ServiceEndpoint>>                     cache_;
};

} // namespace registry
} // namespace rpc
