//
// Created by Administrator on 2021/3/16.
//

#ifndef NEW_POST_CHATDATABASE_H
#define NEW_POST_CHATDATABASE_H
#include <mysql/mysql.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include "locker.h"
#include "common.h"
using namespace std;
struct userInfo {
    int id;
    string name;
    string self_signature;
    string picture;
};

class ChatDataBase {
private:
    MYSQL* mysql;
    MYSQL_RES* result;    //指向查询结果的指针
    MYSQL_ROW row;
public:
    ChatDataBase();
    ~ChatDataBase();
    bool initDb(string host, string user, string passwd, string db_name);
    bool exeSQL(string sql);
    // 数据库连接和 去连接
    void my_database_connect(const char* name);//OK

    ////用户注册
    void my_database_user_password(int userID, string password, int& userid); //OK
    ////判断用户是不是在线
    //bool my_database_user_state(int userid);
    ////获取用户信息
    void my_database_user_information(User& ur, int);

    ////用户登录判断
    bool my_database_password_correct(int id, string password); //OK
     bool my_database_delete_user(int id);
    bool my_database_find_user(int id); 
       bool my_database_tempreture_elec(string username, int start_time, int end_time, int& tempre);	
	bool my_database_statu_elec(string username, int start_time, int end_time, int& tempre);	
	bool my_database_add_elec(string username, int start_time, int end_time, int temp, int statu);
};


#endif //NEW_POST_CHATDATABASE_H

