#ifndef USER_H
#define USER_H

#include <string>
using namespace std;
//匹配USER表的ORM类
class User
{
public:
    User(int mid = -1, string nm = "", string pwd = "", string st = "offline")
    {
        this->id = mid;
        this->name = nm;
        this->password = pwd;
        this->state = st;
    }

    void setId(int mid) { this->id = mid; }
    void setName(string nm) { this->name = nm; }
    void setPwd(string pwd) { this->password = pwd; }
    void setState(string st) { this->state = st; }

    int getId() { return this->id; }
    string getName() { return this->name; }
    string getPwd() { return this->password; }
    string getState() {return  this->state; }
protected:
    int id;
    string name;
    string password;
    string state;
};

#endif