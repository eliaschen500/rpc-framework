#include "net/event_loop.h"
#include "net/channel.h"
#include "net/epoll_poller.h"
#include <stdexcept>
#include <cassert>
#include <sys/eventfd.h>
#include <unistd.h>

namespace rpc {
namespace net {

static int create_eventfd() {
    int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) throw std::runtime_error("eventfd failed");
    return fd;
}

EventLoop::EventLoop()
    : thread_id_(std::this_thread::get_id()),
      poller_(std::make_unique<EpollPoller>()),
      wakeup_fd_(create_eventfd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_))
{
    wakeup_channel_->set_read_callback([this]{ handle_read(); });
    wakeup_channel_->enable_reading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->disable_all();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
}

void EventLoop::loop() {
    assert(is_in_loop_thread());
    quit_.store(false, std::memory_order_relaxed);

    while (!quit_.load(std::memory_order_relaxed)) {
        active_channels_.clear();
        poller_->poll(10 /*ms*/, active_channels_);

        for (Channel* ch : active_channels_) {
            ch->handle_event();
        }
        do_pending_functors();
    }
}

void EventLoop::quit() {
    quit_.store(true, std::memory_order_release);
    if (!is_in_loop_thread()) wakeup();
}

void EventLoop::run_in_loop(Functor f) {
    if (is_in_loop_thread()) {
        f();
    } else {
        queue_in_loop(std::move(f));
    }
}

void EventLoop::queue_in_loop(Functor f) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(f));
    }
    if (!is_in_loop_thread() || calling_pending_functors_.load()) {
        wakeup();
    }
}

void EventLoop::update_channel(Channel* channel) {
    assert(is_in_loop_thread());
    poller_->update_channel(channel);
}

void EventLoop::remove_channel(Channel* channel) {
    assert(is_in_loop_thread());
    poller_->remove_channel(channel);
}

void EventLoop::assert_in_loop_thread() const {
    if (!is_in_loop_thread()) {
        throw std::runtime_error("EventLoop called from a wrong thread");
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::handle_read() {
    uint64_t one = 1;
    ::read(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::do_pending_functors() {
    std::vector<Functor> functors;
    calling_pending_functors_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }
    for (auto& f : functors) f();
    calling_pending_functors_.store(false);
}

} // namespace net
} // namespace rpc
