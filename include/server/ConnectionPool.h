#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include"db.h"
#include<string>
#include<queue>
#include<iostream>
#include<thread>
#include<mutex>
#include<memory>
#include<functional>
#include<atomic>
#include<condition_variable>


using namespace std;



class ConnectionPool
{
public:
    //获取连接池单例对象
    static ConnectionPool* getConncetionPool();


    //给外部提供接口，从连接池中获取一个可用的空闲连接
    //返回智能指针，智能指针析构的时候，会自动调用我们在lamda中定义的删除器
    shared_ptr<MySQL>getConnection();

private:
    ConnectionPool();
    ~ConnectionPool();

    bool LoadConfigFile();

    //运行在独立的线程中，负责生产新连接
    //如果对裂空了且没有达到最大连接数，就自动创建
    void produceConnectionTask();

    //扫描超过maxIndleTime时间的空闲连接，进行多余的连接回收
    void scannerConnectionTask();

    string _ip;// mysql的ip地址
    unsigned short _port;// mysql的端口号 3306
    string _username;// mysql登陆用户名
    string _password;// mysql登陆密码
    string _dbname;// 连接的数据库名称

    int _initSize;// 连接池的初始连接量
    int _maxSize;// 连接池的最大连接量
    int _maxIdleTime;// 连接池最大空闲时间
    int _connectionTimeout;// 连接池获取连接的超时时间

    queue<MySQL*>_connectionQue;// 存储mysql连接的队列
    mutex _queueMutex;// 维护连接队列线程安全的互斥锁
    atomic_int _connectionCnt;// 记录连接所创建的connection连接的总数量
    condition_variable _cv;// 设置条件变量，用于连接生产线程和消费线程的通信
};


#endif