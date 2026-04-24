#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

// Conditionally include the real ZK headers.
#ifdef WITH_ZOOKEEPER
#include <zookeeper/zookeeper.h>
#endif

namespace rpc {
namespace registry {

// Thin RAII wrapper around the ZooKeeper C client.
// When compiled without WITH_ZOOKEEPER, provides a no-op in-memory stub
// so the rest of the codebase compiles and tests cleanly.
class ZooKeeperClient {
public:
    using WatchCallback = std::function<void(const std::string& path)>;

    explicit ZooKeeperClient(const std::string& hosts,
                             int session_timeout_ms = 30000);
    ~ZooKeeperClient();

    // Non-copyable.
    ZooKeeperClient(const ZooKeeperClient&)            = delete;
    ZooKeeperClient& operator=(const ZooKeeperClient&) = delete;

    bool connected() const;

    // Creates a persistent znode (creates parent nodes as needed).
    bool create(const std::string& path, const std::string& data = "");

    // Creates an ephemeral znode (auto-deleted when session ends).
    bool create_ephemeral(const std::string& path, const std::string& data = "");

    // Returns the data stored at path.
    std::string get(const std::string& path);

    // Returns child node names.
    std::vector<std::string> get_children(const std::string& path);

    // Registers a watcher that fires when the child list of path changes.
    bool watch_children(const std::string& path, WatchCallback cb);

    // Deletes a node.
    bool remove(const std::string& path);

private:
#ifdef WITH_ZOOKEEPER
    static void zk_watcher(zhandle_t* zh, int type, int state,
                            const char* path, void* ctx);
    zhandle_t* zh_{nullptr};
#else
    // In-memory stub state.
    struct Stub;
    std::unique_ptr<Stub> stub_;
#endif

    std::string hosts_;
};

} // namespace registry
} // namespace rpc
