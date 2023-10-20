
#include <string>
#include <vector>
#include "ChatDataBase.h"
ChatDataBase::ChatDataBase()
{
    /*my_database_connect("tengxun");*/
    mysql = mysql_init(NULL);
    if (mysql == NULL)
    {
        cout << "Error:" << mysql_error(mysql);
        exit(1);
    }
}

ChatDataBase::~ChatDataBase()
{
    /*mysql_close(mysql);*/
    if (mysql != NULL)  //关闭数据连接
    {
        mysql_close(mysql);
    }
}

void ChatDataBase::my_database_connect(const char* name)
{
    mysql = mysql_init(NULL);
    cout << "111" << endl;
    mysql = mysql_real_connect(mysql, "121.36.69.144", "root", "123456", name, 3306, NULL, 0); \
        cout << "111" << endl;
    if (NULL == mysql)
    {
        cout << "connect database failure" << endl;
    }
    else {
        cout << "connect database success" << endl;
    }

    if (mysql_query(mysql, "set names utf8;") != 0)
    {
        cout << "mysql_query error" << endl;
    }
    else
    {
        cout << "set name utf8 success and mysql_query success" << endl;
 //       sprintf(sql, "INSERT INTO T_ELEC (name, start_time, end_time, tempreture, statu) VALUES ('%s', %d, %d, %d, %d);", username.c_str(), start_time, end_time, temp, statu);
    }

}
//
void ChatDataBase::my_database_user_password(int userID, string password, int& userid)
{
    char sql[128] = { 0 };
//    sprintf(sql, "insert into T_USER (f_user_id,f_password,f_user_name,f_online,f_signature) values('%d','%s','%s','%d','%s);",userID, password.c_str(),"2", 0, "happy");

sprintf(sql, "insert into T_USER (f_user_id, f_password, f_user_name, f_online, f_signature) values(%d, '%s', '2', %d, '%s');", userID, password.c_str(), 0, "happy");

    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql);
        return;
    }
    memset(sql, 0, sizeof(sql));
    
    sprintf(sql, "SELECT max(f_user_id) from T_USER ;");
    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql);
        return;
    }
    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    userid = stoi(row[0]);
}

bool ChatDataBase::my_database_password_correct(int id, string password)
{
    char sql[128] = { 0 };
    sprintf(sql, "select f_password from T_USER where f_user_id='%d';", id);
    if (mysql_query(mysql, sql) != 0)
    {
        cout << "mysql_query error" << endl;
    }
    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && password == row[0])
    {
        cout << "password true" << endl;
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "UPDATE T_USER SET f_online=1 where f_user_id='%d';", id);
        if (mysql_query(mysql, sql) != 0) {
            cout << "update user online information error" << endl;
            return false;
        }
        else
        {
            cout << "update user online information success" << endl;
            return true;
        }
    }
    else
    {
        cout << "user no exist or password false" << endl;
        return false;
    }
}



void ChatDataBase::my_database_user_information(User& ur, int userid) {
    char sql[1024] = { 0 };
    sprintf(sql, "select f_user_id,f_user_name,f_online,f_signature from T_USER where f_user_id='%d'", userid);
    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql);
        return;
    }
    auto res = mysql_store_result(mysql);
    auto numField = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        for (int i = 0; i < numField; i++)
        {
            if (i == 0)
                ur.userId = stoi(row[i]);
            if (i == 1)
                ur.userName = row[i];
            if (i == 2)
                ur.online = stoi(row[i]);
            if (i == 3)
                ur.desc = row[i];
        }
    }
}

bool ChatDataBase::initDb(string host, string user, string passwd, string db_name) {
    mysql = mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), 3306, NULL, 0);
    if (mysql == NULL)
    {
        cout << "Error: " << mysql_error(mysql);
        exit(1);
    }
    return true;
}

bool ChatDataBase::exeSQL(string sql) {
    if (mysql_query(mysql, sql.c_str()))
    {
        cout << "Query Error: " << mysql_error(mysql);
        return false;
    }
    else // 查询成功
    {
        result = mysql_store_result(mysql);  //获取结果集
        if (result)  // 返回了结果集
        {
            int  num_fields = mysql_num_fields(result);   //获取结果集中总共的字段数，即列数
            int  num_rows = mysql_num_rows(result);       //获取结果集中总共的行数
            for (int i = 0; i < num_rows; i++) //输出每一行
            {
                //获取下一行数据
                row = mysql_fetch_row(result);
                if (row < 0) break;

                for (int j = 0; j < num_fields; j++)  //输出每一字段
                {
                    cout << row[j] << "\t\t";
                }
                cout << endl;
            }

        }
        else  // result==NULL
        {
            if (mysql_field_count(mysql) == 0)   //代表执行的是update,insert,delete类的非查询语句
            {
                // (it was not a SELECT)
                int num_rows = mysql_affected_rows(mysql);  //返回update,insert,delete影响的行数
                printf("%s execute successed! affect %d rows\n", sql.c_str(), num_rows);
            }
            else // error
            {
                cout << "Get result error: " << mysql_error(mysql);
                return false;
            }
        }
    }
    return true;
}



bool ChatDataBase::my_database_find_user(int id) 
{
   
        char sql[128] = { 0 };
        sprintf(sql, "SELECT * FROM T_USER WHERE f_user_id = %d;", id);
        if (mysql_query(mysql, sql))
        {
            cout << "Query Error: " << mysql_error(mysql);
            return -1; // 返回-1 表示出现错误
        }
        MYSQL_RES* res = mysql_store_result(mysql);

        // 如果查询结果为空，表示未找到用户
        if (mysql_num_rows(res) == 0)
        {
            return 0; // 返回0 表示未找到用户
        }

        // 找到用户
        return 1; // 返回1 表示找到用户

}



bool ChatDataBase::my_database_delete_user(int id)
{
    char sql[128] = { 0 };
    sprintf(sql, "DELETE FROM T_USER WHERE f_user_id = %d;", id);
    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql);
        return false; // 返回失败状态
    }
    return true; // 返回成功状态
}

bool ChatDataBase::my_database_tempreture_elec(string name, int start_time, int end_time, int& tempre)
{
    char sql[256] = { 0 };
    sprintf(sql, "SELECT tempreture FROM T_ELEC WHERE name='%s' AND start_time <= %d AND end_time >= %d;", name.c_str(), start_time, end_time);

    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql) << endl;
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysql);

    if (mysql_num_rows(res) == 0)
    {
        return false; // 未找到记录
    }

    // 从结果集中提取温度值
    MYSQL_ROW row = mysql_fetch_row(res);
    tempre = atoi(row[0]);

    return true; // 找到记录并提取温度值
}

bool ChatDataBase::my_database_statu_elec(string username, int start_time, int end_time, int& tempre)
{
    char sql[256] = { 0 };
    sprintf(sql, "SELECT statu FROM T_ELEC WHERE name='%s' AND start_time <= %d AND end_time >= %d;", username.c_str(), start_time, end_time);

    if (mysql_query(mysql, sql))
    {
        cout << "Query Error: " << mysql_error(mysql) << endl;
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysql);

    if (mysql_num_rows(res) == 0)
    {
        return false; // 未找到记录
    }

    // 从结果集中提取温度值
    MYSQL_ROW row = mysql_fetch_row(res);
    tempre = atoi(row[0]);

    return true; // 找到记录并提取温度值

}



bool ChatDataBase::my_database_add_elec(string username, int start_time, int end_time, int temp, int statu) 
{

        char sql[256] = { 0 };
        sprintf(sql, "INSERT INTO T_ELEC (name, start_time, end_time, tempreture, statu) VALUES ('%s', %d, %d, %d, %d);", username.c_str(), start_time, end_time, temp, statu);

        if (mysql_query(mysql, sql))
        {
            cout << "Query Error: " << mysql_error(mysql) << endl;
            return false;
        }

        return true; // 插入成功
   

}
