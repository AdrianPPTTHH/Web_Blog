#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include "../log/log.h"

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <assert.h>
#include <iostream>

class SqlConnPool{
public:
    // 单例
    static SqlConnPool *Instance();
    
    // 初试化sql连接池（创建sql连接）
    void Init(const char* host, int port,
    const char* user, const char* passwd,
    const char* database, int connSize);

    // 返回MYSQL对象
    MYSQL *GetConn();
    
    int GetFreeConnCount();

    // 用完sql连接之后，回收sql连接，重新入池
    void RecyConn(MYSQL * sql);
    
    void ClosePool();

private:
    // 构造函数
    SqlConnPool();
    ~SqlConnPool();
    
    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool &operator=(const SqlConnPool&) = delete;
    
    int MAX_CONN_;
    int useCount_;
    int freeCount_;
    
    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;

    // 信号量 用于协调多个线程访问资源，防止太多线程同时访问mysql
    sem_t semID_;

};


#endif