#ifndef REDISCONNPOOL
#define REDISCONNPOOL

#include "../log/log.h"

#include <hiredis/hiredis.h>
#include <queue>
#include <assert.h>
#include <semaphore.h>
#include <mutex>


class RedisConnPool{

public:
    RedisConnPool();
    ~RedisConnPool();
    
    static RedisConnPool* Instance();

    redisContext * GetConn();
    void RedisRecycle(redisContext * r);

    void Init(const char * ip, int port, int ConnSize, const struct timeval t = {0,0});


private:
    std::queue<redisContext *> redis_q;
    int MaxConn_;
    
    // 设置信号灯
    sem_t semID_;

    //还是得要锁
    std::mutex mtx_;
    
};

#endif 