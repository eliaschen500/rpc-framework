#include "registry/load_balancer.h"
#include <stdexcept>
#include <random>
#include <numeric>

namespace rpc {
namespace registry {

// ── Random ────────────────────────────────────────────────────────────────
ServiceEndpoint RandomLoadBalancer::select(
    const std::vector<ServiceEndpoint>& endpoints)
{
    if (endpoints.empty()) throw std::runtime_error("no available endpoints");
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, endpoints.size() - 1);
    return endpoints[dist(rng)];
}

// ── Round-Robin ───────────────────────────────────────────────────────────
ServiceEndpoint RoundRobinLoadBalancer::select(
    const std::vector<ServiceEndpoint>& endpoints)
{
    if (endpoints.empty()) throw std::runtime_error("no available endpoints");
    size_t idx = counter_.fetch_add(1, std::memory_order_relaxed) % endpoints.size();
    return endpoints[idx];
}

// ── Weighted Random ───────────────────────────────────────────────────────
ServiceEndpoint WeightedLoadBalancer::select(
    const std::vector<ServiceEndpoint>& endpoints)
{
    if (endpoints.empty()) throw std::runtime_error("no available endpoints");
    int total = 0;
    for (const auto& ep : endpoints) total += ep.weight;

    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, total - 1);
    int r = dist(rng);

    for (const auto& ep : endpoints) {
        r -= ep.weight;
        if (r < 0) return ep;
    }
    return endpoints.back();
}

// ── Consistent Hash ───────────────────────────────────────────────────────
ConsistentHashLoadBalancer::ConsistentHashLoadBalancer(int virtual_nodes)
    : virtual_nodes_(virtual_nodes) {}

uint32_t ConsistentHashLoadBalancer::hash(const std::string& s) const {
    // FNV-1a 32-bit
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

ServiceEndpoint ConsistentHashLoadBalancer::select(
    const std::vector<ServiceEndpoint>& endpoints)
{
    if (endpoints.empty()) throw std::runtime_error("no available endpoints");

    // Build the ring each time (endpoints list may change).
    std::map<uint32_t, const ServiceEndpoint*> ring;
    for (const auto& ep : endpoints) {
        for (int i = 0; i < virtual_nodes_; ++i) {
            uint32_t h = hash(ep.to_string() + "#" + std::to_string(i));
            ring[h]    = &ep;
        }
    }

    uint32_t key_hash = hash(key_.empty() ? "default" : key_);
    auto it = ring.lower_bound(key_hash);
    if (it == ring.end()) it = ring.begin();
    return *it->second;
}

// ── Factory ───────────────────────────────────────────────────────────────
std::unique_ptr<LoadBalancer> make_load_balancer(LBStrategy strategy) {
    switch (strategy) {
        case LBStrategy::Random:          return std::make_unique<RandomLoadBalancer>();
        case LBStrategy::RoundRobin:      return std::make_unique<RoundRobinLoadBalancer>();
        case LBStrategy::Weighted:        return std::make_unique<WeightedLoadBalancer>();
        case LBStrategy::ConsistentHash:  return std::make_unique<ConsistentHashLoadBalancer>();
    }
    return std::make_unique<RoundRobinLoadBalancer>();
}

} // namespace registry
} // namespace rpc
