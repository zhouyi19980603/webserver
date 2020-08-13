#include "log.h"
#include <cstring>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

Log::Log()
{
    m_count = 0;
    m_is_async = false;
    _writeThread = nullptr;
    _deque = nullptr;
    m_today = 0;
    m_fp = nullptr;
}

Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}


bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
//    if(max_queue_size > 0)
//    {
        m_is_async = true;
        unique_ptr<threadsafe_queue<std::string>> newdeque(new threadsafe_queue<std::string>);
        _deque = move(newdeque);
        std::unique_ptr<std::thread> newthread(new thread (flush_log_thread));
        _writeThread = move(newthread);
//    }else
//    {
//        m_is_async = false;
//    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char [m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;


    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0}; //完整的路径名

    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

//    cout<<"log_full_name"<<log_full_name<<endl;
    m_fp = fopen(log_full_name, "a"); //生成并模式设为追加每次添加在文件末位
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    cout<<"进来了"<<endl;
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    unique_lock<mutex> locker(_mtx);
//    m_mutex.lock();
    m_count++;

    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {

        char new_log[256] = {0};
        fflush(m_fp);
//        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    locker.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    locker.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
//    cout<<"m_buf: "<<m_buf<<endl;

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    locker.unlock();

    if (m_is_async && !_deque->full())
    {
        _deque->push(log_str);
    }
    else
    {
        unique_lock<mutex> locker1(_mtx);
        fputs(log_str.c_str(), m_fp);
        locker1.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    lock_guard<mutex> locker(_mtx);
    //强制刷新写入流缓冲区
    fflush(m_fp);

}
