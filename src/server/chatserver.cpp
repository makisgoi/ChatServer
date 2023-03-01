#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"
#include <functional>
#include <string>

using namespace std;
using namespace placeholders;
using namespace nlohmann;

ChatServer::ChatServer(EventLoop *loop,               // reactor
                       const InetAddress &listenAddr, // ipAddress and portID
                       const string &nameArg)         // server's name
    : _server(loop, listenAddr, nameArg),
      _loop(loop)
{
    //连接的回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    //可读写事件的回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    // set threads' number
    _server.setThreadNum(4);
}
void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{   
    //客户端断开连接
    if (!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
    ////提取出缓冲区的消息，放在string，再转成json
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);
}
