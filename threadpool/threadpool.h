#ifndef threadpool_H
#define threadpool_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <functional>
#include <atomic>
#include "httpconn/http_conn.h"


using namespace std;

template<typename T>
class threadpool
{
public:
    threadpool(int threadnums=8,int max_request=9000)    :m_thread_number(threadnums),m_max_request(max_request),m_stop(false)
    {
        if(m_thread_number <= 0 || m_max_request <= 0)
            throw std::exception();
        for(int i=0;i<m_thread_number;i++)
        {
            m_works.emplace_back(std::thread(&threadpool::worker, this));
            //分离的线程终止时，会自动释放资源，因此不用单独
        }
        for(int i=0;i<m_thread_number;i++)
        {
            m_works[i].detach(); //分离的线程终止时，会自动释放资源，因此不用单独
        }
    }

    threadpool(const threadpool&) = delete ;
    threadpool& operator=(const threadpool& other) = delete ;
    ~threadpool(){
        m_stop = true;
    }
    bool append(T* request)
    {
        lock_guard<std::mutex> lk(m_mutex);
        if(m_tasks.size() >= m_max_request)
        {
            return false;
        }
        m_tasks.push(request);
        m_cond.notify_one();
        return true;
    }


private:
    void worker()
    {
        while (!m_stop) {
            cout<<"我准备开始工作了"<<endl;
            unique_lock<mutex> lk(m_mutex);
            m_cond.wait(lk);
            cout<<"我已经开始工作了"<<endl;
            T* request = m_tasks.front();
            m_tasks.pop(); //弹出
            lk.unlock();
            if(!request)
                continue;
            request->process();
        }
    }
private:
    int m_thread_number;
    int m_max_request;
    T x;
    std::mutex m_mutex;
    condition_variable m_cond;
    vector<thread> m_works;
    queue<T *> m_tasks;
    atomic_bool m_stop;

};


#endif // threadpool_H
