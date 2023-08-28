#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>
namespace rx{
class EventLoop;
class TcpConnection;

enum STATE{
    New, // channel未添加到poller中
    Added, // channel已添加到poller中
    Deleted // // channel从poller中删除
};


class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);  

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }


    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; return 0; }

    // 设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; updateChannel(); }
    void disableReading() { events_ &= ~kReadEvent; updateChannel(); }
    void enableWriting() { events_ |= kWriteEvent; updateChannel(); }
    void disableWriting() { events_ &= ~kWriteEvent; updateChannel(); }
    void disableAll() { events_ = kNoneEvent; updateChannel(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int state() { return state_; }
    void set_state(STATE sta) { state_ = sta; }

    EventLoop* ownerLoop() { return loop_; }
    void removeChannel();
private:

    void updateChannel();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    // 每个channel需要保存监听它的事件循环，因为每个loop对应一个epoller，loop是通过一个epoller来对监听的通道进行添加和删除的操作的
    EventLoop *loop_; 
    const int fd_;    // fd, epoller监听的文件描述符
    int events_; // 注册fd感兴趣的事件
    int revents_; // epoller返回的发生的事件
    int state_; // 标识channel的三种状态：新事件、已添加、已删除


    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

};