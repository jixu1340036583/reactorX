#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <list>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>

#include <chrono>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>


#include "Logger.h"

namespace tp{
using PoolSeconds = std::chrono::seconds;

/*
    * core_threads:核心线程数，线程池中最少线程数，也是初始化时创建的线程数量
    * max_threads: 最大线程数 当任务数太多时，会创建更多线程，但数量不会超过 max_threads
    * max_task_size: 最大任务个数
    * time_out： 当time_out时间内核心线程以外的线程没有执行任务的话，该线程会被回收
*/
struct ThreadPoolConfig {
    unsigned int core_threads;
    unsigned int max_threads; 
    unsigned int max_task_size;
    PoolSeconds time_out;
};

class ThreadPool
{
public:
    // 线程的四种状态：初始化、等待、运行、停止
    enum class ThreadState { kInit = 0, kWaiting = 1, kRunning = 2, kStop = 3 };

    // 线程的种类标识：标志该线程是核心线程还是Cache线程，Cache线程是因为任务过多线程池内部临时创建的线程
    enum class ThreadFlag { kCore = 0, kCache = 1 };

    using ThreadPtr = std::shared_ptr<std::thread>;
    using ThreadId = std::atomic<int>;

    using ThreadStateAtomic = std::atomic<ThreadState>;
    using ThreadFlagAtomic = std::atomic<ThreadFlag>;

    /// @brief 包装一个线程，内部包含指向线程的智能指针、线程id、线程类型、线程状态
    struct ThreadWrapper {
        ThreadPtr ptr;
        ThreadId id;
        ThreadFlagAtomic flag;
        ThreadStateAtomic state;

        ThreadWrapper():ptr(nullptr), id(0), state(ThreadState::kInit) { }
    };

    using ThreadWrapperPtr = std::shared_ptr<ThreadWrapper>;
    using ThreadPoolLock = std::unique_lock<std::mutex>;

    class join_threads{
        private:
            std::unordered_map<int, ThreadWrapperPtr>& threads;
        public:
            // 这里的形参不小心定义成了非引用，也就是ThreadPool构造时，worker_threads_还为空，导致没能执行join()
            explicit join_threads(std::unordered_map<int, ThreadWrapperPtr>& _threads):threads(_threads){}
            ~join_threads(){
                for(auto &t: threads){
                    if((t.second)->ptr->joinable())
                        (t.second)->ptr->join();
                }
            }
    };

    ThreadPool(ThreadPoolConfig config);
    // 析构函数：终止线程池，但如果队列中有任务将任务执行完
    ~ThreadPool() { 
        ShutDown(); 
    }

    /// @brief 判断配置给线程池的参数是否有效
    /// @param config 线程池参数
    /// @return 有效则返回true，反之返回false
    bool IsValidConfig(ThreadPoolConfig config);

    /// @brief 启动线程池
    /// @return 启动失败返回false
    bool Start();

    /// @brief 获取下一个可用的线程id
    /// @return 下一个可用的线程id
    int GetNextThreadId() { return this->thread_id_++; }

    void AddThread(int id, ThreadFlag thread_flag);

    void ShutDown(bool is_now);

    // 关闭线程池，如果任务队列中还有任务则等待
    void ShutDown() {
        ShutDown(false);
        LOG_DEBUG("shutdown!");
    }

    // 不管任务队列是否为空，立即关闭
    void ShutDownNow() {
        ShutDown(true);
        LOG_DEBUG("shutdown now!");
    }


    // 工作线程
    void work_thread(ThreadWrapperPtr thread_ptr);
    

    /// @brief 重置线程池大小
    /// @param thread_num 
    void Resize(int thread_num);

    /// @brief 向线程池提交任务
    /// @tparam F 可调用对象类型
    /// @tparam ...Args 可调用对象的可变实参类型
    /// @param f 可调用对象
    /// @param ...args 可调用对象的实参
    /// @return 
    template <typename F, typename... Args>
    auto Run(F &&f, Args &&... args) -> std::shared_ptr<std::future<std::result_of_t<F(Args...)>>>;

    /// @brief 当前线程池是否可用
    bool IsAvailable() { return is_available_.load(); }

    // 获取正在处于等待状态的线程的个数
    int GetWaitingThreadSize() { return this->waiting_thread_num_.load(); }

    // 获取线程池中当前线程的总个数
    int GetTotalThreadSize() { return this->worker_threads_.size(); }

    int getTotalTaskNum() {return total_function_num_; }

private:
    /// @brief 线程池配置参数
    ThreadPoolConfig config_;
    /// @brief 线程链表
    std::unordered_map<int, ThreadWrapperPtr> worker_threads_;

    std::mutex wk_mutex_; // 工作线程互斥锁
    /// @brief 任务队列
    std::queue<std::function<void()>> tasks_;
    std::mutex task_mutex_; // 任务队列锁
    std::condition_variable task_cv_;

    std::atomic<int> total_function_num_; // 执行总任务数
    std::atomic<int> waiting_thread_num_; // 处于等待状态的线程数量
    std::atomic<int> thread_id_; // 用于为新创建的线程分配ID

    std::atomic<bool> is_shutdown_now_;
    std::atomic<bool> is_shutdown_;
    std::atomic<bool> is_available_; // 线程池是否可用
    join_threads joiners; // 用于join 

};


template <typename F, typename... Args>
auto ThreadPool::Run(F &&f, Args &&... args) -> std::shared_ptr<std::future<std::result_of_t<F(Args...)>>> {
    if (this->is_shutdown_.load() || this->is_shutdown_now_.load() || !IsAvailable()) {
        return nullptr;
    }
    // 如果等待线程数量为0且当前线程数量小于最大线程数量，那就增加一个临时线程
    if (GetWaitingThreadSize() == 0 && GetTotalThreadSize() < config_.max_threads) {
        AddThread(GetNextThreadId(), ThreadFlag::kCache);
    }

    using return_type = std::result_of_t<F(Args...)>; // 首先我们得到传入可调用对象的返回类型

    auto task = std::make_shared<std::packaged_task<return_type()>>(  // 用package_task包装一个无参数的task
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)); // 传入bind包装好的无参数的可调用对象
    ++total_function_num_; 

    std::future<return_type> res = task->get_future(); // 保存好future
    {
        ThreadPoolLock lock(this->task_mutex_);
        this->tasks_.emplace([task]() { (*task)(); }); // ？ 应该是想调用移动版本
    }
    this->task_cv_.notify_one(); // 唤醒一个等待任务的线程
    return std::make_shared<std::future<std::result_of_t<F(Args...)>>>(std::move(res)); // 将前面的future移动到make_shared内的堆内存中
}


};

#endif