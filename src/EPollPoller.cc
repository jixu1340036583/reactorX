#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>
namespace rx{


EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)  // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller() 
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{

    LOG_DEBUG("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_DEBUG("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}


// 
void EPollPoller::updateChannel(Channel *channel)
{
    const int state = channel->state();
    LOG_DEBUG("func=%s => fd=%d events=%d state=%d \n", __FUNCTION__, channel->fd(), channel->events(), state);

    if (state == New || state == Deleted)
    {
        if (state == New)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_state(Added);
        updateChannel(EPOLL_CTL_ADD, channel);
    }
    else  // state是Added状态
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 如果没有关注事件，那么就是注销该通道
        {
            updateChannel(EPOLL_CTL_DEL, channel);
            channel->set_state(Deleted);
        }
        else
        {
            updateChannel(EPOLL_CTL_MOD, channel); // 否则就是修改关注的事件
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_DEBUG("func=%s => fd=%d\n", __FUNCTION__, fd);
    
    int state = channel->state();
    if (state == Added) updateChannel(EPOLL_CTL_DEL, channel);
    
    channel->set_state(Deleted);
}

// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::updateChannel(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    
    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd; 
    event.data.ptr = channel;
    
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}



// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i=0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}


};