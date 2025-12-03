#include "offlinemessagemodel.hpp"
#include "db.h"
#include"ConnectionPool.h"
void offlineMsgModel::insert(int userid, string msg)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values('%d','%s')",
            userid, msg.c_str());

    //MySQL mysql;
    shared_ptr<MySQL>sp=ConnectionPool::getConncetionPool()->getConnection();
    if (sp!=nullptr)
    {
        sp->update(sql);
    }
}

// 删除用户的离线消息
void offlineMsgModel::remove(int userid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid=%d",
            userid);

    //MySQL mysql;
    shared_ptr<MySQL>sp=ConnectionPool::getConncetionPool()->getConnection();
    if (sp!=nullptr)
    {
        sp->update(sql);
    }
}

// 查询用户的离线消息
vector<string> offlineMsgModel::query(int userid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid= %d", userid);

    vector<string> vec;
    //MySQL mysql;
    shared_ptr<MySQL>sp=ConnectionPool::getConncetionPool()->getConnection();
    if (sp!=nullptr)
    {
        MYSQL_RES *res = sp->query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有离线消息放到vec中返回
            MYSQL_ROW row;
            while((row= mysql_fetch_row(res))!=nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}