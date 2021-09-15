#include "server.h"

#include <sys/param.h>


int main(int argc, char* argv[])
{
    if(argc <= 1)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    //const char* ip = argv[1];
    int port = atoi(argv[1]);

    int trig_model = 1;//触发模式：ET：1、LT：0
    int actor_model = 1;//反应堆模式：proactor：0；reactor：1

    int pid; 
	int i; 
 
	//忽略终端I/O信号，STOP信号
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	
	pid = fork();
	if(pid > 0) {
		exit(0); //结束父进程，使得子进程成为后台进程
	}
	else if(pid < 0) { 
		return -1;
	}
 
	//建立一个新的进程组,在这个新的进程组中,子进程成为这个进程组的首进程,以使该进程脱离所有终端
	setsid();
 
	//再次新建一个子进程，退出父进程，保证该进程不是进程组长，同时让该进程无法再打开一个新的终端
	pid=fork();
	if( pid > 0) {
		exit(0);
	}
	else if( pid< 0) {
		return -1;
	}
 
	//关闭所有从父进程继承的不再需要的文件描述符
	for(i=0;i< NOFILE;close(i++));
 
	//改变工作目录，使得进程不与任何文件系统联系
	chdir("/mnt/hgfs/lalala/test");
 
	//将文件当时创建屏蔽字设置为0
	umask(0);
 
	//忽略SIGCHLD信号
	signal(SIGCHLD,SIG_IGN); 




    {

		string user = "cdy";
		string password = "wsadcdy123ad";
		string dataBaseName = "webData";

        server webserver(port, 10, 10000, trig_model, actor_model, user, password, dataBaseName);

        webserver.init_log();//默认开启，阻塞队列模式

        
        
        printf("create_http_conn\n");
        webserver.create_http_conn();

		printf("create_sql\n");
		webserver.sql_pool();

		if(!webserver.create_threadpool())
		{
			
			return -1;
		}

        printf("create_listen\n");
        webserver.create_listen();

        printf("create_epoll\n");
        webserver.create_epoll();

        printf("create_timer\n");
        webserver.create_timer();

        printf("main_loop\n");
        webserver.main_loop();
    }


    return 0;

}