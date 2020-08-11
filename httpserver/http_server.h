#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <sys/types.h>
#include <iostream>
#include <cstring>
#include "httpconn/http_conn.h"
#include "threadpool/threadpool.h"
#include "http_epoller.h"

using namespace std;


const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
//const int TIMESLOT = 5;             //最小超时单位

class HttpServer
{
public:
    HttpServer();
    ~HttpServer();

    void start();

private:
    char* m_root;
    size_t m_port;
    epoll_event events[MAX_EVENT_NUMBER];  //最大事件处理数 也就是连接数
    int m_epollfd;
    size_t m_listened;
    size_t user_count;
    HttpEpoller* m_epoller = nullptr;
    vector<HttpConn> users;
    threadpool<HttpConn>* m_pool;

private:
    void event_listen(); //事件监听
    void event_loop(); //事件循环处理
    bool deal_listen();
    void deal_read(int sockfd);
    void deal_write(int sockfd);
};


#endif // HTTP_SERVER_H
