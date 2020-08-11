#include "threadpool.h"


//template<typename T>
//threadpool<T>::threadpool(int threadnums, int max_request)
//    :m_thread_number(threadnums),m_max_request(max_request),m_stop(false)
//{
//    if(m_thread_number <= 0 || m_max_request <= 0)
//        throw std::exception();
//    for(int i=0;i<m_thread_number;i++)
//    {
//        m_works.push_back(std::thread(&threadpool::worker,this));
//        //分离的线程终止时，会自动释放资源，因此不用单独
//    }
//    for(int i=0;i<m_thread_number;i++)
//    {
//        m_works[i].detach(); //分离的线程终止时，会自动释放资源，因此不用单独
//    }
//}

//template<typename T>
//threadpool<T>::threadpool()
//{

//}

//template<typename T>
//threadpool<T>::~threadpool()
//{
////    m_stop = true;
//}

//template<typename T>
//bool threadpool<T>::append(T *request)
//{
//    lock_guard<std::mutex> lk(m_mutex);
//    if(m_tasks.size() >= m_max_request)
//    {

//        return false;
//    }
//    m_tasks.push(request);
//    m_cond.notify_one();
//    return true;
//}

//工作线程
//template<typename T>
//void threadpool<T>::worker()
//{
//    while (!m_stop) {
//        unique_lock<mutex> lk(m_mutex);
//        m_cond.wait(lk,[this]{return;});
//        T* request = m_tasks.front();
//        m_tasks.pop(); //弹出
//        lk.unlock();
//        if(!request)
//            continue;
//        request->process();
//    }
//}

