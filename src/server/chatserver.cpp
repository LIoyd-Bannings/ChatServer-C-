#include "chatserver.hpp"
#include "json.hpp"
#include <functional>
#include <string>
#include "chatservice.hpp"
#include <muduo/base/Logging.h>
#include "serverstarts.hpp"
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

    // 循环读取缓冲区，直到剩下的数据不足以组成一个完整的包
    while (buffer->readableBytes() >= 4)
    {
        // 2. 读取长度头 (peekInt32 读取4字节并转为本机字节序)
        // peek 只是“看一眼”，不会把数据从缓冲区移除
        int32_t len = buffer->peekInt32();

        // 如果包长度大得离谱（比如超过 10MB），说明读到了垃圾数据，直接关掉连接
        if (len > 1024 * 1024 * 10 || len < 0) 
        {
             LOG_ERROR << "Invalid package length: " << len;
             conn->shutdown(); // 遇到恶意包/错误包，主动断开
            break;
        }       




        if (buffer->readableBytes() < 4 + len)
        {
            // 数据不够（半包），直接 break，等待下次 TCP 收到更多数据再来处理
            // Muduo 会自动保留现有数据，下次拼接后再回调 onMessage
            break;
        }

        // 4. 数据足够了！开始拆包
        // 先把这4字节头从缓冲区移除
        buffer->retrieve(4);

        // 再把 len 长度的数据读出来转成 string
        string json_str = buffer->retrieveAsString(len);
        // 【埋点1】收到任何数据都+1
        g_total_recv_requests++;

        try
        {
            json js = json::parse(json_str);
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
        // catch (const json::exception &e)
        //{
        //  LOG_ERROR << "JSON Parse Error: " << e.what() << " | Received Data: " << buf;
        //  // 解析失败，不要让服务器挂掉，而是记录日志并断开这个有问题的连接
        //  //conn->shutdown();
        //  return;
        // }
        catch (const std::exception &e)
        {
            // 【埋点3】发生异常（解析失败、字段缺失、类型错误等）
            g_json_parse_errors++;
            // 捕获其他可能的标准异常
            // LOG_ERROR << "Std Exception: " << e.what();
        }
        catch (...)
        {
            // 【埋点3】发生异常，说明是无效数据（粘包或格式错）
            g_json_parse_errors++;
            // LOG_ERROR << "Unknown Exception caught in onMessage";
        }
    }
}