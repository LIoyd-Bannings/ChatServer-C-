#include "chatserver.hpp"
#include "json.hpp"
#include <functional>
#include <string>
#include "chatservice.hpp"
#include <muduo/base/Logging.h>
#include"serverstarts.hpp"
using namespace std;
using namespace placeholders;
using json = nlohmann::json;
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg) : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);
}

void ChatServer::start() // 启动服务
{
    _server.start();
}
// 上报连接相关的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 【埋点1】收到任何数据都+1
    g_total_recv_requests++;
    if (buf.empty())
    {
        return;
    }
    json js;
    try
    {
        js = json::parse(buf);
        // if (!js.contains("msgid") )
        // {
        //     LOG_ERROR << "Request JSON invalid (no msgid ): " << buf;
        //     return; // 忽略这个非法请求，不要崩
        // }

        //=json::parse(buf);
        // 数据的反序列化

        // 达到的目的：完全解耦网络模块的代码和业务模块的代码
        // 通过js["msgid"]获取=》业务handler =》conn js time
        int msgid = js.at("msgid").get<int>();

        // 【埋点2】能走到这里，说明是合法的有效请求
        g_valid_requests++;

        auto msgHandler = ChatService::instance()->getHandler(msgid);
        // 回调消息绑定好的事件处理器 来实行相应的业务处理["msgid"]
        msgHandler(conn, js, time);
    }
    //catch (const json::exception &e)
    //{
       // LOG_ERROR << "JSON Parse Error: " << e.what() << " | Received Data: " << buf;
       // // 解析失败，不要让服务器挂掉，而是记录日志并断开这个有问题的连接
       // //conn->shutdown();
       // return;
    // }
     catch (const std::exception &e)
    {
    // 【埋点3】发生异常（解析失败、字段缺失、类型错误等）
        g_json_parse_errors++;
        // 捕获其他可能的标准异常
        //LOG_ERROR << "Std Exception: " << e.what();
    }
    catch (...)
    {
        // 【埋点3】发生异常，说明是无效数据（粘包或格式错）
        g_json_parse_errors++;
        //LOG_ERROR << "Unknown Exception caught in onMessage";
    }

}