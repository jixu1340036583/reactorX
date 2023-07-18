#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

// #define MyDEBUG
//#define WRITELOG

// 异步写日志队列
template<typename T>
class LockQueue{
public:
    void Push(const T& data){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(data);
        m_cv.notify_one();
    }
    T Pop(){
        std::unique_lock<std::mutex> lock(m_mutex);
        while(m_queue.empty()){
            // 日志队列为空，线程进入wait状态
            m_cv.wait(lock);
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }
private:
    std::queue<T> m_queue;
    std::mutex m_mutex; // 拿到锁的线程才能读或写日志
    std::condition_variable m_cv;
};

// Mprpc框架提供的日志系统
enum LogLevel{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core信息
    DEBUG, // 调试信息
};

class Logger{
public:
    // 设置成单例模式
    static Logger& getInstance();
    // 设置日志级别
    void setLogLevel(LogLevel level);
    // 写日志
    void log(std::string&& msg);
private:
    LogLevel m_loglevel; // 记录日志级别
    LockQueue<std::string> m_lckQue; // 日志缓冲队列
    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
};

// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        std::string str = "(" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ") -> " + std::string(buf);\
        logger.log(std::move(str)); \
    } while(0) 

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        std::string str = "(" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ") -> " + std::string(buf);\
        logger.log(std::move(str)); \
    } while(0) 

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        std::string str = "(" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ") -> " + std::string(buf);\
        logger.log(std::move(str)); \
        exit(-1); \
    } while(0) 

#ifdef MyDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        std::string str = "(" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ") -> " + std::string(buf);\
        logger.log(std::move(str)); \
    } while(0) 
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif
