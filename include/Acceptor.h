#pragma once

#include <functional>
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

namespace rx{

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) 
    {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();
    
    EventLoop *loop_; // 主反应堆事件循环，监听的就是当前的监听socket
    Socket acceptSocket_; // 监听socket
    Channel acceptChannel_; // 主反应堆监听的通道
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};


};