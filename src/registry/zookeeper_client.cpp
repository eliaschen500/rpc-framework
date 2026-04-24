#include "registry/zookeeper_client.h"
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <algorithm>

namespace rpc {
namespace registry {

#ifdef WITH_ZOOKEEPER
// ── Real ZooKeeper implementation ─────────────────────────────────────────

void ZooKeeperClient::zk_watcher(zhandle_t*, int type, int state,
                                  const char* path, void* ctx) {
    if (type == ZOO_CHILD_EVENT && ctx) {
        auto* cb = static_cast<WatchCallback*>(ctx);
        (*cb)(path ? path : "");
    }
}

ZooKeeperClient::ZooKeeperClient(const std::string& hosts,
                                 int session_timeout_ms)
    : hosts_(hosts)
{
    zh_ = zookeeper_init(hosts.c_str(), nullptr, session_timeout_ms,
                         nullptr, nullptr, 0);
    if (!zh_) throw std::runtime_error("zookeeper_init failed");
}

ZooKeeperClient::~ZooKeeperClient() {
    if (zh_) zookeeper_close(zh_);
}

bool ZooKeeperClient::connected() const {
    return zh_ && zoo_state(zh_) == ZOO_CONNECTED_STATE;
}

bool ZooKeeperClient::create(const std::string& path, const std::string& data) {
    int rc = zoo_create(zh_, path.c_str(),
                        data.data(), static_cast<int>(data.size()),
                        &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
    return rc == ZOK || rc == ZNODEEXISTS;
}

bool ZooKeeperClient::create_ephemeral(const std::string& path,
                                        const std::string& data) {
    int rc = zoo_create(zh_, path.c_str(),
                        data.data(), static_cast<int>(data.size()),
                        &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, nullptr, 0);
    return rc == ZOK || rc == ZNODEEXISTS;
}

std::string ZooKeeperClient::get(const std::string& path) {
    char buf[512];
    int  buf_len = sizeof(buf);
    zoo_get(zh_, path.c_str(), 0, buf, &buf_len, nullptr);
    return std::string(buf, buf_len > 0 ? buf_len : 0);
}

std::vector<std::string> ZooKeeperClient::get_children(const std::string& path) {
    String_vector sv{};
    zoo_get_children(zh_, path.c_str(), 0, &sv);
    std::vector<std::string> result;
    for (int i = 0; i < sv.count; ++i) result.emplace_back(sv.data[i]);
    deallocate_String_vector(&sv);
    return result;
}

bool ZooKeeperClient::watch_children(const std::string& path, WatchCallback cb) {
    auto* ctx = new WatchCallback(std::move(cb));
    String_vector sv{};
    int rc = zoo_wget_children(zh_, path.c_str(), zk_watcher, ctx, &sv);
    deallocate_String_vector(&sv);
    return rc == ZOK;
}

bool ZooKeeperClient::remove(const std::string& path) {
    return zoo_delete(zh_, path.c_str(), -1) == ZOK;
}

#else
// ── In-memory stub (compiles without ZK installed) ────────────────────────

struct ZooKeeperClient::Stub {
    std::mutex mu;
    std::unordered_map<std::string, std::string>              nodes;
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_map<std::string, WatchCallback>            watchers;
};

ZooKeeperClient::ZooKeeperClient(const std::string& hosts, int)
    : hosts_(hosts), stub_(std::make_unique<Stub>())
{
    std::cout << "[ZooKeeper-stub] using in-memory registry (host=" << hosts << ")\n";
}

ZooKeeperClient::~ZooKeeperClient() = default;

bool ZooKeeperClient::connected() const { return true; }

bool ZooKeeperClient::create(const std::string& path, const std::string& data) {
    std::lock_guard<std::mutex> lk(stub_->mu);
    stub_->nodes[path] = data;
    // Register under parent's children list.
    auto slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0) {
        std::string parent = path.substr(0, slash);
        std::string child  = path.substr(slash + 1);
        auto& kids = stub_->children[parent];
        if (std::find(kids.begin(), kids.end(), child) == kids.end()) {
            kids.push_back(child);
            if (stub_->watchers.count(parent)) {
                stub_->watchers[parent](parent);
            }
        }
    }
    return true;
}

bool ZooKeeperClient::create_ephemeral(const std::string& path,
                                        const std::string& data) {
    return create(path, data);
}

std::string ZooKeeperClient::get(const std::string& path) {
    std::lock_guard<std::mutex> lk(stub_->mu);
    auto it = stub_->nodes.find(path);
    return it != stub_->nodes.end() ? it->second : "";
}

std::vector<std::string> ZooKeeperClient::get_children(const std::string& path) {
    std::lock_guard<std::mutex> lk(stub_->mu);
    auto it = stub_->children.find(path);
    return it != stub_->children.end() ? it->second : std::vector<std::string>{};
}

bool ZooKeeperClient::watch_children(const std::string& path, WatchCallback cb) {
    std::lock_guard<std::mutex> lk(stub_->mu);
    stub_->watchers[path] = std::move(cb);
    return true;
}

bool ZooKeeperClient::remove(const std::string& path) {
    std::lock_guard<std::mutex> lk(stub_->mu);
    stub_->nodes.erase(path);
    auto slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0) {
        std::string parent = path.substr(0, slash);
        std::string child  = path.substr(slash + 1);
        auto& kids = stub_->children[parent];
        kids.erase(std::remove(kids.begin(), kids.end(), child), kids.end());
    }
    return true;
}

#endif // WITH_ZOOKEEPER

} // namespace registry
} // namespace rpc
