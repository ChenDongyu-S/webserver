/*************************************************************************
	> File Name: lst_time.h
	> Author: 
	> Mail: 
	> Created Time: Fri 09 Jul 2021 11:49:19 AM CST
 ************************************************************************/

#ifndef _LST_TIME_H
#define _LST_TIME_H

#include<time.h>

#define BUFFER_SIZE 64

class util_timer;//向前声明

//客户端链接的数据，每一个客户端练上来都被记录着fd和定时器
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer * timer;

};

//定时器类
class util_timer
{
public:
    util_timer():prev(NULL),next(NULL){}

public:
    time_t expire;//任务的超时时间，这里使用绝对时间
    void (*cb_func)(client_data*);//任务回调函数，回调函数处理的客户数据，由定时器的执行者传递给回调函数
    client_data * user_data;
    util_timer* prev;//指向前一个定时器
    util_timer* next;//指向后一个定时器    

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

sort_timer_lst::~sort_timer_lst()
{
    util_timer* temp = head;
    while(temp)
    {
        head = head->next;
        delete temp;
        temp = head;
    }
}


void sort_timer_lst::add_timer(util_timer * timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head = tail = timer;
        return;
    }
    //为了链表的有序性；我们根据目标定时器的超过时间来排序，（按照超时时间从小到大排序）如果超时时间小于head，成为head，负责根据顺序插入
    if(timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    add_timer(timer,head);//将timer插入head中,这是一个重载函数，注意参数！
}


void sort_timer_lst::adjust_timer(util_timer * timer)
{
	if(!timer)
	{
		return;
	}
	util_timer * temp = timer->next;
	//不用调整的情况：1.处于尾部。2.虽有变化，但不影响排位
	if(!temp || timer->expire < temp->expire)
	{
		return;
	}
	//如果为头部节点，最好的时间复杂度是取出来重新插入
	if(timer == head)
	{
		head = head->next;
		head->prev = NULL;
		timer->next = NULL;
		add_timer(timer,head);
    }
    else//不为头部，也可以取出来插入！
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,head);

    }
}

void sort_timer_lst::del_timer(util_timer* timer)
{
	if(!timer)
	{
		return;
	}
	//下面这个条件成立表示链表中只有一个定时器，即目标定时器
	if((timer == head) && (timer == tail))
	{
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}
	//如果链表中至少有两个定时器，且目标定时器是链表的头结点，则将链表的头结点重置为源节点的下一个节点
	if(timer == head)
	{
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}
	//如果链表中至少有两个定时器，且目标节点是链表的尾节点
	if(timer == tail)
	{
		tail = tail->prev;
		tail->next = NULL;
		delete timer;
		return;
	}
	//如果链表至少有三个定时器，并且目标节点是在链表的中间位置
	else
	{
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		delete timer;
		return;
	}
	
}


void sort_timer_lst::tick()
{
	if(!head)
	{
		return;
	}
	printf("timer tick !\n");
	time_t cur = time(NULL);//获取系统当前时间
	util_timer* temp = head;

	//从头节点开始一次处理每一个定时器，直到遇到一个尚未到期的定时器，这就是定时器的核心逻辑
	//说白了就是遍历

	while(temp)
	{
		//因为每个定时器都是用绝对时间作为超时值，所以我们可以吧定时器的超时值和系统当前时间，比较一判断定时器是否到期
		if(cur < temp->expire)
		{
			break;
		}
		//调用定时器的回调函数，已执行定时任务
		temp->cb_func(temp->user_data);
		//执行完定时任务以后，就将它从链表中删除，并且重置链表头节点
		head = temp->next;
		//链表操作的时候一定一定要注意！先检查是否为空
		if(head)
		{
			head->prev = NULL;
		}
		delete temp;
		temp = head;
	}

}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
	util_timer* prev = lst_head;
	util_timer* temp = prev->next;
	//遍历head之后的所有节点，知道找到一个节点的值大于timer的位置插入
	while (temp)
	{
		if(timer->expire < temp->expire)
		{
			prev->next = timer;
			timer->next = temp;
			temp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = temp;
		temp = temp->next;
	}
	//如果遍历完还没有找到，就直接插到尾节点
	if(!temp)
	{
		prev->next = timer;
		timer->prev = prev;
		timer->next = NULL;
		tail = timer;
	}
	
}


#endif
