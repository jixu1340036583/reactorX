# distroChat

## 内容列表

- [背景](#背景)
- [前置模块讲解](#前置模块讲解)
- [设计思路以及代码结构](#设计思路以及代码结构)
- [系统的不足之处](#系统的不足之处)
- [项目中遇到的问题](#项目中遇到的问题)
- [安装说明](#安装说明)


## 背景
本项目是一个基于Reactor网络模型开发的网络库。它是一个高性能、跨平台的网络编程库，旨在提供稳定可靠的网络通信能力。通过使用该网络库，可以实现高并发、高效率的网络通信。但由于时间紧迫，目前只实现了服务器模块的开发，客户端模块还未实现。

**工作内容：**

+ 搭建了一个基于事件驱动的网络模型。它能够处理多个并发的TCP连接，包括连接的建立、断开、读写等操作。通过合理的连接管理，确保网络通信的稳定性和高效性。
+ 为了提高网络库的并发处理能力，集成了我自己实现的一个线程池。通过使用线程池，将每个网络连接的读写操作放入独立的线程中进行处理，从而避免了阻塞主线程的情况，并提高了网络库的整体性能。

+ 实现读写缓冲区。


## 前置模块讲解

线程池：[ThreadPool](https://github.com/jixu1340036583/ThreadPool)


## 设计思路以及代码结构

### 整体架构
网络库是基于reactor网络模型实现的，它是个高效的基于事件循环的网络模型，它由主反应堆，子反应堆、事件分发器、事件处理器组成；
- 主反应堆我定义为一个EventLoop对象，运行在主线程中，主反应堆单纯负责监听和分发客户端连接；
- 每个子反应堆也是一个EventLoop对象，单独运行在子线程中，并且在创建时需要将该事件循环对象的地址返回并保存到主反应堆的事件循环队列中；另外每个子反应堆还有一个用于线程间通信的Eventfd事件通知描述符；
- 事件分发器就是一个epoller 对象，它封装了 epoll 的三个行为。每一次事件循环都是基于Epoller来实现的，主要包括通过epoll_ctl向epoll实例中注册事件、通过epoll_wait返回活跃事件并处理；
- 当有新的客户端连接时，主反应堆会用轮询的方式从队列中得到一个子反应堆，然后将连接分发给它；子反应堆收到新连接后后，会将其注册到epoll实例中；当发生活跃事件时，epoller会将该事件返回给子反应堆，然后调用相应事件处理器来处理；
- 另外为了实现高效的io读写，我为每个连接都设置了读写缓存。
> **子反应堆如何返回主反应堆的？**
> 我是基于线程库中的future和promise实现的，将promise传入线程函数中，在ep对象构造完后通过promise保存其地址，然后主反应堆中通过future.get()获取。（不能将ep对象的指针作为线程函数的返回值，因为线程函数最终会直接执行loop()开启事件循环，没机会返回指针了。）
> **分发过程是怎么样的呢？**
> 这个分发的过程有**四点**：
>  - 1 每个eventloop对象在构造时会保存一下当前线程的id，用于确保每个子线程只运行一个事件循环；
>	- 2 每个子反应堆在创建时就会生成一个eventfd事件通知描述符，并注册到epoll实例中；
>	- 3 是当主反应堆准备分发连接时，轮询选中一个ep对象，首先通过线程id判断该ep是否运行在当前线程，如果是则直接执行回调；如果不是，那么就将该回调函数添加到该ep对象的任务队列中，然后向eventfd发送一个唤醒消息。收到唤醒消息的子反应堆就会从epoll_wait返回，然后在事件处理器中只需要读一下8字节的唤醒消息，然后在一次事件循环结束前执行一下任务队列中的回调。
>	- 4 这个回调的任务就是为新连接注册感兴趣的事件，同时注册到子反应堆的epoll实例中。之所以这样设计，是因为根据事件驱动模型的设计原则，主反应堆和子反应堆的职责划分应该清晰且明确，主反应堆通常只负责监听和分发连接，而对连接的处理都是在在子反应堆中执行的。这样有助于充分发挥多核处理器的并发能力。

### 用到的I\O模型

我是将同步非阻塞I\O和 IO多路复用结合使用的，原因有二：

+ 如果单一使用non-blocking，意味着我们需要为每个新连接都单独开一个线程，并通过轮询得来检查数据是否已经准备好，这会产生大量的上下文切换开销以及浪费CPU周期；

+ 如果单一采用同步阻塞I\O和IO多路复用就更不合适了，因为任意的I\O 操作都会阻塞当前线程，直到客户端断开连接，这期间无法处理其他其他的事件。
+ 因此，完美的做法就是将同步非阻塞I\O和 IO多路复用结合使用。这种方法允许线程同时监听多个I\O操作，只要至少有一个事件发生，线程才会被唤醒。并且处理完一个事件马上就会去处理下一个事件，不会发生阻塞。

### 采用边缘触发
reactorX采用的是**边缘触发方式**，理由如下：
- 首先肯定是因为边缘触发方式效率较高，每次epoll_wait就能读出所有数据，不必频繁在内核态和用户态之间切换；
- 其次是因为我才用了堆缓冲区和栈缓冲区结合的办法，可以保证一次就能将内核缓冲区的数据全部读出，所以不必使用水平触发方式。

### 为什么要有I\O缓冲？I\O输入输出缓冲是如何实现的√！1
**为什么要有I\O缓冲？**
因为I\O操作涉及到系统调用，而系统调用是有性能开销的，所以一次I\O操作的读写的数据量越多越好， 那么就可以在应用层设计一个I\O缓冲区来实现。

**如何实现**
- 对于一个I\O缓冲区来说，我们不能让它太大，因为每个连接都会有一个缓冲区，太大会浪费内存；太小的话又有可能一次读不完socket缓冲区中的数据。因此，我是用readv系统调用结合了缓冲区空间和栈空间，来应对单次读取数据量过大的问题。这样做就避免了开辟巨大 Buffer 造成的内存浪费，也避免了多次系统调用的开销。
- 缓冲区具体对应一个Buffer类，其内部使用字符数组实现，大小可以自动增长。Buffer中有两个关键的成员函数，分别是readFd()和writeFd：
	+ **当某个socket有可读事件发生时，会首先调用readfd()**: readFd()会在在栈上准备一小段空间，然后调用 readv() 来将socket缓冲区中的的数据**一次全部拷贝**到应用层接收缓冲区中；如果存不下，那么多出的数据就暂时拷贝到栈空间，再将应用层缓冲区扩容，然后再将栈空间的数据追加到接收缓冲区后面；**最后再将Buffer传递到用户的消息回调中**，让用户直接从应用层缓冲区中读取数据，而无需接触内核；
	+ **当用户需要发送数据时，会首先调用writefd()方法**：那么首先要判断一下输出缓冲区是否为空：
		+ 如果**当前输出缓冲区为空**：那么就将数据直接写到内核缓冲区中，然后根据write的返回值，来判断一次是否全部写入成功，如果没有全部写入，那么将剩余的数据写入到输出缓冲区中，并为当前的socket注册写事件；
		+ 如果**当前输出缓冲区不为空**，说明输出缓冲区中还有数据没有写到内核缓冲区中，因为TCP是基于字节流的因此要保持有序，所以要将新数据追加到缓冲区的后面，继续注册写事件。

#### 缓冲区大小调整策略
- Buffer的大小不是固定的，它是可以自动增长的，假设客户代码一次性写入 1000 字节，而当前可写的字节数只有 624，那么buffer会自动增长以容纳全部数据。
- 由于 vector 重新分配了内存，原来指向它元素的迭代器都会失效，这就是为什么 readIndex 和 writeIndex 是整数下标而不是迭代器。




## 系统的不足之处
+ ReactorX中的缓冲区用环形队列可能会更好，但是由于缓存区较大，deque在数据存取上的性能也未必比vector好。
+ 没有采用零拷贝。一是由于零拷贝编程比较复杂，还未掌握。二是由于我这个网络库主要是为了服务于分布式框架。而且常用的千兆以太网的裸吞吐量是125MB/s，而服务器内存的带宽至少是4GB/s，因此对于几十k大小的数据，在内存里拷贝几次的性能损失几乎可以忽略不计。



## 项目中遇到的问题




## 安装说明

开发环境：Ubuntu VsCode

编译器：g++

编译工具：CMake

编程语言：C++





