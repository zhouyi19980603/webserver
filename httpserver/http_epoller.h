#ifndef HTTP_EPOLLER_H
#define HTTP_EPOLLER_H

/**************************************************************

  @brief....: epoll的事件处理 比如注册描述符进内核，删除，修改
  @author...: zhouyi
  @date.....:2020-08-05

  ************************************************************/


#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class HttpEpoller{
public:
    explicit HttpEpoller(int epollfd);
    ~HttpEpoller();
    void addfd(int fd,bool one_shot);
    void removefd(int fd);
    void modfd(int fd,int ev);
    int setnonblacking(int fd);

private:
     int m_epollfd; //epollfd文件描述符
};

#endif // HTTP_EPOLLER_H
