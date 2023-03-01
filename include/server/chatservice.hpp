#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include "json.hpp"
#include "usermodel.hpp"
#include <mutex>
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace nlohmann;

using MsgHandler = std::function<void(const TcpConnectionPtr&, json&, Timestamp)>;

class ChatService
{
public:
    static ChatService* instance();
    MsgHandler getHandler(int msgid);
    void login(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void clientCloseException(const TcpConnectionPtr& conn);
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void reset();
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void handleRedisSubscribeMessage(int userid, string msg);

private:
    ChatService();
    //消息id和对应的事件处理器
    unordered_map<int, MsgHandler> _msgHandlerMap;
    //在线用户的连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    //定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    Redis _redis;
};

#endif