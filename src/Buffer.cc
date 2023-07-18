#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
namespace rx{
/**
 * 从fd上读取数据  Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小
 */ 
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间  64K
    
    struct iovec vec[2];
    
    const size_t writable = writableBytes(); 
    vec[0].iov_base = begin() + writerIndex_; // 从可写的位置开始写
    vec[0].iov_len = writable; // 可写的长度

    vec[1].iov_base = extrabuf; // 栈空间起始地址
    vec[1].iov_len = sizeof extrabuf; // 栈空间大小
    
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经足够存储读出来的数据
    {
        writerIndex_ += n; // 更新writerIndex_
    }
    else // extrabuf里面也写入了数据 
    {
        writerIndex_ = buffer_.size();
        // 扩容缓存，从writerIndex_开始写extrabuf中缓存的数据
        append(extrabuf, n - writable);  
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}
};