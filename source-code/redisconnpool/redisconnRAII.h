#ifndef REDISCONNRAII
#define REDISCONNRAII

#include "redisconnpool.h"

#include <hiredis/hiredis.h>

class RedisConnRAII{

public:
    RedisConnRAII(redisContext ** r, RedisConnPool * conn){
        
        // 将传入的 **r 给初始化
        *r = conn->GetConn();

        // 将*r 和 *conn 放入RAII  RAII析构的时候 也会将r_给返回去
        r_ = *r;
        
        redisconn_ = conn;
        
    }

    ~RedisConnRAII(){
        if(r_){
            redisconn_->RedisRecycle(r_);
        }
    }


private:
    redisContext * r_;
    RedisConnPool * redisconn_;
};



#endif