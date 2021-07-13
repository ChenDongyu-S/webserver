#include "server.h"

//定时器回调函数，他删除非活动链接socket上的注册事件
void cb_func(client_data* user_data)
{
    epoll_ctl(http_conn::m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    close(user_data->sockfd);
    http_conn::m_user_count--;
    printf("close fd [%d] by time cb_func \n",user_data->sockfd);
    printf("\n");
    printf("\n");
    printf("\n");

}

//信号处理函数
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(m_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}










server::server(int port,int thread_number = 10, int thread_task_number = 10000):
m_port(port),m_thread_number(thread_number),m_thread_task_number(thread_task_number),
m_stop_server(false), m_close_log(0), m_is_block(1)
{

}

//初始化日志
void server::init_log()
{
    if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_is_block)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}



server::~server()
{

    close(m_pipefd[0]);
    close(m_pipefd[1]);
    close(m_epollfd);
    close(m_listenfd);
    delete[] m_users_timer;
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
        m_pool = new threadpool<http_conn>();
        LOG_DEBUG("创建线程new完毕\n");
    }
    catch(...)
    {
        LOG_DEBUG("catch\n");
        return false;
    }
    LOG_DEBUG("c-th 完毕\n");
    return true;
}

void server::create_http_conn()
{
    //预先为每个可能的客户链接分配一个http_conn对象
    users = new http_conn[MAX_FD];
    assert(users);
    int users_conut = 0;
}

void server::create_listen()
{

    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    //优雅关闭
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);


    //设置端口复用
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(m_listenfd, 5);
    assert(ret >= 0);



}

void server::create_epoll()
{

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    addfd(m_epollfd, m_listenfd, false);
    http_conn::m_epollfd = m_epollfd;




}

void server::create_timer()
{
    //定时器相关
    int ret;
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);//创建一对sock套接字，分别为pipefd[2]
    assert(ret != -1);
    this->setnonblocking(m_pipefd[1]);
    addfd(m_epollfd, m_pipefd[0], false);

    //设置信号处理函数
    addsig(SIGPIPE, SIG_IGN,true);
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    bool m_stop_server = false;

    m_users_timer = new client_data[MAX_FD];

    alarm(TIMESLOT);
}


bool server::accpt_thing()
{
    struct sockaddr_in client_address;
    socklen_t clinet_addrlength = sizeof(client_address);
    //printf("wait listen accpt!\n");
    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, & clinet_addrlength);


    // struct linger tmp = {1, 1};
    // setsockopt(connfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));


    // //设置端口复用
    // int flag = 1;
    // setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));



    LOG_DEBUG("---------------------b拉起来的connfd == [%d]--------------\n",connfd);
    //printf("get listen accpt!\n");
    LOG_DEBUG("\n");

    if(connfd < 0)
    {
        LOG_DEBUG("error is %d\n",errno);
        return false;
    }
    if(http_conn::m_user_count >= MAX_FD)
    {
        show_error(connfd,"Internet server busy");
        return false;
    }
    //初始化客户端链接
    users[connfd].init(connfd, client_address);


    m_users_timer[connfd].address = client_address;
    m_users_timer[connfd].sockfd = connfd;
    //创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_list中
    util_timer * timer = new util_timer;
    timer->user_data = &m_users_timer[connfd];
    timer->cb_func = cb_func;
    //timer->cb_func = [this](client_data* user_data){cb_func(user_data)};
    time_t cur = time(NULL);
    timer->expire = cur + 1 * TIMESLOT;//超时时间为15s，15无连接则关闭链接
    m_users_timer[connfd].timer = timer;
    m_timer_list.add_timer(timer);

    return true;

}



void server::timer_over(int sockfd)
{
    util_timer *timer = m_users_timer[sockfd].timer;
    timer->cb_func(&m_users_timer[sockfd]);
    if (timer)
    {
        m_timer_list.del_timer(timer);
    }
}

bool server::time_signal()
{
    int ret;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(-1 == ret)
    {
        //handle the error
        return false;
    }
    else if(ret == 0)
    {
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
                    m_stop_server = true;
                }
                
            }
        }
    }
}

void server::read_thing(int sockfd)
{
    util_timer *timer = m_users_timer[sockfd].timer;
    //printf("wait epollin accpt!\n");
    if(users[sockfd].read())
    {
        m_pool->append(users + sockfd);//直接吧整个http_conn都传进去，不过传的都是地址哈，+sockfd就是传当前sock所在的对象呀
        if(timer)
        {
            m_timer_list.adjust_timer(timer);
        }

        updata_time(sockfd);
    }
    else
    {
        timer->cb_func(&m_users_timer[sockfd]);
        if (timer)
        {
            m_timer_list.del_timer(timer);
        }
        //users[sockfd].close_conn();
    }
}

void server::write_thing(int sockfd)
{
    //printf("wait epollout accpt!\n");
    if(users[sockfd].write())
    {
        updata_time(sockfd);
    }
    else 
    {
        timer_over(sockfd);
    }
}

void server::main_loop()
{

    m_timeout = false;
    m_stop_server = false;
    while(!m_stop_server)
    {
        //printf("epoll wait!\n");


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
                printf("------------------fd==[%d]\n",events[i].data.fd);

                printf("\n");
                printf("\n");
                if(events[i].events & EPOLLRDHUP) printf("---------EPOLLRDHUP--------");
                if(events[i].events & EPOLLHUP) printf("---------EPOLLHUP--------");
                if(events[i].events & EPOLLERR) printf("---------EPOLLERR--------");
                printf("\n");
                printf("\n");
                printf("accpt error! and close fd \n");
                //如果有异常直接关闭链接
                timer_over(sockfd);
            }


            //一旦信号触发，触发sig_headle函数往pipe[1]中写入数据，此时可以触发epoll
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                printf("signal事件\n");
                int flag = time_signal();
                if(false == flag)
                {
                    continue;
                }
            }


            else if(events[i].events & EPOLLIN)
            {
                printf("epollin事件\n");
                read_thing(sockfd);
            }

            else if(events[i].events & EPOLLOUT)
            {
                //创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_list中
                printf("写事件\n");
                write_thing(sockfd);

            }
            if(m_timeout)
            {
                printf("时间到了\n");
                timer_handler();
                m_timeout = false;
            }
        }
    }
}


//更新sockfd上的计时器
void server::updata_time(int sockfd)
{
    util_timer * timer = m_users_timer[sockfd].timer;
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
    //定时处理任务，实际上就是调用tick函数
    m_timer_list.tick();
    //因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信号
    alarm(TIMESLOT);
}

