#include "server.h"

//定时器回调函数，他删除非活动链接socket上的注册事件
void cb_func(http_conn *user_data)
{
    epoll_ctl(http_conn::m_epollfd, EPOLL_CTL_DEL, user_data->get_sockfd(), 0);
    assert(user_data);

    close(user_data->get_sockfd());
    http_conn::m_user_count--;
    LOG_INFO("fd==[%d] closed by time cb_func \n",user_data->get_sockfd());


}

//信号处理函数
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(m_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}


//初始化
server::server(int port,int thread_number, int thread_task_number, int trig_model, int actor_model):
    m_port(port),
    m_thread_number(thread_number),
    m_thread_task_number(thread_task_number),
    m_stop_server(false), 
    m_close_log(0),
    m_is_block(1),
    m_trig_model(trig_model),
    m_actor_model(actor_model)
{

}

//初始化日志
void server::init_log()
{
    if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_is_block)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);//异步日志
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);//同步日志
    }
}


//释放堆区的空间
server::~server()
{

    close(m_pipefd[0]);
    close(m_pipefd[1]);
    close(m_epollfd);
    close(m_listenfd);
    //delete[] m_users_timer;
    delete[] users;
    delete m_pool;

}

// void server::init(int port,int thread_number = 10, int thread_task_number = 20, bool stop_server = false)
// {
//     m_port = port;
//     m_thread_number = thread_number;
//     m_thread_task_number = thread_task_number;
//     m_stop_server = stop_server;
//     m_timeout = false;
// }



//初始化线程池
bool server::create_threadpool()
{
    //创建线程池
    m_pool = NULL;
    try
    {
        m_pool = new threadpool<http_conn>(m_thread_number, m_thread_task_number, m_actor_model);
        LOG_INFO("创建线程池完毕\n");
    }
    catch(...)
    {
        LOG_ERROR("线程池错误！\n");
        return false;
    }

    return true;
}

//创建可用链接类数组，用于管理链接
void server::create_http_conn()
{
    //预先为每个可能的客户链接分配一个http_conn对象
    users = new http_conn[MAX_FD];
    assert(users);
    LOG_INFO("http_conn[]创建完成！\n");

}


//创建监听文件描述符
void server::create_listen()
{

    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));


    //设置本端socket
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);


    //设置端口复用
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    //绑定
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    //监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    LOG_INFO("监听创建成功！fd==[%d]\n",m_listenfd);


}

//创建epoll树，并将文件描述符上树
void server::create_epoll()
{

    //创建events数组用于接受events事件
    epoll_event events[MAX_EVENT_NUMBER];

    //创建epoll文件描述符：红黑树+双向链表
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    
    //将监听文件描述符上树
    addfd(m_epollfd, m_listenfd, false, m_trig_model);
    http_conn::m_epollfd = m_epollfd;
    LOG_INFO("epoll创建完成，listen上树成功！\n");
}


//创建定时器
void server::create_timer()
{
    //定时器相关
    int ret;
    
    //统一事件源：创建一个sock管道，将信号的触发写到一端，另一端使用epoll监听
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);//创建一对sock套接字，分别为pipefd[2]

    assert(ret != -1);
    this->setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false, m_trig_model);

    //设置信号处理函数

    //避免调用write写入已关闭的链接产生SIGPIPE信号中断进程
    addsig(SIGPIPE, SIG_IGN,true);

    //给以下两个时间触发信号绑定信号处理函数
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    
    //服务器停止运行标志
    bool m_stop_server = false;

    //创建客户数据数组，其中包含了客户对应的定时器信息
    //m_users_timer = new client_data[MAX_FD];

    //5秒钟之后触发ALARM信号->sig_headler->tick()清理过期链接
    alarm(TIMESLOT);
}

//接受一个链接
bool server::accpt_thing()
{
    struct sockaddr_in client_address;
    socklen_t clinet_addrlength = sizeof(client_address);
    int connfd;    


    //ET模式下要循环读取数据哦
    if(m_trig_model == 1)
    {
        while(1)
        {
            connfd = accept(m_listenfd, (struct sockaddr*)&client_address, & clinet_addrlength);
            LOG_DEBUG("建立链接fd==[%d]\n", connfd);
            if(connfd <= 0)
            {
                LOG_DEBUG("error is %d\n", errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD)
            {
                show_error(connfd,"Internet server busy");
                break;
            }
            client_init(connfd, client_address);
        }
        return false;
    }
    else
    {
        connfd = accept(m_listenfd, (struct sockaddr*)&client_address, & clinet_addrlength);
        LOG_DEBUG("建立链接fd==[%d]\n", connfd);
        if(connfd < 0)
        {
            LOG_DEBUG("error is %d\n", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD)
        {
            show_error(connfd,"Internet server busy");
            return false;
        }
        client_init(connfd, client_address);
    }



    return true;//LT模式返回true表示流程继续，ET模式读完可以去读下一个

}

void server::client_init(int connfd, struct sockaddr_in client_address)
{
    //初始化客户端链接
    users[connfd].init(connfd, client_address, m_trig_model, &m_timer_list);

    //把链接记录到客户数据上，以便计时器关闭非活跃链接
    //m_users_timer[connfd].address = client_address;
    //m_users_timer[connfd].sockfd = connfd;

    //创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_list中
    util_timer * timer = new util_timer;
    timer->user_data = &users[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;//超时时间为15s，15无连接则关闭链接
    users[connfd].set_timer(timer);
    m_timer_list.add_timer(timer);
}

//到期关闭链接，也可以用于其他情况关闭链接
void server::timer_over(int sockfd)
{
    util_timer *timer = users[sockfd].get_timer();

    //执行回调函数，负责关闭链接
    timer->cb_func(&users[sockfd]);

    //操作指针前先判空
    if (timer)
    {
        m_timer_list.del_timer(timer);
    }
}

//处理信号
bool server::time_signal()
{
    int ret;
    int sig;

    //信号数组，用于接受信号
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(-1 == ret)
    {
        LOG_ERROR("从管道中接受信号失败，接受失败！！\n");
        return false;
    }
    else if(ret == 0)
    {
        LOG_ERROR("从管道中接受信号失败，管道中无数据！\n");
        return false;
    }
    else 
    {
        for(int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
                case SIGALRM:
                {
                    //用timeout变量标记有定时任务需要处理，但不立即处理定时任务，这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务
                    m_timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    LOG_ERROR("SIGTERM信号触发，退出进程！\n");
                    m_stop_server = true;
                }
                
            }
        }
    }
}


//处理读事件
void server::read_thing(int sockfd)
{ 
    LOG_DEBUG("read_thing\n");
    util_timer *timer = users[sockfd].get_timer();
    

    if(m_actor_model == 1)
    {

        updata_time(sockfd);
        //交给线程池处理
        users[sockfd].set_read();
        m_pool->append(users + sockfd);
        
    }
    else
    {
        //proactor模式，读写操作都在主线程，分析请求在工作线程
        if(users[sockfd].read())
        {
            //直接吧整个http_conn都传进去，不过传的都是地址哈，+sockfd就是传当前sock所在的对象呀
            m_pool->append(users + sockfd);

            if(timer)
            {
                m_timer_list.adjust_timer(timer);
            }
            //更新计时时间
            updata_time(sockfd);
        }
        else
        {
            LOG_ERROR("read()出错关闭连接！\n");
            timer->cb_func(&users[sockfd]);
            if (timer)
            {
                m_timer_list.del_timer(timer);
            }

        }
    }

}

//写事件
void server::write_thing(int sockfd)
{
    LOG_DEBUG("write_thing\n");

    if(m_actor_model == 1)
    {
        updata_time(sockfd);
        users[sockfd].set_write();
        m_pool->append(users + sockfd);

    }
    else
    {
        if(users[sockfd].write())
        {
            //写入成功，更新为活跃链接
            updata_time(sockfd);
        }
        else 
        {
            LOG_ERROR("write_thing error and close fd = [%d]\n",sockfd);
            timer_over(sockfd);
        }
    }

}


//主循环
void server::main_loop()
{
    LOG_INFO("main_loop!\n");
    m_timeout = false;
    m_stop_server = false;
    while(!m_stop_server)
    {
        LOG_DEBUG("main_loop!\n");
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            printf("epoll failure!\n");
            break;
        }

        //处理事件
        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;

            //建立新链接
            if(sockfd == m_listenfd)
            {
                bool flag = accpt_thing();
                if(false == flag)
                {
                    continue;
                }
            }


            //error事件
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                LOG_INFO("fd=[%d] 监听到：", sockfd);
                if(events[i].events & EPOLLRDHUP) LOG_INFO("EPOLLRDHUP事件");
                if(events[i].events & EPOLLHUP) LOG_INFO("EPOLLHUP事件");
                if(events[i].events & EPOLLERR) LOG_INFO("EPOLLERR事件");
                LOG_INFO("\n");

                //如果有异常直接关闭链接
                timer_over(sockfd);
            }


            //一旦信号触发，触发sig_headle函数往pipe[1]中写入数据，此时可以触发epoll
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                //LOG_INFO("signal事件:\n");
                int flag = time_signal();
                if(false == flag)
                {
                    continue;
                }
            }


            else if(events[i].events & EPOLLIN)
            {
                LOG_INFO("epollin事件:\n");
                read_thing(sockfd);
            }

            else if(events[i].events & EPOLLOUT)
            {
                //创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_list中
                LOG_INFO("写事件\n");
                write_thing(sockfd);

            }
            if(m_timeout)
            {
                //LOG_INFO("计时事件：\n");
                timer_handler();
                m_timeout = false;
            }
        }
    }
}


//更新sockfd上的计时器
void server::updata_time(int sockfd)
{
    LOG_INFO("更新为活跃连接fd=[%d]\n", sockfd);
    util_timer * timer = users[sockfd].get_timer();
    if(timer)
    {
        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        //printf("adjust timer by write!\n");
        m_timer_list.adjust_timer(timer);
    }
}



void server::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction ac;
    memset(&ac, '\0', sizeof(ac));
    ac.sa_handler = handler;
    if(restart)
    {
        ac.sa_flags |= SA_RESTART;//中断系统调用时，返回的时候重新执行该系统调用
    }
    sigfillset(&ac.sa_mask);//sa_mask中的信号只有在该ac指向的信号处理函数执行的时候才会屏蔽的信号集合!
    assert(sigaction(sig, &ac, NULL) != -1);
}

//显示错误，关闭该链接的文件描述符
void server::show_error(int connfd, const char * info)
{
    printf("%s",info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

//设置非阻塞IO
int server::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}




void server::timer_handler()
{
    //LOG_DEBUG("处理定时任务\n");
    //定时处理任务，实际上就是调用tick函数
    m_timer_list.tick();
    //因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信号
    alarm(TIMESLOT);
}

