#include "common.h"
#include "json.h"

/*
struct User{
    int userId;
    string userName;
    string iconStr;
    string desc;
    int online;
};
*/

Json::Value userToJsonObj(User user) {
    Json::Value obj;
    obj["userId"] = user.userId;
    obj["userName"] = user.userName;
    obj["desc"] = user.desc;
    obj["online"] = user.online;
    return obj;
}


