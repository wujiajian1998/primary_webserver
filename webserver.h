#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "./threadpool/threadPool.h"
#include "./http_conn/http_conn.h"
#include <assert.h>
#include "./log/log.h"
const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class Webserver
{
public:

    Webserver();
    ~Webserver();
    void init(int port, int thread_num, int trigmode, int close_log, int OPT_LINGER, int actor_model);
    void thread_pool();
    void eventListen();
    void eventLoop();
    void add_client(int connfd, struct sockaddr_in client_address);
    //void adjust_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer, int sockfd);
    bool deal_clinetdata();
    bool deal_signal(bool& timeout, bool& stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
    void trigmode();
    void log_write();

public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;
    bool m_stop_server;
    int m_pipefd[2];
    int m_epollfd;
    int m_OPT_LINGER;
    http_conn *users;

    //线程池
    ThreadPool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event
    epoll_event events[MAX_EVENT_NUMBER];
    int m_listenfd;

    //trigmode
    int m_trigmode;
    int m_listenmode;
    int m_connmode;
    //定时器
    client_data *users_timer;
    Utils utils;
};


#endif