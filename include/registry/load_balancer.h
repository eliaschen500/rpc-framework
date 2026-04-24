#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <map>

namespace rpc {
namespace registry {

struct ServiceEndpoint {
    std::string host;
    uint16_t    port;
    int         weight{1};

    std::string to_string() const {
        return host + ":" + std::to_string(port);
    }
};

// Abstract load balancer.
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;
    virtual ServiceEndpoint select(const std::vector<ServiceEndpoint>& endpoints) = 0;
};

// Stateless random selection.
class RandomLoadBalancer : public LoadBalancer {
public:
    ServiceEndpoint select(const std::vector<ServiceEndpoint>& endpoints) override;
};

// Round-robin across all available endpoints.
class RoundRobinLoadBalancer : public LoadBalancer {
public:
    ServiceEndpoint select(const std::vector<ServiceEndpoint>& endpoints) override;
private:
    std::atomic<uint64_t> counter_{0};
};

// Weighted random: endpoints with higher weight get proportionally more requests.
class WeightedLoadBalancer : public LoadBalancer {
public:
    ServiceEndpoint select(const std::vector<ServiceEndpoint>& endpoints) override;
};

// Consistent hash: maps a request key to a stable endpoint.
// Defaults to using endpoint address as the virtual-node seed.
class ConsistentHashLoadBalancer : public LoadBalancer {
public:
    explicit ConsistentHashLoadBalancer(int virtual_nodes = 150);
    ServiceEndpoint select(const std::vector<ServiceEndpoint>& endpoints) override;

    // Provide an explicit hash key (e.g. client IP or user-id) to control affinity.
    void set_hash_key(const std::string& key) { key_ = key; }

private:
    int         virtual_nodes_;
    std::string key_;
    uint32_t    hash(const std::string& s) const;
};

// Factory.
enum class LBStrategy { Random, RoundRobin, Weighted, ConsistentHash };

std::unique_ptr<LoadBalancer> make_load_balancer(LBStrategy strategy);

} // namespace registry
} // namespace rpc
