#include "net/channel.h"
#include "net/event_loop.h"
#include <sys/epoll.h>
#include <cassert>

namespace rpc {
namespace net {

const int Channel::kNoneEvent  = 0;
const int Channel::kReadEvent  = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd) {}

Channel::~Channel() {
    assert(is_none_event());
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_  = obj;
    tied_ = true;
}

void Channel::update() {
    loop_->update_channel(this);
}

void Channel::remove() {
    disable_all();
    loop_->remove_channel(this);
}

void Channel::handle_event() {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) handle_event_with_guard();
    } else {
        handle_event_with_guard();
    }
}

void Channel::handle_event_with_guard() {
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (close_cb_) close_cb_();
    }
    if (revents_ & (EPOLLERR)) {
        if (error_cb_) error_cb_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_cb_) read_cb_();
    }
    if (revents_ & EPOLLOUT) {
        if (write_cb_) write_cb_();
    }
}

} // namespace net
} // namespace rpc
