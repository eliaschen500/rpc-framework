#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace rpc {
namespace net {

class Channel;
class EpollPoller;

// One EventLoop per thread (the "one loop per thread" model).
// Safe to call run_in_loop / queue_in_loop from any thread.
class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    // If called from the owning thread, runs f immediately.
    // Otherwise queues it and wakes up the loop.
    void run_in_loop(Functor f);
    void queue_in_loop(Functor f);

    void update_channel(Channel* channel);
    void remove_channel(Channel* channel);

    bool is_in_loop_thread() const {
        return thread_id_ == std::this_thread::get_id();
    }

    void assert_in_loop_thread() const;

private:
    void wakeup();
    void handle_read();        // reads wakeup fd
    void do_pending_functors();

    using ChannelList = std::vector<Channel*>;

    std::atomic<bool>                  quit_{false};
    std::thread::id                    thread_id_;
    std::unique_ptr<EpollPoller>       poller_;

    int                                wakeup_fd_;   // eventfd
    std::unique_ptr<Channel>           wakeup_channel_;

    ChannelList                        active_channels_;

    std::mutex                         mutex_;
    std::vector<Functor>               pending_functors_;
    std::atomic<bool>                  calling_pending_functors_{false};
};

} // namespace net
} // namespace rpc
