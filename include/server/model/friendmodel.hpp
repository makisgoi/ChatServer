#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include <vector>
#include "user.hpp"
using namespace std;

class FriendModel{
public:
    //添加好友
    void insert(int userid, int friendid);
    //返回好友列表
    vector<User> query(int userid);
};


#endif