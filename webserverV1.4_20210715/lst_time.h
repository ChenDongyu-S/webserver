/*************************************************************************
	> File Name: lst_time.h
	> Author: 
	> Mail: 
	> Created Time: Fri 09 Jul 2021 11:49:19 AM CST
 ************************************************************************/

#ifndef _LST_TIME_H
#define _LST_TIME_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>


#include "log.h"
#include "http_conn.h"
#define BUFFER_SIZE 64

class util_timer;//向前声明
class http_conn;


//客户端链接的数据，每一个客户端练上来都被记录着fd和定时器
// struct client_data
// {
//     sockaddr_in address;
//     int sockfd;
//     //char buf[BUFFER_SIZE];
//     util_timer * timer;

// };

//定时器类
class util_timer
{
public:
    util_timer():prev(NULL),next(NULL){}

public:
    time_t expire;//任务的超时时间，这里使用绝对时间
    void (*cb_func)(http_conn* );//任务回调函数，回调函数处理的客户数据，由定时器的执行者传递给回调函数
    http_conn *user_data;
    util_timer *prev;//指向前一个定时器
    util_timer *next;//指向后一个定时器    

};

//定时器链表。他是一个升序、双向链表，且带有头节点和尾节点
class sort_timer_lst
{
public:

    sort_timer_lst():head(NULL),tail(NULL) {}//初始化
    
    ~sort_timer_lst();//链表被销毁时，删除其中所有节点

    void add_timer(util_timer * timer);//按顺序添加计时器

    void adjust_timer(util_timer * timer); //当某一个任务发生变换时，应该调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的超过时间延长的情况，即该定时器需要往尾部移动

    void del_timer(util_timer* timer); //将目标定时器timer从链表中删除

    /*也就是遍历链表，超时的全部执行回调函数*/
    void tick();//SIGALRM信号每次被触发就在其信号处理函数（如果使用统一事件源，那就是主函数）中执行一次tick函数，已处理链表上到期的任务


private:
    //一个重载的辅助函数,当add_timer不能完成时，会调用该函数进行添加
    void add_timer(util_timer* timer, util_timer* lst_head);

private:
    util_timer* head;
    util_timer* tail;

};


#endif
