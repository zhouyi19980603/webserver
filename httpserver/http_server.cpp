#include "http_server.h"
#include"commen/commen.h"


/**************************************************************

  @brief....: web轻量服务器的简单版本
  @author...: zhouyi
  @date.....:2020-08-05

  ************************************************************/




HttpServer::HttpServer()
{
    char server_path[200];
    getcwd(server_path,200); //得到当前路径
    char resource[10]="/resource"; //资源所存放的路径

    try {
        users.resize(MAX_FD); //预先定义vector内存大小
        m_port = 8080;

        int m_threads = 8;
        m_pool = new threadpool<HttpConn>(m_threads);
    } catch (const std::exception&) {
        if(m_pool)
            delete m_pool;
        throw "内存错误";
    }
    user_count = 0;

    //commen初始化静态变量

}


HttpServer::~HttpServer()
{
    close(m_epollfd);
    close(m_listened);
    delete m_pool;
}

void HttpServer::start()
{
    event_listen();
    event_loop();
}

void HttpServer::event_listen()
{
    cout<<"event_listen"<<endl;
    //网络服务端基本流程
    m_listened = socket(PF_INET,SOCK_STREAM,0);
    assert(m_listened > 0); //断言判断

    //优雅关闭
    struct linger tmp = {0, 1};
    setsockopt(m_listened, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    int on=1;
    setsockopt(m_listened,SOL_SOCKET, SO_REUSEADDR,&on,sizeof (on));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof (address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    ret = bind(m_listened,(struct sockaddr*)&address,sizeof (address));
    assert(ret >=0);
    ret = listen(m_listened,5);
    assert(ret >= 0);
    m_epollfd = epoll_create(5); //创建epoll文件描述符，内部会创建一个红黑树维护

    m_epoller = new HttpEpoller (m_epollfd); //这里去调用构造函数 初始化
    //将其添加进内核处理中epoll_ctl
    m_epoller->addfd(m_listened,false);

    //httpconn还没链接
    HttpConn::m_epolled = m_epollfd; // 给http

}

void HttpServer::event_loop()
{
    cout<<"event_loop 外层"<<endl;
    while (true) {
        //准备好的有序事件会放在events
        cout<<"event_loop 内层"<<endl;
        int number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno != EINTR))
        {
            cout<<"epoll failure";
            break;
        }
        for(int i=0;i<number;i++)
        {
            int sockfd = events[i].data.fd; //得到相应的文件描述符
            if(sockfd == m_listened) //监听事件的处理
            {
                if(!deal_listen())
                    continue;
            }else if(events[i].events & EPOLLIN) //读
            {
                deal_read(sockfd);
            }else if(events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
    }
}

bool HttpServer::deal_listen()
{

    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof (client_address);
    int connfd = accept(m_listened,(sockaddr*)&client_address,&client_addrlen);
    cout<<"这是连接事件处理"<<endl;
    if(connfd < 0)
    {
        cout << "connfd is error 连接出错"<<endl;
        return false;
    }
    //初始化操作
    users[connfd].init(connfd,client_address);
}

void HttpServer::deal_read(int sockfd)
{
    if(users[sockfd].read())
    {
        //这里会添加进线程池中
        cout<<"deal_read(int sockfd)"<<endl;
        m_pool->append(&users[sockfd]);
    }else
    {
        users[sockfd].close_conn();
    }
}

void HttpServer::deal_write(int sockfd)
{
    if(users[sockfd].write())
    {
        cout<<"deal_write(int sockfd)"<<endl;
    }else
    {
//        m_epoller->removefd(sockfd);
        users[sockfd].close_conn();
    }
}
