#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>

namespace rpc {
namespace net {

// Fixed-size thread pool with a bounded task queue.
class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t thread_count);
    ~ThreadPool();

    void submit(Task task);

    size_t thread_count() const { return threads_.size(); }

private:
    void worker();

    std::vector<std::thread>   threads_;
    std::queue<Task>           tasks_;
    std::mutex                 mu_;
    std::condition_variable    cv_;
    std::atomic<bool>          stop_{false};
};

} // namespace net
} // namespace rpc
