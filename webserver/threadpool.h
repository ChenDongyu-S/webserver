#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
#include "log.h"
#include "lst_time.h"

class util_timer;
class sort_timer_lst;
//线程池类，将他定义为模板类是为了代码复用
template<typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool( int thread_number, int max_request, int m_actor_model);
    ~threadpool();
    bool append(T *request);   

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
    int m_actor_model;          //反应堆模式 
};
template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests, int actor_model) : 
m_thread_number(thread_number), 
m_max_requests(max_requests), 
m_stop(false),m_threads(NULL),
m_actor_model(actor_model)
{
    LOG_INFO("初始化线程池！\n");
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    LOG_INFO("创建线程\n");
    m_threads = new pthread_t[m_thread_number];
    LOG_INFO("判断线程\n");
    if (!m_threads)
        throw std::exception();

    LOG_INFO("循坏创建线程\n");
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
    LOG_INFO("循坏创建线程结束\n");
}
template <typename T>
threadpool<T>::~threadpool()
{
    LOG_INFO("线程池析构！\n");
    delete[] m_threads;
}
template <typename T>
bool threadpool<T>::append(T *request)
{
    LOG_INFO("append()\n");
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template <typename T>
void *threadpool<T>::worker(void *arg)
{

    threadpool *pool = (threadpool *)arg;
    pool->run();
    
    return pool;
}
template <typename T>
void threadpool<T>::run()
{

    while(true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T * request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
        {
            continue;
        }

        if(m_actor_model == 1)
        {
            if(request->get_rw_state() == true)
            {
                if(request->read())
                {
                    //只有读取成功需要这个
                    LOG_INFO("通过reactor模式读取成功\n");
                    request->process();
                }
                else
                {
                    LOG_ERROR("write_thing error and close fd\n");
                    util_timer *timer = request->get_timer();

                    //执行回调函数，负责关闭链接
                    timer->cb_func(request);

                    //操作指针前先判空
                    if (timer)
                    {
                        request->get_timer_list()->del_timer(timer);
                    }
                }
            }
            else
            {
                if(request->write())
                {
                    
                }
                else
                {
                    LOG_ERROR("write_thing error and close fd\n");
                    util_timer *timer = request->get_timer();

                    //执行回调函数，负责关闭链接
                    timer->cb_func(request);

                    //操作指针前先判空
                    if (timer)
                    {
                        request->get_timer_list()->del_timer(timer);
                    }
                }
            }
        }
        else
        {
            request->process();
        }


        
    }
}

#endif
