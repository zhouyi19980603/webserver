#ifndef LOG_H
#define LOG_H

#include <iostream>
#include "block_queue.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <stdarg.h>

using namespace std;

class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void flush_log_thread()
    {
        Log::get_instance()->async_write_log();
    }

    void write_log(int level, const char *format, ...);

    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void flush();
private:
    Log();
    virtual ~Log();
    void async_write_log()
    {
        string str="";
        while (_deque->try_pop(str)) {
            lock_guard<mutex> locker(_mtx);
            fputs(str.c_str(),m_fp);
        }

    }
    std::unique_ptr<threadsafe_queue<std::string>> _deque;
    std::unique_ptr<std::thread> _writeThread;
    std::mutex _mtx;
    FILE* m_fp;

    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天

    char *m_buf;
    bool m_is_async;
    int m_close_log; //关闭日志
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif // LOG_H
