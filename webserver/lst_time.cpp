#include "lst_time.h"



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
	if(!temp || (timer->expire < temp->expire))
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
        add_timer(timer,timer->next);

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

