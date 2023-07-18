#include "Logger.h"
#include "Timestamp.h"
#include <thread>
#include <iostream>

std::string levelstr(LogLevel level){
    switch (level)
    {
    case INFO:
        return "INFO"; 
        break;
    case DEBUG:
        return "DEBUG"; 
        break;
    case ERROR:
        return "ERROR"; 
        break;
    case FATAL:
        return "FATAL"; 
        break;
    default:
    return "";
        break;
    }
}

// 获取日志唯一的实例对象
Logger& Logger::getInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger(){
    // 启动专门的写日志线程
#ifdef WRITELOG
    std::thread writeLogTask([&](){
        while(1){
            // 获取当前的日期，然后取日志信息，写入相应的日志文件中 a+
            time_t now = time(nullptr);
            tm* nowtm = localtime(&now);
            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year+1900, nowtm->tm_mon+1, nowtm->tm_mday);
            FILE *fp = fopen(file_name, "a+");
            if(fp == nullptr){
                std::cout << "logger file: " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }
            std::string msg = m_lckQue.Pop();
            char time_buf[128] = {0};
            sprintf(time_buf, "%d:%d:%d =>[%s] ", nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec, levelstr(m_loglevel).c_str());
            msg.insert(0, time_buf);
            msg.append("\n");
            fputs(msg.c_str(), fp);
            fclose(fp);
        }
    });
    // 设置分离线程，守护线程
    writeLogTask.detach();
#endif
}

// 设置日志级别
void Logger::setLogLevel(LogLevel level)
{
    m_loglevel = level;
}


// 写日志  [级别信息] time : msg
void Logger::log(std::string&& msg)
{  
    std::cout << Timestamp::now().toString() << " ";
    switch (m_loglevel)
    {
    case INFO:
        std::cout << "[INFO] ";
        break;
    case ERROR:
        std::cout << "[ERROR] ";
        break;
    case FATAL:
        std::cout << "[FATAL] ";
        break;
    case DEBUG:
        std::cout << "[DEBUG] ";
        break;
    default:
        break;
    }

    // 打印时间和msg
    std::cout << msg << std::endl;
    m_lckQue.Push(msg);
}
