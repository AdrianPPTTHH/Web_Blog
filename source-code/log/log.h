#ifndef LOG_H
#define LOG_H

#include "../buffer/buffer.h"
#include "./blockqueue.h"

#include <memory>
#include <thread>
#include <mutex>
#include <stdarg.h> //va_start va_end
#include <sys/stat.h> //mkdir
#include <assert.h>
#include <sys/time.h>

class Log{
public:
    // 初始化日志对象，等级、路劲、后缀、最大队列容量
    void Init(int level, 
        const char* path = "./log", 
        const char* suffix = ".log", 
        int maxQueueCapacity = 1024);

    // 单例模式，获取Log对象函数
    static Log* Instance();

    static void FlushLogThread();
    
    void write(int level, const char *format, ...);

    void flush();

    int GetLevel();
    
    void SetLevel(int level);

    bool IsOpen(){
        return IsOpen_;
    }

private:

    Log();
    void AppendLogLevelTitle_(int level);
    ~Log();
    void AsyncWrite_();

    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;
    
    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool IsOpen_;

    Buffer buff_;
    int level_;
    // 是否以异步方式写入
    bool isAsync_;

    FILE* fp_;
    
    // 用智能指针管理 string类型的阻塞队列
    std::unique_ptr<BlockDeque<std::string>> deque_;

    // 用智能指针管理 write线程
    std::unique_ptr<std::thread> writeThread_;

    std::mutex mtx_;

};


// 定义宏用于记录日志（##__VA_ARGS__将可变参数...展开，并且用" "拼接在一起）

// log->Getlevel() <= level 检查log对象是否有资格记录当前级别的日志 

#define LOG_BASE(level, format, ...) \
do{\
    Log* log = Log::Instance();\
    if(log->IsOpen() && log->GetLevel() >= level){\
        log->write(level, format, ##__VA_ARGS__);\
        log->flush();\
    }\
}while(0);

// 基于LOG_BASE定义， 用于不同级别日志记录
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif