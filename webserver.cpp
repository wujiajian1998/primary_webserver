#include "webserver.h"
class Utils;
//定时器到时任务函数
void cb_func(client_data *user_data)
{
    //将sockfd从epoll中删去
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    //客户端连接减1
    http_conn::m_user_count--;
}

Webserver::Webserver() 
{
    //客户端连接数组
    users = new http_conn[MAX_FD];
    //定时器数组
    users_timer = new client_data[MAX_FD]; 

}

Webserver::~Webserver()
{
    //关闭连接
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    //释放资源
    delete[] users;
    delete[] users_timer;
    //delete m_pool;
    
}
//服务器参数初始化
void Webserver::init(int port, int thread_num, int trigmode, int close_log, int OPT_LINGER, int actor_model)
{
    m_port = port;
    m_thread_num = thread_num;
    m_trigmode = trigmode;
    m_close_log = close_log;
    m_OPT_LINGER = OPT_LINGER;
    m_actormodel = actor_model;
}
//设置触发模式
void Webserver::trigmode()
{   
    switch (m_trigmode)
    {
    case 0://LT + LT
        m_listenmode = 0;
        m_connmode = 0;
        break;
    case 1://LT + ET
        m_listenmode = 0;
        m_connmode = 1;
        break;
    case 2://ET + LT
        m_listenmode = 1;
        m_connmode = 0;
        break;
    case 3://ET + ET
        m_listenmode = 1;
        m_connmode = 1;
        break;
    default:
        break;
    }

}


void Webserver::log_write()
{

        if(m_close_log == 0)  {
            //初始化日志
            Log::Instance()->init(1, "./log", ".log", 0);
            if(m_stop_server) { LOG_ERROR("========== Server init error!==========");}
            else {
                LOG_INFO("========== Server init ==========");
                LOG_INFO("Port:%d", m_port);
                LOG_INFO("Listen Mode: %s, Conn Mode: %s",
                                (m_listenmode == 1 ? "ET": "LT"),
                                (m_connmode == 1 ? "ET": "LT"));
                LOG_INFO("OPT LINGER: %s", m_OPT_LINGER == 1 ? "优雅关闭": "直接关闭");
                LOG_INFO("actor Mode: %s", m_actormodel == 1 ? "Reactor": "Proactor");
                //LOG_INFO("LogSys level: %d", logLevel);
                //LOG_INFO("srcDir: %s", HttpConn::srcDir);
                //LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
            }
        }  
        
    
}

void Webserver::add_client(int connfd, struct sockaddr_in client_address) {
    users[connfd].init( connfd, client_address, m_connmode, m_close_log);
    //初始化client_data数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
     //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中 
    util_timer* timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time( NULL );
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer( timer );
}

//处理客户端数据
bool Webserver::deal_clinetdata() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof( client_address );
    if(m_listenmode == 0) {
        //LT
        //printf("\n\nlisten read mode : LT\n\n");
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD) {
            LOG_ERROR("%s", "Internal server busy");
            close(connfd);
            //utils.show_error(connfd, "Clients is full");
            return false;
        }
        add_client(connfd, client_address);
    } else {
        //ET 循环监听
        while (1) {
            //printf("\n\nlisten read mode : ET\n\n");
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0) {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD) {
                LOG_ERROR("%s", "Internal server busy");
                //utils.show_error(connfd, "Clients is full");
                close(connfd);
                break;
            }
            add_client(connfd, client_address);
        }
        return false;
    }
    return true;   
}

//处理定时器信号
bool Webserver::deal_signal(bool &timeout, bool &stop_server) {
    int sig;
    char signals[1024];
    int ret = 0;
    ret = recv( m_pipefd[0], signals, sizeof( signals ), 0 );
    if( ret == -1 ) {
        return false;
    } else if( ret == 0 )  {
        return false;
    } else  {
        for( int i = 0; i < ret; ++i ) {
            switch( signals[i] )  {
                case SIGALRM: {
                    // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                    // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

// 创建线程池
void Webserver::thread_pool()
{
    //线程池
    m_pool = new ThreadPool<http_conn>(4, 12);
}
//创建监听fd并绑定
void Webserver::eventListen()
{
    //创建监听文件标识符
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    if(m_OPT_LINGER == 0) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (m_OPT_LINGER == 1) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    //清空
    bzero( &address, sizeof( address ) );
    //任意ip都可以连接
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    //ipv4
    address.sin_family = AF_INET;
    //指定端口
    address.sin_port = htons(m_port);
    //设置端口复用
    int reuse = 1;
    setsockopt( m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    //绑定
    ret = bind( m_listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0);
    //监听
    ret = listen( m_listenfd, 5);
    assert( ret >= 0);
    //初始化工具类 设置超时时间
    utils.init(TIMESLOT);

    //epoll创建内核事件表
    m_epollfd = epoll_create( 5 );
    assert(m_epollfd != -1);
    //设定触发方式
    
    utils.addfd(m_epollfd, m_listenfd, false, m_listenmode);
   
    http_conn::m_epollfd = m_epollfd;

    //创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    //设置管道写端为非阻塞  减少信号处理函数的执行时间
    utils.setnonblocking(m_pipefd[1]);
    //监听管道读端
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);
    
    //设置信号处理函数
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);
    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

//事件循环  proactor模式
void Webserver::eventLoop()
{   
    //超时信号
    bool timeout = false;
    //停止服务信号
    bool stop_server = false;
    if(!stop_server) { LOG_INFO("========== Server start =========="); }
    while(!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("Epoll Error");
            break;
        }
        for ( int i = 0; i < number; i++ )  {
            int sockfd = events[i].data.fd;
            if( sockfd == m_listenfd ) {
                //监听到的是主线程
                bool flag = deal_clinetdata();
                if(false == flag) continue;
            } 
            else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )  {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                del_timer(timer, sockfd);
            } 
            else if( ( sockfd == m_pipefd[0] ) && ( events[i].events & EPOLLIN ) ) {
                // 处理定时器信号
                int sig;
                char signals[1024];
                int ret = 0;
                bool flag = true;
                ret = recv( m_pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 ) {
                    flag = false;
                } else if( ret == 0 )  {
                    flag = false;
                } else  {
                    for( int i = 0; i < ret; ++i ) {
                        switch( signals[i] )  {
                            case SIGALRM: {
                                // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                                // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                                timeout = true;
                                break;
                            }
                            case SIGTERM: {
                                stop_server = true;
                                break;
                            }
                        }
                    }
                }
                if (false == flag) {
                    LOG_ERROR("%s", "dealsignal failure");
                    continue;
                }   
            } else if(events[i].events & EPOLLIN) {
                deal_read(sockfd);
            } else if( events[i].events & EPOLLOUT ) {
                deal_write(sockfd);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级
        if( timeout ) {
            utils.timer_handler();
            //信号归位
            timeout = false;

        }
    
    }

}

//处理读事件 
void Webserver::deal_read(int sockfd) {
    util_timer *timer = users_timer[sockfd].timer;

    
        //请求读事件
        if(users[sockfd].read()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].m_address.sin_addr));
            //主线程完成request的处理 加入线程池任务队列
            m_pool->append(users + sockfd);
           
            if (timer) {
                adjust_timer(timer);
            }
        } else {
            //写失败 关闭连接 删除定时器
            del_timer(timer, sockfd); 

        }
    
    
}

//处理写事件 
void Webserver::deal_write(int sockfd) {
    util_timer *timer = users_timer[sockfd].timer;
    
    
        //Proactor
        if(users[sockfd].write() ) {
        LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].m_address.sin_addr));
            //写事件成功
            if (timer) {
                //更新定时器到期时间
                adjust_timer(timer);
            }
        }
        else {
            //一次性写失败 关闭连接 删除定时器
            del_timer(timer, sockfd); 
        }
    
    
}

//调整定时器
void Webserver::adjust_timer(util_timer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}
//删除定时器
void Webserver::del_timer(util_timer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);
        if(timer)
            utils.m_timer_lst.del_timer(timer); 
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}