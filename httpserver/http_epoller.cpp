#include "http_epoller.h"

HttpEpoller::HttpEpoller(int epollfd)
    :m_epollfd(epollfd)
{

}

HttpEpoller::~HttpEpoller()
{
    close(m_epollfd);
}

void HttpEpoller::addfd(int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLHUP;
    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(m_epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblacking(fd);
}

void HttpEpoller::removefd(int fd)
{
    epoll_ctl(m_epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void HttpEpoller::modfd(int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(m_epollfd,EPOLL_CTL_MOD,fd,&event);
}

int HttpEpoller::setnonblacking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);//ET模式  一次读取
    int new_option = old_option | O_NONBLOCK;//非堵塞
    fcntl(fd, F_SETFL, new_option);//添加新方法
    return old_option;//返回之前设置
}
