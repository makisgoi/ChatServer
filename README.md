# ChatServer

## 1 项目简介
该项目是在 Linux 下使用 C++语言开发的集群聊天服务器，服务器支持用户登录、新用户注册、添加好友和群组、离线消息显示、好友聊天、群组聊天等功能，且实现了服务器的集群和跨服务器通信功能。

## 2 主要内容
- 使用 muduo 网络库作为项目的网络核心模块，提供高并发网络 I/O 服务，实现网络模块和业务模块的解耦；
- 利用 JSON 对通信的消息进行序列化和反序列化；
- 利用 MySQL 关系型数据库实现项目中数据的存储；
- 配置 nginx 基于 TCP 的负载均衡，实现聊天服务器的集群功能，提高后端服务的并发能力；
- 利用基于 Redis 的发布-订阅模式，实现跨服务器的消息通信。


## 3 需要配置的环境
### 3.1 nginx的负载均衡模块
**step1 安装nginx**

**step2 修改配置文件**

`cd nginx/conf`

`vim nginx.conf`

在event和http之间加上如下内容：

```
# nginx tcp loadbalance config
stream {
	upstream MyServer {
		server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
		server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
	}

	server {
		proxy_connect_timeout 1s;
        # nginx监听8000端口，故客户端向8000端口发送请求
		listen 8000;
		proxy_pass MyServer;
		tcp_nodelay on;
	}
}
```

**step3 启动nginx**

`cd nginx/sbin`

`ls`

`nginx`

### 3.2 redis
**step1 安装redis**

**step2 启动redis服务器和客户端**

启动服务器：`./redis-server redis.conf`
启动客户端：`./redis-cli`

**step3 订阅/发布**

订阅： `subscribe <channelName>`

发布：`publish <channelName> sth `

### 3.3 MySQL

安装MySQL

## 4 编译命令
`cd build`

`rm -rf *`

`cmake ..`

`make`
