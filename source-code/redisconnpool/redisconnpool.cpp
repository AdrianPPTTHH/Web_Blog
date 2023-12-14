#include "redisconnpool.h"


RedisConnPool::RedisConnPool(){
    
}


RedisConnPool::~RedisConnPool(){
    while(!redis_q.empty()){
        redisContext * tmp = redis_q.front();
        redisFree(tmp);
        tmp == NULL;
    }

}

RedisConnPool* RedisConnPool::Instance(){
    static RedisConnPool * conn_redis = new RedisConnPool();
    return conn_redis;
}




void RedisConnPool::Init(const char * ip, int port, int connSize, const struct timeval t){
    assert(connSize > 0);
    MaxConn_ = connSize;

    if(t.tv_sec != 0 || t.tv_usec != 0){
        for(int i = 0 ; i < MaxConn_ ; i++){
            redisContext *r = redisConnectWithTimeout(ip, port, t);
            assert(r != NULL && ! r->err);
            redis_q.push(std::move(r));
        }
    }else{
        for(int i = 0; i < MaxConn_; i++){
            redisContext *r = redisConnect(ip, port);
            assert(r != NULL && ! r->err);
            redis_q.push(std::move(r));
        }
    }

    //还是使用信号器来搞   0是线程  1是进程
    sem_init(&semID_, 0, MaxConn_);

}


redisContext * RedisConnPool::GetConn(){
    if(redis_q.empty()){
        LOG_INFO("redis busy!");
        return nullptr;
    }

    sem_wait(&semID_);
    
    
    std::unique_lock<std::mutex> lck(mtx_);
    redisContext * r = redis_q.front();
    redis_q.pop();

    return r;
    
}


void RedisConnPool::RedisRecycle(redisContext * r){
    assert(r);
    
    std::unique_lock<std::mutex> lck(mtx_);
    
    redis_q.push(r);
    
    sem_post(&semID_);
    
}