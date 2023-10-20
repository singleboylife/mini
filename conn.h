#ifndef __CONN_H__
#define __CONN_H__
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "locker.h"
#include "json.h"
#include <string>
#include "conn.h"


using namespace std;

static const int READBUFSIZE = 4096;
static const int WRITEBUFSIZE = 8192;



enum PROCESS_CODE {
    INCOMPLETE = 0, //JSON数据不完整
    REPLY,          //有数据返回客户
    NONE,            //测试使用
    DONE            //读取完所有数据
};

class conn {
public:
    /*初始化新接受的连接*/
    void init(int sockfd, const sockaddr_in& addr);

    void init();

    /*解析协议，处理客户请求*/
    void process();

    /*非阻塞读操作*/
    bool read();

    /*非阻塞写操作*/
    bool process_write();


    //关闭客户端和服务器的连接
    void close_conn(bool real_close = true);

    conn() {
        // fprintf(stdout, "Debug info: construct one connection\n");
    }

    ~conn() {//析构
        fprintf(stdout, "Destroy one connection\n");
    }
    //所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_epollfd;
    static int m_user_count;

    //read data buffer should be private, for test, its public
    char m_read_buf[READBUFSIZE];

    //服务器的本地sockfd
    int m_sockfd;
private:
    int m_userId;

    //客户端的地址
    sockaddr_in m_address;


    int m_read_idx;


    //当前正在分析的字符在读缓冲区中的位置
    int m_checked_idx;

    //协议相关的字段，操作命令
    string cmd;

    string reply;   //json形式
    char m_write_buf[WRITEBUFSIZE];  //char 数组
    int m_write_idx;

private:
    void do_register(const Json::Value& root);
    void do_login(const Json::Value& json);
    
void do_delete(const Json::Value& json);
void do_find(const Json::Value& json);
void do_tempreture(const Json::Value& json);
void do_statu(const Json::Value& json);
void do_add(const Json::Value& json);
    PROCESS_CODE process_read();
};

#endif

