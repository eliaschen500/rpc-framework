#include "net/thread_pool.h"

namespace rpc {
namespace net {

ThreadPool::ThreadPool(size_t thread_count) {
    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back([this]{ worker(); });
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    cv_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stop_.load()) throw std::runtime_error("ThreadPool: submit on stopped pool");
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::worker() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this]{ return stop_.load() || !tasks_.empty(); });
            if (stop_.load() && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

} // namespace net
} // namespace rpc
