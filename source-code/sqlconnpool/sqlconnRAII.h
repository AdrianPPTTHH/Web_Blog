#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H

#include "sqlconnpool.h"

class SqlConnRAII{

public:

    SqlConnRAII(MYSQL ** sql, SqlConnPool *connpool){
        assert(connpool);

        *sql = connpool->GetConn();

        sql_ = *sql;

        connpool_ = connpool;
    }

    ~SqlConnRAII(){
        if(sql_){
            connpool_->RecyConn(sql_);
        }
    }

    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif