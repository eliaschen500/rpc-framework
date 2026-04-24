#include "registry/service_registry.h"
#include <sstream>

namespace rpc {
namespace registry {

static const std::string kRootPath = "/rpc";

ServiceRegistry::ServiceRegistry(const std::string& zk_hosts,
                                  LBStrategy strategy)
    : zk_(std::make_unique<ZooKeeperClient>(zk_hosts)),
      lb_(make_load_balancer(strategy))
{
    zk_->create(kRootPath);
}

bool ServiceRegistry::register_service(const std::string& service_name,
                                        const std::string& host,
                                        uint16_t           port,
                                        int                weight)
{
    std::string service_path = kRootPath + "/" + service_name;
    zk_->create(service_path);

    std::string node_path = service_path + "/" + host + ":" + std::to_string(port);
    return zk_->create_ephemeral(node_path, std::to_string(weight));
}

ServiceEndpoint ServiceRegistry::discover(const std::string& service_name) {
    std::vector<ServiceEndpoint> endpoints;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = cache_.find(service_name);
        if (it != cache_.end()) {
            endpoints = it->second;
        }
    }

    if (endpoints.empty()) {
        refresh(service_name);
        std::lock_guard<std::mutex> lk(mu_);
        endpoints = cache_[service_name];
    }

    return lb_->select(endpoints);
}

void ServiceRegistry::refresh(const std::string& service_name) {
    std::string service_path = kRootPath + "/" + service_name;

    // Set up watcher for future updates.
    zk_->watch_children(service_path,
        [this, service_name](const std::string&) {
            refresh(service_name);
        });

    auto children = zk_->get_children(service_path);
    std::vector<ServiceEndpoint> endpoints;
    endpoints.reserve(children.size());

    for (const auto& child : children) {
        // child is "host:port"
        auto colon = child.rfind(':');
        if (colon == std::string::npos) continue;
        ServiceEndpoint ep;
        ep.host = child.substr(0, colon);
        ep.port = static_cast<uint16_t>(std::stoi(child.substr(colon + 1)));

        std::string weight_str = zk_->get(service_path + "/" + child);
        if (!weight_str.empty()) ep.weight = std::stoi(weight_str);
        endpoints.push_back(ep);
    }

    std::lock_guard<std::mutex> lk(mu_);
    cache_[service_name] = std::move(endpoints);
}

bool ServiceRegistry::unregister_service(const std::string& service_name,
                                          const std::string& host,
                                          uint16_t           port)
{
    std::string node_path = kRootPath + "/" + service_name + "/"
                            + host + ":" + std::to_string(port);
    return zk_->remove(node_path);
}

} // namespace registry
} // namespace rpc
