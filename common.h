
#ifndef COMMON_H__
#define COMMON_H__
#include <string>
#include "json.h"

using namespace std;


struct User {
    int userId;
    string userName;
    string desc;
    int online;
};


Json::Value userToJsonObj(User user);



#endif


