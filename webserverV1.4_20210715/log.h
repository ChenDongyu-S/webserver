/*************************************************************************
	> File Name: log.h
	> Author: 
	> Mail: 
	> Created Time: Fri 13 Jul 2021 15:11:17 PM CST
 ************************************************************************/

#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log
{
public:

    //创建唯一实例，并获取之
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }

    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_line = 5000000, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

private:

    Log();
    virtual ~Log();
    void *async_write_log()
    {
        string single_log;
        //从阻塞队列取出一个日志文件string，写入文件
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }


private:
    char dir_name[128];//路径名
    char log_name[128];//log文件名
    int m_split_lines;//日志最大行数
    int m_log_buf_size;//日志缓冲区大小
    long long m_count;//日志行数记录
    int m_today;//日期
    FILE *m_fp;//日志文件指针
    char *m_buf;
    block_queue<string> *m_log_queue;//阻塞队列
    bool m_is_async;//同步吗
    locker m_mutex;
    int m_close_log;
};


#define LOG_DEBUG(format, ...) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__);Log::get_instance()->flush();}
#define LOG_INFO(format, ...) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__);Log::get_instance()->flush();}
#define LOG_WARN(format, ...) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__);Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__);Log::get_instance()->flush();}


#endif
