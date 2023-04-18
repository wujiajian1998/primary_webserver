#include "webserver.h"



int main(int argc, char *argv[])
{
    if( argc <= 6 ) {
        printf( "usage: %s port_number threadnum trigmode close_log OPT_LINGER\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]);
    int threadnum = atoi(argv[2]);
    int trigmode = atoi(argv[3]);
    int close_log = atoi(argv[4]);
    int OPT_LINGER = atoi(argv[5]);
    int actor_model = atoi(argv[6]);
    Webserver server;

    printf( "开始初始化\n");
    server.init(port, threadnum, trigmode, close_log, OPT_LINGER, actor_model);

    printf( "创建线程池\n");
    server.thread_pool();

    printf("设置触发模式");
    server.trigmode();

    printf("初始化日志\n");
    server.log_write();

    printf( "开始监听\n");
    server.eventListen();
    
    printf( "开始循环事件\n");
    server.eventLoop();

    return 0;
}