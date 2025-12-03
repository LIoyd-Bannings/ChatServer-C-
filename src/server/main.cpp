#include"chatserver.hpp"
#include"chatservice.hpp"
#include<iostream>
#include<signal.h>
#include<mysql/mysql.h>
#include"serverstarts.hpp"
using namespace std;

//监控线程函数线程
void monitorTask()
{
    while(true)
    {
        //每秒运行一次
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 获取当前这一秒的数据（获取后重置为0，显示的是瞬时速度）
        int recv_qps = g_total_recv_requests.exchange(0);
        int valid_qps = g_valid_requests.exchange(0);
        int error_qps = g_json_parse_errors.exchange(0);
        int db_qps = g_db_connect_success_count.exchange(0);

        std::cout << "---------------------------------------" << std::endl;
        std::cout << "[Monitor] Total Recv QPS : " << recv_qps << " (Network Load)" << std::endl;
        std::cout << "[Monitor] JSON Error QPS : " << error_qps << " (Sticky/Bad Data)" << std::endl;
        std::cout << "[Monitor] Valid Req  QPS : " << valid_qps << " (Logic Process)" << std::endl;
        std::cout << "[Monitor] DB Connect QPS : " << db_qps << " (Real DB Performance)" << std::endl;
        std::cout << "---------------------------------------" << std::endl;
    }
}

//处理服务器ctrl+c 结束后 重置user的状态信息的
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc,char**argv)
{
    if(argc<3)
    {
        cerr<<"command invalid example:./ChatServer 127.0.0.1 6000"<<endl;
    }

    char *ip=argv[1];
    uint16_t port=atoi(argv[2]);

    signal(SIGINT,resetHandler);

    //先进行全局的初始化
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        std::cerr << "could not initialize mysql library" << std::endl;
        exit(1);
    }
    //启动监控线程  让他在后台自己运行
    std::thread t(monitorTask);
    t.detach();


    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();
    return 0;
}