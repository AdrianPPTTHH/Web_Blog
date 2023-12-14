#include "sqlconnpool.h"


// 编译阶段 编译器可以强制访问private  所以可以初始化构造函数
// 这里初试化的构造函数，Instance()可以用到
SqlConnPool::SqlConnPool(){
    useCount_ = 0;
    freeCount_ = 0;
}


SqlConnPool* SqlConnPool::Instance(){
    // 虽然Instance可能会被多次调用，但是下面这句话只会创建一个*connPool
    // 这是编译器的懒惰初始化机制

    // 这两种写法构建了connPool对象，调用构造函数
    // static SqlConnPool connPool;
    // return &connPool

    static SqlConnPool * connPool = new SqlConnPool();
    return connPool;
    
    
    // 以下写法是错误的  只是定义了一个SqlConnPool的指针，并且没有进行初始化，指向空，不会调用构造函数
    // static SqlConnPool *connPool;
    // return connPool;
}


void SqlConnPool::Init(const char* host, int port,
    const char* user, const char* passwd,
    const char* database, int connSize = 10){
        
        assert(connSize > 0);
        for(int i = 0; i < connSize; i++){
            MYSQL *sql = nullptr;
            sql = mysql_init(sql);
            if(!sql){
                LOG_ERROR("Mysql init error");
                assert(sql);
            }

            // mysql_real_connect:<mysql/mysql.h>中的函数，用于创建sql连接句柄
            sql = mysql_real_connect(sql, host, user, passwd, database, port, nullptr, 0);
            
            if(!sql){
                LOG_ERROR("Mysql connect error");
                assert(sql);
            }

            connQue_.push(sql);
        }
        MAX_CONN_ = connSize;

        // 初始化信号量计数器（信号量semID_最多有MAX_CONN_来执行任务， 
        // 其中第二个参数0表示线程 非0表示进程）
        sem_init(&semID_, 0, MAX_CONN_);
}


MYSQL * SqlConnPool::GetConn(){
    MYSQL * sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("sqlConnPool busy!");
        return nullptr;
    }

    // 只有当信号量的计数器>0 才工作
    sem_wait(&semID_);
    
    {
        std::unique_lock<std::mutex> my_unique_lock(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }

    return sql;
}


void SqlConnPool::RecyConn(MYSQL * sql){
    assert(sql);

    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    connQue_.push(sql);

    // 增加信号计数器的数字
    sem_post(&semID_);
}


void SqlConnPool::ClosePool(){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    while(!connQue_.empty()){
        MYSQL * sql = connQue_.front();
        connQue_.pop();
        mysql_close(sql);
    }

    // 自动释放程序中 所有mysql的资源（连接、数据结构）
    mysql_library_end();
}


int SqlConnPool::GetFreeConnCount(){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool(){
    ClosePool();
}