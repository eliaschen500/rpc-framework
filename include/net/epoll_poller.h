#pragma once
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

namespace rpc {
namespace net {

class Channel;

// Edge-triggered epoll wrapper. Returns active channels on each poll().
class EpollPoller {
public:
    using ChannelList = std::vector<Channel*>;

    explicit EpollPoller();
    ~EpollPoller();

    // Polls for I/O events, fills active_channels, returns true if any.
    void poll(int timeout_ms, ChannelList& active_channels);

    void update_channel(Channel* channel);
    void remove_channel(Channel* channel);

private:
    static const int kInitEventListSize = 16;
    static const int kNew     = -1;
    static const int kAdded   =  0;
    static const int kDeleted =  1;

    void epoll_ctl(int op, Channel* channel);
    void fill_active_channels(int num_events, ChannelList& active_channels) const;

    int epfd_;
    std::vector<epoll_event>               events_;
    std::unordered_map<int, Channel*>      channels_;  // fd -> Channel*
};

} // namespace net
} // namespace rpc
