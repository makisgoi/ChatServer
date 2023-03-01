#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

using namespace muduo;
using namespace muduo::net;


class ChatServer{
public:
    ChatServer(EventLoop* loop, //reactor
            const InetAddress& listenAddr, //ipAddress and portID
            const string& nameArg); //server's name
            
    void start();
private:
    void onConnection(const TcpConnectionPtr&);
    void onMessage(const TcpConnectionPtr&, Buffer*, Timestamp);
    TcpServer _server;
    EventLoop* _loop; //epoll
};


#endif

