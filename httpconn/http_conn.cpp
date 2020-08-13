#include "http_conn.h"
#include "commen/commen.h"


int HttpConn::m_user_count = 0;
int HttpConn::m_epolled = -1;

//关闭连接
void HttpConn::close_conn(bool real_close) {

    if (real_close && (m_socked != -1)) {
        removefd(m_epolled, m_socked);
        std::cout<<"socket:"<<m_socked<<" host:"<<m_host<<" close connection"<<std::endl;
        m_socked = -1;
        m_user_count--;
    }
}

void HttpConn::process()
{

    RESULT_CODE read_ret = process_read();
    //NO_REQUEST，表示请求不完整，需要继续接收请求数据
    if (read_ret == NO_REQUEST) {
        //注册并监听读事件
        modfd(m_epolled, m_socked, EPOLLIN);
        //异常
        return;
    }

    bool write_ret = process_write(read_ret);

    if (!write_ret) {
       close_conn();
    }

    modfd(m_epolled, m_socked, EPOLLOUT);
}
void HttpConn::init(int sockfd, const sockaddr_in& addr) {
    m_socked = sockfd;
    m_address = addr;
    int reuse = 1;
    setsockopt(m_socked, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epolled, sockfd, true);
    m_user_count++;
    init_state();
}
void HttpConn::init_state() {
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHEACK_STATE_REQUESTLINE;
    m_link = false;//默认不保持连接
    m_quest_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_state = CHEACK_STATE_REQUESTLINE;
    m_read_idx = 0;
    m_write_idx = 0;
    m_checked_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

/*
process_read通过while循环，将主从状态机进行封装，对报文的每一行进行循环处理。

判断条件

    主状态机转移到CHECK_STATE_CONTENT，该条件涉及解析消息体

    从状态机转移到LINE_OK，该条件涉及解析请求行和请求头部

    两者为或关系，当条件为真则继续循环，否则退出

循环体

    从状态机读取数据

    调用get_line函数，通过m_start_line将从状态机读取数据间接赋给text

    主状态机解析text

*/

HttpConn::RESULT_CODE HttpConn::process_read()//main state mache
{
    LINE_STATUS line_status = LINE_OK;
    RESULT_CODE ret = NO_REQUEST;
    char* text = 0;
    while (((m_check_state == CHEACK_STATE_CONTENT) && (line_status == LINE_OK)) || ((line_status = parse_line()) == LINE_OK)) {
        text = get_line();
        //得到一行数据
        //m_start_line是每一个数据行在m_read_buf中的起始位置
        //m_checked_idx表示从状态机在m_read_buf中读取的位置
        m_start_line = m_checked_idx;
        std::cout<<"info:"<<text<<std::endl;
        switch (m_check_state) {
        case CHEACK_STATE_REQUESTLINE: {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;
        }
        case CHEACK_STATE_HEADER: {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST) {
                return  BAD_REQUEST;

            }
            else if (ret == GET_REQUEST) {
                return do_request();
            }
            break;
        }
        case CHEACK_STATE_CONTENT:
        {
            //解析消息体
            ret = parse_content(text);
            //完整解析POST请求后，跳转到报文响应函数
            if (ret == GET_REQUEST) {
                return do_request();
            }
            //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
            line_status = LINE_OPEN;
            break;
        }
        default: {
            return INTERNAL_ERROR;
        }
        }

    }
    return NO_REQUEST; //返回请求不完整状态信息
}

bool HttpConn::process_write(HttpConn::RESULT_CODE ret)
{
    switch (ret) {
    case INTERNAL_ERROR: {
        add_status_line(500,commen::error_500_title);
        add_headers(strlen(commen::error_500_form));
        if (!add_content(commen::error_500_form)) {
            return false;
        }
        break;
    }
    case BAD_REQUEST: {
        add_status_line(400, commen::error_400_title);
        add_headers(strlen(commen::error_400_form));
        if (!add_content(commen::error_400_form)) {
            return false;
        }
        break;
    }
    case NO_RESOURCE: {
        add_status_line(404,commen::error_404_title);
        add_headers(strlen(commen::error_404_form));
        if (!add_content(commen::error_404_form)) {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST: {
        add_status_line(400, commen::error_403_title);
        add_headers(strlen(commen::error_403_form));
        if (!add_content(commen::error_403_form)) {
            return false;
        }
        break;
    }
    case FILE_REQUEST: {
        //添加状态行：http/1.1 状态码 状态消息
        add_status_line(200,commen::ok_200_title);
        if (m_file_stat.st_size != 0) {
            //如果请求的资源存在
            add_headers(m_file_stat.st_size);
            //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_lv[0].iov_base = m_write_buf;
            m_lv[0].iov_len = m_write_idx;
            //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_lv[1].iov_base = m_file_address;
            m_lv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            //这里并没有对值进行保存，而是在使用时进行了遍历

            return true;

        }
        else {
            const char* ok_string = "<html><body><h1>hello</h1></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
            {
                return false;
            }
        }

    }
    default: {
        return false;
    }
    }
    m_lv[0].iov_base = m_write_buf;
    m_lv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;

}



void HttpConn::unmap()
{
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool HttpConn::add_response(const char* format, ...)
{
    //如果写入内容超出m_write_buf大小则报错
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    //定义可变参数列表
    va_list arg_list;
    //将变量arg_list初始化为传入参数
    va_start(arg_list, format);
    //将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    //如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    //更新m_write_idx位置
    m_write_idx += len;
    //清空可变参列表
    va_end(arg_list);

    printf("request: %s",m_write_buf);
    return true;
}
bool HttpConn::add_content(const char* content)
{
    return add_response("%s", content);
}

bool HttpConn::add_status_line(int status, const char* title)
{
    //第一个参数传进去的是格式
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::add_content_length(int content_length)
{
    return add_response("Content-Length: %d\r\n", content_length);
}
//添加连接状态，通知浏览器端是保持连接还是关闭
bool HttpConn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_link == true) ? "keep-alive" : "close");
}

bool HttpConn::add_content_type(const char* type)
{
    return add_response("Content-Type: %s;charset=utf-8\r\n", type);
}
//添加空行
bool HttpConn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool HttpConn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}

bool HttpConn::read()
{
    std::cout<<"deal_read()"<<std::endl;
    if (m_read_idx > READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_socked, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }
        else if (bytes_read == 0) {
            return false;
        }
        m_read_idx += bytes_read;

        // m_read_idx为字节所读的偏移量,最后也表示一共有的字节数
        std::cout<<"read():" <<m_read_idx<<std::endl;
    }
//    process();
    return true;
}

/*

服务器子线程调用process_write完成响应报文，随后注册epollout事件。服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端。
在生成响应报文时初始化byte_to_send，包括头部信息和文件数据大小。通过writev函数循环发送响应报文数据，根据返回值更新byte_have_send和iovec结构体的指针和长度，并判断响应报文整体是否发送成功。

若writev单次发送成功，更新byte_to_send和byte_have_send的大小，若响应报文整体发送成功,则取消mmap映射,并判断是否是长连接.

    长连接重置http类实例，注册读事件，不关闭连接，

    短连接直接关闭连接

若writev单次发送不成功，判断是否是写缓冲区满了。

    若不是因为缓冲区满了而失败，取消mmap映射，关闭连接

    若eagain则满了，更新iovec结构体的指针和长度，并注册写事件，等待下一次写事件触发（当写缓冲区从不可写变为可写，触发epollout），因此在此期间无法立即接收到同一用户的下一请求，但可以保证连接的完整性。
*/

bool HttpConn::write()
{
    int temp = 0;

    //若要发送的数据长度为0，表示响应报文为空，一般不会出现这种情况
    if(bytes_to_send == 0)
    {
        modfd(m_epolled,m_socked,EPOLLIN);
        init_state();
        return true;
    }

    while (1) {

        //所写入的缓冲区通常是非连续的
        //将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_socked,m_lv,m_iv_count);

        if(temp < 0)
        {
            if(errno == EAGAIN)
            {
                //重新注册写事件
                modfd(m_epolled,m_socked,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        if(bytes_have_send >= m_lv[0].iov_len)
        {
            m_lv[0].iov_len = 0;
            m_lv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_lv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_lv[0].iov_base = m_write_buf + bytes_have_send;
            m_lv[0].iov_len = m_lv[0].iov_len-bytes_have_send;
        }

        if(bytes_to_send <=0)
        {
            unmap();
            modfd(m_epolled,m_socked,EPOLLIN);
            if(m_link)
            {
                init_state();//重新初始化HTTP对象,不断开连接
                return true;
            }else
            {
                return false;
            }
        }
    }
}

HttpConn::LINE_STATUS HttpConn::parse_line()
{
    char temp;//表示当前字节
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if ((m_checked_idx + 1) == m_read_idx) {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n') {
            if ((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r')) {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
HttpConn::RESULT_CODE HttpConn::parse_request_line(char* text) {
    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
    //请求行中最先含有空格和\t任一字符的位置并返回
    m_url = strpbrk(text, " \t"); //取得\t的位置  在text中寻找第一个匹配\t的元素
    if (!m_url) {
        return BAD_REQUEST;    //如果没有寻找到 说明格式错误
    }

    *m_url++ = '\0'; //前面分隔 获得空格指向的下一个位置
    //因为这里设为了\0 所以目前text为method(posr or get)
    char* method = text;//text;
    if (strcasecmp(method, "GET") == 0) {
        m_quest_method = GET;//如果请求方式为GET
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_quest_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");//消除连续的空格
    m_version = strpbrk(m_url, " \t");//然后在寻找下一个"\t"
    if (!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';

    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    //对请求资源前7个字符进行判断
    //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
        //找到第一个匹配之处 返回对应匹配之处开始的字符串
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHEACK_STATE_HEADER;
    return NO_REQUEST;
}

/*
解析完请求行后，主状态机继续分析请求头。在报文中，请求头和空行的处理使用的同一个函数，这里通过判断当前的text首位是不是\0字符，若是，则表示当前处理的是空行，若不是，则表示当前处理的是请求头。


CHECK_STATE_HEADER

    调用parse_headers函数解析请求头部信息

    判断是空行还是请求头，若是空行，进而判断content-length是否为0，如果不是0，表明是POST请求，则状态转移到CHECK_STATE_CONTENT，否则说明是GET请求，则报文解析结束。

    若解析的是请求头部字段，则主要分析connection字段，content-length字段，其他字段可以直接跳过，各位也可以根据需求继续分析。

    connection字段判断是keep-alive还是close，决定是长连接还是短连接

    content-length字段，这里用于读取post请求的消息体长度
*/
//解析http请求的一个头部信息

HttpConn::RESULT_CODE HttpConn::parse_headers(char* text)
{
    //判断是空行还是请求头
    //等于\0表明这是空行，不是请求头
    if (text[0] == '\0') {
        //判断是GET还是POST请求
        if (m_content_length != 0) {
            std::cout<<"这是post请求"<<std::endl;
            m_check_state = CHEACK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " ");
        if (strcasecmp(text, "keep-alive") == 0) {
//            m_link = false;
            m_link = true;
        }
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " ");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " ");
        m_host = text;
    }
    else if (strncasecmp(text, "Accept-Encoding:", 16) == 0) {
        text += 16;
        text += strspn(text, " ");
        m_encode_method = text;
    }
    else if (strncasecmp(text, "User-Agent:", 11) == 0) {
        text += 11;
        text += strspn(text, " ");
        m_user_agent = text;
    }
    else if (strncasecmp(text, "Accept:", 7) == 0) {
        text += 7;
        text += strspn(text, " ");
        m_accept_types.push_back(text);
    }
    else if (strncasecmp(text, "Accept-Language:", 16) == 0) {
        text += 16;
        text += strspn(text, " ");
        m_accept_language.push_back(text);
    }
    else {
        printf("oop! unknow header %s\n", text);

    }
    return NO_REQUEST;
}

HttpConn::RESULT_CODE HttpConn::parse_content(char* text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;

}

//8-6渲染响应界面时出了问题
HttpConn::RESULT_CODE HttpConn::do_request()
{
    // /run/media/root/linux_data/C++/UNIX 网络编程/网络编程项目实战/TinyWebServer/root/


    strcpy(m_real_file,commen::doc_root);
    int len = strlen(commen::doc_root);
//    std::cout<<"m_real_file:"<<commen::doc_root<<std::endl;
    //printf("m_url:%s\n", m_url);
    //找到m_url中/的位置
    const char *p = strrchr(m_url, '/');


    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //将网站目录和/log.html进行拼接，更新到m_real_file中
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '2') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/welcome.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    //如果以上均不符合，即不是登录和注册，直接将url与网站目录拼接
    //这里的情况是welcome界面，请求服务器上的一个图片

    //通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    //失败返回NO_RESOURCE状态，表示资源不存在
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

     //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    //以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    //避免文件描述符的浪费和占用
    close(fd);
    //表示请求文件存在，且可以访问
    return FILE_REQUEST;
}


HttpConn::HttpConn() {

}

HttpConn::~HttpConn()
{

}
