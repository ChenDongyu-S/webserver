#include "server.h"



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


    {

        

        server webserver(port, 10, 10000, trig_model, actor_model);

        webserver.init_log();//默认开启，阻塞队列模式

        if(!webserver.create_threadpool())
        {
            
            return -1;
        }
        
        printf("create_http_conn\n");
        webserver.create_http_conn();

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