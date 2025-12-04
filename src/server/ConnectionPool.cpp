#include "ConnectionPool.h"
#include <muduo/base/Logging.h> //使用muduo日志

// 获取连接池单例对象
ConnectionPool *ConnectionPool::getConncetionPool()
{
    static ConnectionPool pool;
    return &pool;
}

// 给外部提供接口，从连接池中获取一个可用的空闲连接
// 返回智能指针，智能指针析构的时候，会自动调用我们在lamda中定义的删除器
shared_ptr<MySQL> ConnectionPool::getConnection()
{
    unique_lock<mutex> lock(_queueMutex);

    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(_connectionTimeout);

    // 如果队列为空 则等待
    while (_connectionQue.empty())
    {
        // 等待 _connectionTimeout 毫秒
        if (_cv.wait_until(lock, deadline) == cv_status::timeout)
        {
            if (_connectionQue.empty())
            {
                LOG_ERROR << "Failed to get connection: Timeout!";
                return nullptr;
            }
        }
    }
    shared_ptr<MySQL> sp(_connectionQue.front(),
                         [&](MySQL *pcon)
                         {
                             // 删除器逻辑 在sp出作用域地时候回调
                             unique_lock<mutex> lock(_queueMutex);
                             pcon->refreshAliveTime(); // 刷新空闲时间
                             _connectionQue.push(pcon);
                             _cv.notify_all(); // 通知消费者有连接可用了
                         });
    _connectionQue.pop();
    return sp;
}

ConnectionPool::ConnectionPool()
{
    // 1.加载配置
    if (!LoadConfigFile())
    {
        return;
    }

    // 2.创建初始数量的连接
    for (int i = 0; i < _initSize; ++i)
    {
        MySQL *p = new MySQL();

        if (p->connect(_ip, _port, _username, _password, _dbname))
        {
            p->refreshAliveTime(); // 刷新开始空闲的起始时间
            _connectionQue.push(p);
            _connectionCnt++;
        }
        else
        {
            LOG_ERROR << "create mysql connection error!";
            delete p;
        }
    }
    thread ConnectionTask(std::bind(&ConnectionPool::produceConnectionTask, this));
    ConnectionTask.detach();
}
ConnectionPool::~ConnectionPool()
{
    // 释放队列里所有的连接
    unique_lock<mutex> lock(_queueMutex);
    while (!_connectionQue.empty())
    {
        MySQL *p = _connectionQue.front();
        _connectionQue.pop();
        delete p;
    }
}

// 运行在独立的线程中，负责生产新连接
// 如果队列空了且没有达到最大连接数，就自动创建
void ConnectionPool::produceConnectionTask()
{
    for (;;)
    {
        unique_lock<mutex> lck(_queueMutex);
        while (!_connectionQue.empty())
        {
            _cv.wait(lck);
        }

        if (_connectionCnt < _maxSize)
        {
            MySQL *p = new MySQL();
            if (p->connect(_ip, _port, _username, _password, _dbname))
            {
                p->refreshAliveTime(); // 刷新开始空闲的起始时间
                _connectionQue.push(p);
                _connectionCnt++;
                LOG_INFO << "Produced a new connection, total: " << _connectionCnt;
            }
            else
            {
                
                LOG_ERROR << "create mysql connection error!";
                delete p;
            }
        }
        _cv.notify_all();
    }
}

// 扫描超过maxIndleTime时间的空闲连接，进行多余的连接回收
void ConnectionPool::scannerConnectionTask()
{
    for (;;)
    {
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));
        unique_lock<mutex> lck(_queueMutex);
        while (_connectionCnt > _initSize)
        {
            MySQL *p = _connectionQue.front();
            if (_connectionQue.front()->getAliveTime() > (_connectionTimeout * 1000))
            {
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }
            else
            {
                break;
            }
        }
    }
}

bool ConnectionPool::LoadConfigFile()
{
    FILE *pf = fopen("mysql.conf", "r");
    if (pf == nullptr)
    {
        LOG_ERROR << "mysql.conf file is not exist!";
        return false;
    }
    while (!feof(pf))
    {
        char line[1204] = {0};
        fgets(line, 1024, pf);
        string str = line;
        int idx = str.find('=', 0);
        if (idx == -1) // 无效配置项
        {
            continue;
        }
        // password=123456\n
        int endidx = str.find('\n', idx);
        string key = str.substr(0, idx);
        string value = str.substr(idx + 1, endidx - idx - 1);
        if (!value.empty() && value.back() == '\r')
        {
            value.pop_back();
        }
        if (key == "ip")
            _ip = value;
        else if (key == "port")
            _port = atoi(value.c_str());
        else if (key == "username")
            _username = value;
        else if (key == "password")
            _password = value;
        else if (key == "dbname")
            _dbname = value;
        else if (key == "initSize")
            _initSize = atoi(value.c_str());
        else if (key == "maxSize")
            _maxSize = atoi(value.c_str());
        else if (key == "maxIdleTime")
            _maxIdleTime = atoi(value.c_str());
        else if (key == "connectionTimeout")
            _connectionTimeout = atoi(value.c_str());
    }
    LOG_INFO << "Config Loaded: ip=" << _ip << " port=" << _port << " initSize=" << _initSize;
    return true;
}