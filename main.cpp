#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "conn.h"
#include "ChatDataBase.h"
#include "DbPool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

//Ԥ��Ϊÿ�����ܵĿͻ����ӷ���һ��conn����
conn* conns = new conn[MAX_FD];
DbPool* db_pool = DbPool::getInstance();

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("usage: %s port_number\n", argv[0]);
        return 1;
    }
    else
    {

        printf("wait��%s port\n", argv[1]);

    }

    // const char *ip = argv[1];
    int port = atoi(argv[1]); //�˿ں�

    //����SIGPIPE�ź�
    addsig(SIGPIPE, SIG_IGN);

    //�����̳߳�
    threadpool<conn>* pool = NULL;
    try {
        pool = new threadpool<conn>;
    }
    catch (...) {
        fprintf(stderr, "catch some exception when create threadpool.\n");
        return 1;
    }

    //void DbPool::init(string ip, string user, string password, string dbname, int port, int maxConn)
    db_pool->init("192.168.43.181", "martin", "123456", "mini", 3306, 8);   //�������ݿ����ӳ�

    assert(conns);
    int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = { 1, 0 };
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);    //add listenfd to epollfd
    conn::m_epollfd = epollfd;
    printf("wait for the client...\n");
    while (true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failure\n");
            break;
        }

        //epoll ���أ��������سɹ����¼��б���ÿ���¼��ֱ��ж�
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {//�����û���������
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address,
                    &client_addrlength);
                if (connfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (connfd >= MAX_FD) {
                    fprintf(stderr, "connfd >= MAX_FD\n");
                    continue;
                }
                if (conn::m_user_count >= MAX_FD) {
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                conns[connfd].init(connfd, client_address);
                printf("Debug info: accept one connect, connection count = %d\n", conn::m_user_count);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //������쳣��ֱ�ӹرտͻ�������
                conns[sockfd].close_conn();
                printf("Debug info: user count = %d\n", conn::m_user_count);
            }
            else if (events[i].events & EPOLLIN) {
                //���ݶ��Ľ����������������ӵ��̳߳أ����ǹر�����
                if (conns[sockfd].read()) {//��ȡ�ͻ��˴��͵�����
                    printf("read_buf:\n %s", conns[sockfd].m_read_buf);
                    pool->append(conns + sockfd);   //���ͻ��˵��������������У��ò����ỽ���̳߳��е�һ���߳�Ϊ�ÿͻ��˽��д���
                }
                else {
                    printf("Debug info: user count = %d\n", conn::m_user_count);
                    conns[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT) {
                //����д�Ľ���������Ƿ�ر�����
                if (!conns[sockfd].process_write()) {
                    conns[sockfd].close_conn();
                    printf("Debug info: user count = %d\n", conn::m_user_count);
                }
            }
            else {
                fprintf(stderr, "epoll return from an unexpected event\n");
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] conns;
    delete pool;
    delete db_pool;
    return 0;
}
