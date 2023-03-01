#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>

using namespace std;
using namespace muduo;

ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

ChatService::ChatService(){
    //LOGIN_MSG = 1, LOGIN_MSG_ACK, REG_MSG, REG_MSG_ACK, ONE_CHAT_MSG, 
    //ADD_FRIEND_MSG, CREATE_GROUP_MSG, ADD_GROUP_MSG, GROUP_CHAT_MSG
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    //连接redis服务器
    if (_redis.connect()) {
        //初始化回调函数
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

void ChatService::reset(){
    _userModel.resetState();
}


void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    json response;
    if (user.getId() == id && user.getPwd() == pwd){
        if (user.getState() == "online"){
            //该用户重复登陆
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "This account is online.";
        }
        else{
            //登录成功，将用户的连接添加进_userConnMap
            //大括号为互斥锁的作用范围
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }            
            //登陆成功，向redis订阅该channel
            _redis.subscribe(id);

            //登录成功，更新state为online
            user.setState("online");
            _userModel.updateState(user);
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty()){
                response["offlinemessage"] = vec;
                _offlineMsgModel.remove(id);
            }
            //查询好友列表信息
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty()){
                vector<string> vec2;
                for (User& user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.emplace_back(js.dump());
                }
                response["friends"] = vec2;
            }
        }
    }
    else{
        //该用户不存在（数据库里不存在此id） or 密码错误（密码和数据库里的对应不上）
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "Id or password is wrong";
    }
    conn->send(response.dump());
}
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time){
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    //_userModel把 user的信息，插入进数据库的 User表中
    bool state = _userModel.insert(user);
    json response;
    if(state){
        //reg succeed
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
    }
    else{
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
    }
    conn->send(response.dump());
}

//get messagehandler
MsgHandler ChatService::getHandler(int msgid){
    if (_msgHandlerMap.find(msgid) == _msgHandlerMap.end()){
        //lamda表达式
        /*
        return [=](const TcpConnection &conn, json& js, Timestamp){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
        */
       LOG_ERROR << "msgid:" << msgid << " can not find handler!";
    }
    else{
        return _msgHandlerMap[msgid];
    }

    return _msgHandlerMap[msgid];
}

//客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if (it->second == conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //从redis中取消订阅
    _redis.unsubscribe(user.getId());

    //更新状态信息
    if (user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }
    //从redis中取消订阅该channel
    _redis.unsubscribe(userid);
    //更新状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//一对一聊天
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        //如果用户在线，直接把消息转发出去
        if (it != _userConnMap.end()){
            it->second->send(js.dump());
            return;
        }
    }
    /*
    单台服务器
    用户不在线，存储到离线消息表
    _offlineMsgModel.insert(toid, js.dump());
    */

   //查询 user表，看用户是否在线
    User user = _userModel.query(toid);
    if (user.getState() == "online") {
        //用户在线，把消息 publish 给 redis队列
        _redis.publish(toid, js.dump());
        return;
    }
    else {
        //用户不在线
        _offlineMsgModel.insert(toid, js.dump());
    }
    

}

void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid, friendid);
}

void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);
    //更新allgroup表和groupuser表
    if (_groupModel.createGroup(group)){
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
    
}

void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
	//对_userConnMap的操作，要加锁；之所以加在这，而不加在for循环内，是因为避免频繁地加锁解锁
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            /*
            单台服务器
            存储离线群消息
            _offlineMsgModel.insert(id, js.dump());
            */
            
           User user = _userModel.query(id);
            if (user.getState() == "online") {
                //用户在线，把消息 publish 给 redis队列
                _redis.publish(id, js.dump());
                return;
            }
            else {
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

//redis notify的回调函数，将redis notify的消息发送给用户
void ChatService::handleRedisSubscribeMessage(int userid, string msg) {
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end()) {
        it->second->send(msg);
        return;
    }
    else{
        _offlineMsgModel.insert(userid, msg);
    }
}