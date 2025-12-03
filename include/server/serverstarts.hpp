#ifndef SERVERSTARTS_H
#define SERVERSTARTS_H

#include<atomic>

//1.总结收到的请求数(衡量网络吞吐)
extern std::atomic<int> g_total_recv_requests;

//2. JSON 解析失败数(衡量粘包/脏比例数据)
extern std::atomic<int> g_json_parse_errors;

//3.成功的业务分发数量(衡量有效请求)
extern std::atomic<int> g_valid_requests;

//4.数据库成功连接数（衡量DB性能  核心指标）
extern std::atomic<int> g_db_connect_success_count;
#endif