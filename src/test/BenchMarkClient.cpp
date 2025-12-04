#include"json.hpp"
#include<iostream>
#include<thread>
#include<string>
#include<unistd.h>
#include<vector>
#include<chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<cstring>
#include"public.hpp"
#include<atomic>
using namespace std;
using json=nlohmann::json;

const int thread_num=10;// 线程数 (模拟并发用户数)
const int msg_per_thread=100000;// 每个用户发送的消息数量
const string server_ip="127.0.0.1";
const int server_port=6000;

atomic<int> g_tx_count{0};// 发送成功的消息数
atomic<long long> g_total_cost{0};// 总耗时 (微秒)


bool sendProto(int fd ,string json_str)
{
    uint32_t len=htonl(json_str.size());
    string sendBuf;
    sendBuf.resize(4 + json_str.size());
    memcpy(&sendBuf[0],&len,4);
    memcpy(&sendBuf[4],json_str.data(),json_str.size());
    size_t total_sent=0;
    size_t to_send=sendBuf.size();
    const char* ptr=sendBuf.data();

    while(total_sent<to_send)
    {
        ssize_t sent=send(fd,ptr+total_sent,to_send-total_sent,0);
        if(sent==-1)
        {
            if(errno==EINTR)continue;
            if(errno==EAGAIN||errno==EWOULDBLOCK)
            {   
                usleep(1000);
                continue;
            }
            return false; 
        }
        if (sent == 0) 
        {
            return false;
        }
        total_sent += sent;
    }

    return true;
}



//获取当前时间的字符串
string getCurrentTime()
{
    auto tt=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm=localtime(&tt);
    char date[60]={0};
    sprintf(date,"%d-%02d-%02d  %02d:%02d:%02d",(int)ptm->tm_year+1900,(int)ptm->tm_mon+1,(int)ptm->tm_mday,
    (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return std::string(date);
}   

//压测线程函数
//startId:该线程模拟的其实用户ID（需要在数据库里预先存在）
void benchmarkTask(int userId)
{
    //1.创建socket
    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(-1==clientfd)
    {
        cerr<<"socket create error"<<endl;
        return;
    }
    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));
    server.sin_family=AF_INET;
    server.sin_port=htons(server_port);
    server.sin_addr.s_addr=inet_addr(server_ip.c_str());

    //2.链接server
    if(-1==connect(clientfd,(sockaddr*)&server,sizeof(sockaddr_in)))
    {
        cerr<<"connect server error"<<endl;
        close(clientfd);
        return;
    }

    //3.执行记录（为了触发DB查询）
    //注意  压测前请确保数据库里有id=userId pwd=123456的数据
    json js;
    js["msgid"]=LOGIN_MSG;
    js["id"]=userId;
    js["password"]="123456";
    string request=js.dump();
    if(!sendProto(clientfd, request))
    {
        cerr << "send login error" << endl;
        close(clientfd);
        return;
    }

    //阻塞等待登陆结果（同步读取）
    char buffer[1024]={0};
    int len=recv(clientfd,buffer,1024,0);
    if(len<=0)
    {
        cerr << "recv login error" << endl;
        close(clientfd);
        return;
    }
    json response=json::parse(buffer);
    if(response["error"].get<int>()!=0)
    {
    // 修改这行打印，把 errmsg 显示出来！
    cerr << "Login failed for user: " << userId 
         << " | Error=" << response["error"] 
         << " | Msg=" << response.value("errmsg", "unknown") << endl;
        close(clientfd);
        return;
    }

    //4.循环发送消息（触发db写入/轮询）
    for(int i=0;i<msg_per_thread;++i)
    {
        json chatjs;
        chatjs["msgid"]=ONE_CHAT_MSG;
        chatjs["id"]=userId;
        chatjs["name"]="BenchmarkUser";
        chatjs["toid"]=userId+1000;
        chatjs["msg"]="hello connection pool test"+to_string(i);
        chatjs["time"]=getCurrentTime();
        string chatReq=chatjs.dump();

        auto start=chrono::high_resolution_clock::now();
        //int sendlen=send(clientfd,chatReq.c_str(),strlen(chatReq.c_str()),0);
        if(sendProto(clientfd, chatReq))
        {   
            auto end=chrono::high_resolution_clock::now();
            auto cost=chrono::duration_cast<chrono::microseconds>(end-start).count();
            g_total_cost+=cost;
            g_tx_count++;
        }
        else
        {
            cerr << "send msg error" << endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    close(clientfd);
}

int main(int argc,char**argv)
{
    cout << "========================================" << endl;
    cout << "  Starting ChatServer Benchmark...  " << endl;
    cout << "  Threads: " << thread_num << ", Msgs/Thread: " << msg_per_thread<< endl;
    cout << "========================================" << endl;

    vector<thread>threads;
    auto start_time=chrono::high_resolution_clock::now();

    //启动多线程
    for(int i=0;i<thread_num;++i)
    {
        //假设数据库有id为1到20的用户
        //这里i+1代表userid
        threads.emplace_back(benchmarkTask,i+1);
    }
    for(auto &t:threads)
    {
        t.join();
    }
    auto end_time=chrono::high_resolution_clock::now();

    std::chrono::duration<double>diff=end_time-start_time;
    double total_time_sec = diff.count();

    //auto duration_sec=chrono::duration_cast<chrono::seconds>(end_time-start_time).count();
    //统计结果
    long total_req=g_tx_count;
    double qps=0;
    if(total_time_sec>0)
    {   
        qps=total_req/(double)total_time_sec;
    }
    double avg_latency_ms=0;
    if(total_req>0)
    {
        avg_latency_ms=(g_total_cost/1000.0)/(double)total_req;
    }
   cout << "================ Result ================" << endl;
    cout << "Total Requests Sent: " << total_req << endl;
    cout << "Total Time Consumed: " << total_time_sec << " s" << endl;
    cout << "QPS (Queries/sec):   " << qps << endl;
    cout << "Avg Latency:         " << avg_latency_ms << " ms" << endl;
    cout << "========================================" << endl;

    return 0;
}