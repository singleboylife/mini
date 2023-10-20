#include <cstring>
#include "json.h"
#include <iostream>
#include <string>
#include "ChatDataBase.h"
#include "conn.h"
#include <unistd.h>

#include <vector>
#include <unordered_map>
#include "DbPool.h"

using namespace std;

locker lock;
unordered_map<int, int> mp; //userid --> sockfd
extern conn* conns;         //连接数组
extern DbPool* db_pool;     //数据库连接池

//epoll文件描述符相关操作，开始~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*将文件描述符fd设置为非阻塞*/
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/*将文件描述符fd注册到epollfd内核事件表中，并使用边沿触发模式*/
void addfd(int epollfd, int fd, bool one_shot) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; //边沿触发模式，读取数据应该一次性读取完毕
    if (one_shot) {
        //EPOLLONESHOT事件：当一个线程在处理某个socket时，其他线程是不可能有机会操作该socket的
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd); //所有加入epoll监视的文件描述符都是非阻塞的
}

/*从epollfd标识的epoll内核事件表中删除fd上的所有注册事件*/
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//对epollfd监视的文件描述符fd，添加ev事件
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
//epoll文件描述符相关操作，结束~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


//init
int conn::m_user_count = 0;
int conn::m_epollfd = -1;

//服务器和客户端建立连接时调用该函数，sockfd是该链接的服务器本地sock，addr是客户端的地址
void conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;
    m_userId = -1;

    //以下两行避免TIME_WAIT状态，仅用于调试，实际使用应去掉
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;

    init();
}

//init all data member
void conn::init() {//长连接，调用该函数，初始化所有数据
    //初始化链接的所有成员
    memset(m_read_buf, '\0', sizeof m_read_buf);
    m_read_idx = 0;
    memset(m_write_buf, '\0', sizeof m_write_buf);
    m_write_idx = 0;
    cmd = "";
    reply = "";
    m_checked_idx = 0;
}


//关闭和客户端的连接
void conn::close_conn(bool real_close /*= true*/) {
    if (real_close && m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);  //从epoll的监视集合中删除连接
        m_sockfd = -1;
        m_user_count--;	//关闭一个连接，客户总数减一
    }
    //将用户状态修改为下线
    if (m_userId != -1) {
        ChatDataBase* db_conn;

        //构造函数从数据库池中拿到一条连接,局部变量 用完之后会会立马往连接池中归还连接归还
        connectionRAII connRAII(db_conn, db_pool);//使用RAII技术从数据库连接池中获取一条数据库连接
        printf("Debug info: userId = %d offline\n", m_userId);
        lock.lock();
        //db_conn->my_database_offline(m_userId);
        m_userId = -1;
        mp.erase(m_userId);
        lock.unlock();
    }
}


//循环读取客户数据，直到无数据可读或者对方关闭连接，注意epoll为边沿触发模式，一次读取数据要读到阻塞
//主线程调用，读取数据
bool conn::read() {
    if (m_read_idx >= READBUFSIZE) {
        return false;
    }

    //这里是否需要考虑将m_read_idx重新置零，不能重新将m_read_idx置零，一次收到的json可能不完整
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READBUFSIZE -
            m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {//非阻塞io，读到EWOULDBLOCK说明数据已经读取完毕
                printf("Debug info: read done! EWOULDBLOCK.\n");
                break;
            }
            return false;//读取出现未知错误
        }
        else if (bytes_read == 0) {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}


//process_read()中调用，处理用户登录的请求
void conn::do_login(const Json::Value& json) {
    int userid = json["userId"].asInt();
    string password = json["password"].asString();

    Json::Value ret;
    ret["cmd"] = "login-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);

    if (db_conn->my_database_password_correct(userid, password)) {//返回一个登录成功的提示信息，并将用户状态修改为在线
        ret["success"] = 1;

        //将用户状态设置为上线
        m_userId = userid;
        printf("login debug, m_userId = %d, m_sockfd = %d\n", m_userId, m_sockfd);
        lock.lock();
        mp[m_userId] = m_sockfd;   //上线加入map中
        lock.unlock();
    }
    else {
        ret["success"] = 0;
    }
    User u; //登录返回用户信息
    db_conn->my_database_user_information(u, userid);

    ret["user"] = userToJsonObj(u);
    reply = ret.toStyledString();
    /*
    if (reply.size() >= WRITEBUFSIZE) {
        fprintf(stderr, "error: write buffer overflow\n");
        return false;
    }
    */
    //copy to send buffer，这里有缓冲区溢出的风险，后期考虑加上判断
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';
}

//
void conn::do_register(const Json::Value& root) {
    int  userID = root["userId"].asInt();
    string password = root["password"].asString();
    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);
  //  int userId=root["userId"].asInt();
   int userId=0;
    cout << "666\n" << endl;
    db_conn->my_database_user_password(userID, password, userId);
    cout << "777\n" << endl;
    Json::Value ret;
    ret["cmd"] = "register-reply";
    ret["userId"] = userId;
    reply = ret.toStyledString();

    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';
}


//处理读取到的用户请求，
PROCESS_CODE conn::process_read() {
    //首先判断是否读取到了完整的json
    int left_bracket_num = 0, right_bracket_num = 0, idx = 0, complete = 0;
    // while (idx < m_read_idx && m_read_buf[idx] != '{') idx++;//json 解析开始
    // if (idx >= m_read_idx) {
    //     return DONE;
    // }
    while (idx < m_read_idx && m_read_buf[idx]) {
        if (m_read_buf[idx] == '{') left_bracket_num++;
        else if (m_read_buf[idx] == '}') right_bracket_num++;
        idx++;
        if (left_bracket_num == right_bracket_num && right_bracket_num != 0) {//读取到了完整的json
            complete = 1;
            //m_read_buf[idx] = 0;
            break;
        }
    }
    // if (idx >= m_read_idx) {
    //     return DONE;
    // }
    if (!complete) {
        fprintf(stdout, "json data format is incomplete \n");
        return INCOMPLETE;
    }

    //读取到了完整的json，下面调用jsoncpp，读取json的内容
    Json::Reader reader;
    Json::Value root;

    int cnt = 0;
    if (!(cnt = reader.parse(m_read_buf, root))) {
        fprintf(stderr, "error, parse json error");
        return INCOMPLETE;  //ERROR
    }

    // printf("m_read_buf[idx] = %c, m_read_buf + idx = %s", m_read_buf[idx], m_read_buf + idx);
    // while (idx < READBUFSIZE && (m_read_buf[idx] == '\0' || m_read_buf[idx] == '\t' || m_read_buf[idx] == '\n') ) idx++;



    string cmd = root["cmd"].asString();
    if (cmd == "register") {
        printf("Debug info: deal with register\n");
        do_register(root);
        printf("Debug info: register reply json: %s", reply.c_str());
        //return REPLY;
    }
    else if (cmd == "login") {
        printf("Debug info: deal with login");
        do_login(root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
        //return REPLY;
    }else if (cmd == "delete")
    {
        printf("Debug info: deal with delete");
        do_delete(root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
    }
    else if(cmd=="find")
    {
        printf("Debug info: deal with find");
        do_find (root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
    
    }
    else if (cmd == "tempreture") 
    {
        printf("Debug info: deal with tempreture");
        do_tempreture(root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
    
    }
    else if (cmd == "statu") 
    {

        printf("Debug info: deal with statu");
        do_statu(root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
    }
    else 
    {
        printf("Debug info: deal with add");
        do_add(root);
        printf("Debug info: login reply json:\n%s,", reply.c_str());
    
    }

    return REPLY;
}

//由线程池中的工作线程调用，这是处理客户请求的入口函数
void conn::process() {
    PROCESS_CODE read_ret = process_read();
    if (read_ret == INCOMPLETE) {
        modfd(m_epollfd, m_sockfd, EPOLLIN); //继续读取数据
        return;
    }
    else if (read_ret == REPLY) {//已经准备好要写回的json，存放在reply
        modfd(m_epollfd, m_sockfd, EPOLLOUT);	//对m_sockfd添加对可写事件，此时主线程epoll_wait返回，处理写事件
    }
    // else {
    //     modfd(m_epollfd, m_sockfd, EPOLLIN);
    // }
}


//sock可写时，主线程epoll返回，会调用该函数，向客户端写入数据
bool conn::process_write() {
    int temp = 0;
    int bytes_have_send = m_write_idx;//数据如果没有一次性发完，接着发送数据
    int bytes_to_send = reply.size() - m_write_idx;//发送一个字符串结束符 '\0'
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1) {
        temp = write(m_sockfd, m_write_buf + bytes_have_send, bytes_to_send);
        if (temp <= -1) {
            if (errno == EAGAIN) {
                /*如果TCP写缓冲区没有空间，等待下一轮EPOLLOUT事件。虽然在此期间，
                服务器无法立即接收到同一客户的下一个请求，但这可以保证连接的完整性*/
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            fprintf(stderr, "write error\n");
            return false;
        }
        bytes_to_send -= temp;
        m_write_idx += temp;
        bytes_have_send = m_write_idx;
        if (bytes_have_send >= reply.size()) { //数据写完了
            //init(); //长连接，重新初始化连接的内容

            //--------------由于使用了epolloneshot事件，一定要加上下面这句话，让其他线程有机会处理该连接-------------
            modfd(m_epollfd, m_sockfd, EPOLLIN);    //重置socket上的EPOLLONESHOT事件，让其他线程有机会继续处理这个socket
            break;
        }
    }
    init(); //数据传输完毕，清空连接缓存
    return true;
}

void conn::do_delete(const Json::Value& json)
{
    int userid = json["userId"].asInt();
    

    Json::Value ret;
    ret["cmd"] = "delete-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);

    if (db_conn->my_database_delete_user(userid)) {//返回一个登录成功的提示信息，并将用户状态修改为在线
        ret["success"] = 1;
    }
    else {
        ret["success"] = 0;
    }
   

    reply = ret.toStyledString();
 
    //copy to send buffer，这里有缓冲区溢出的风险，后期考虑加上判断
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';
}

void conn::do_find(const Json::Value& json) 
{
    printf("666666666666666666666666666666666666\n");
int userid = json["userId"].asInt();
   

    Json::Value ret;
    ret["cmd"] = "find-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);

    if (db_conn->my_database_find_user(userid)) {//返回一个登录成功的提示信息，并将用户状态修改为在线
        ret["success"] = 1;
    }
    else {
        ret["success"] = 0;
    }
    User u; //登录返回用户信息
    db_conn->my_database_user_information(u, userid);

    ret["user"] = userToJsonObj(u);
    reply = ret.toStyledString();
 
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';

}
void conn::do_tempreture(const Json::Value& json) 
{
    string username = json["name"].asString();
    int start_time= json["start"].asInt();
    int end_time = json["end"].asInt();

    Json::Value ret;
    ret["cmd"] = "tempreture-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);
    int tempre=-99;
    if (db_conn->my_database_tempreture_elec(username,start_time,end_time,tempre)) {//返回一个登录成功的提示信息，并将用户状态修改为在线
        ret["success"] = 1;
    }
    else {
        ret["success"] = 0;
    }
    ret["tempreture"] = tempre;

    reply = ret.toStyledString();

    //copy to send buffer，这里有缓冲区溢出的风险，后期考虑加上判断
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';

}
void conn::do_statu(const Json::Value& json) 
{
 string username = json["name"].asString();
    int start_time = json["start"].asInt();
    int end_time = json["end"].asInt();

    Json::Value ret;
    ret["cmd"] = "statu-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);
    int tempre = -99;
    if (db_conn->my_database_statu_elec(username, start_time, end_time, tempre)) {
        ret["success"] = 1;
    }
    else {
        ret["success"] = 0;
    }
    ret["statu"] = tempre;

    reply = ret.toStyledString();

    //copy to send buffer，这里有缓冲区溢出的风险，后期考虑加上判断
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';

}
void conn::do_add(const Json::Value& json) 
{


string username = json["name"].asString();
    int start_time = json["start"].asInt();
    int end_time = json["end"].asInt();
    int temp= json["tempreture"].asInt();
    int statu = json["statu"].asInt();
    Json::Value ret;
    ret["cmd"] = "add-reply";

    ChatDataBase* db_conn;
    connectionRAII connRAII(db_conn, db_pool);
    int tempre = -99;
    if (db_conn->my_database_add_elec(username, start_time, end_time, temp,statu)) {
        ret["success"] = 1;
    }
    else {
        ret["success"] = 0;
    }
  

    reply = ret.toStyledString();

    //copy to send buffer，这里有缓冲区溢出的风险，后期考虑加上判断
    memcpy(m_write_buf, reply.c_str(), reply.size());
    m_write_buf[reply.size()] = '\0';
}


