#include "net/epoll_poller.h"
#include "net/channel.h"
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace rpc {
namespace net {

EpollPoller::EpollPoller()
    : epfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epfd_ < 0) {
        throw std::runtime_error("epoll_create1 failed");
    }
}

EpollPoller::~EpollPoller() {
    ::close(epfd_);
}

void EpollPoller::poll(int timeout_ms, ChannelList& active_channels) {
    int num_events = ::epoll_wait(epfd_,
                                  events_.data(),
                                  static_cast<int>(events_.size()),
                                  timeout_ms);
    if (num_events < 0) {
        if (errno != EINTR) {
            // Log or handle error; don't throw here to keep the loop alive.
        }
        return;
    }
    fill_active_channels(num_events, active_channels);
    if (static_cast<size_t>(num_events) == events_.size()) {
        events_.resize(events_.size() * 2);
    }
}

void EpollPoller::fill_active_channels(int num_events,
                                        ChannelList& active_channels) const {
    for (int i = 0; i < num_events; ++i) {
        Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
        ch->set_revents(static_cast<int>(events_[i].events));
        active_channels.push_back(ch);
    }
}

void EpollPoller::update_channel(Channel* channel) {
    const int index = channel->index();
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            channels_[channel->fd()] = channel;
        }
        channel->set_index(kAdded);
        epoll_ctl(EPOLL_CTL_ADD, channel);
    } else {
        if (channel->is_none_event()) {
            epoll_ctl(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            epoll_ctl(EPOLL_CTL_MOD, channel);
        }
    }
}

void EpollPoller::remove_channel(Channel* channel) {
    channels_.erase(channel->fd());
    if (channel->index() == kAdded) {
        epoll_ctl(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EpollPoller::epoll_ctl(int op, Channel* channel) {
    epoll_event ev{};
    ev.events   = static_cast<uint32_t>(channel->events());
    ev.data.ptr = channel;
    if (::epoll_ctl(epfd_, op, channel->fd(), &ev) < 0) {
        // In production: log this error.
    }
}

} // namespace net
} // namespace rpc
