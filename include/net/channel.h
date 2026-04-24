#pragma once
#include <functional>
#include <memory>

namespace rpc {
namespace net {

class EventLoop;

// Owns no fd lifetime — just ties a fd to an EventLoop with read/write/error callbacks.
class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handle_event();

    void set_read_callback(EventCallback cb)  { read_cb_  = std::move(cb); }
    void set_write_callback(EventCallback cb) { write_cb_ = std::move(cb); }
    void set_error_callback(EventCallback cb) { error_cb_ = std::move(cb); }
    void set_close_callback(EventCallback cb) { close_cb_ = std::move(cb); }

    int  fd()     const { return fd_; }
    int  events() const { return events_; }
    void set_revents(int revents) { revents_ = revents; }

    bool is_writing() const { return events_ & kWriteEvent; }
    bool is_reading() const { return events_ & kReadEvent; }
    bool is_none_event() const { return events_ == kNoneEvent; }

    void enable_reading()  { events_ |=  kReadEvent;  update(); }
    void disable_reading() { events_ &= ~kReadEvent;  update(); }
    void enable_writing()  { events_ |=  kWriteEvent; update(); }
    void disable_writing() { events_ &= ~kWriteEvent; update(); }
    void disable_all()     { events_  =  kNoneEvent;  update(); }

    // Used by EpollPoller to track EPOLL_CTL_ADD vs EPOLL_CTL_MOD.
    int  index() const     { return index_; }
    void set_index(int idx){ index_ = idx; }

    EventLoop* owner_loop() const { return loop_; }
    void remove();

    // Tie to a shared_ptr so we can detect object destruction before dispatch.
    void tie(const std::shared_ptr<void>& obj);

private:
    void update();
    void handle_event_with_guard();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    const int  fd_;
    int        events_{0};
    int        revents_{0};
    int        index_{-1};  // -1 = new, 0 = added, 1 = deleted

    std::weak_ptr<void> tie_;
    bool tied_{false};

    EventCallback read_cb_;
    EventCallback write_cb_;
    EventCallback error_cb_;
    EventCallback close_cb_;
};

} // namespace net
} // namespace rpc
