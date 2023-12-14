#include "./log.h"

Log::Log(){
    lineCount_= 0;
    toDay_ = 0;
    isAsync_ = false;
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
}


Log::~Log(){
    // 处理几个指针

    // 先处理writeThread_和deque_ 因为不用加锁（deque_本来就是flush操作，而且内部有锁
    // writeThread_进行join()操作也是线程安全，所以不用加锁）
    if(writeThread_ && writeThread_->joinable()){
        while(!deque_->empty()){
            deque_->flush();
        };
        
        deque_->close();
        writeThread_->join();
    }

    // 再关闭文件描述符
    if(fp_){
        // fp_文件指针可能会涉及到线程并发，所以上锁
        std::unique_lock<std::mutex> my_unique_lock(mtx_);

        // 确保缓冲区中的所有数据都写入fp_
        flush();
        fclose(fp_);
    }

}

Log * Log::Instance(){
    static Log log;
    return &log;
}

int Log::GetLevel(){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    return level_;
}


void Log::SetLevel(int level){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    this->level_ = level;
}


void Log::Init(int level = 1,
        const char* path,
        const char* suffix,
        int maxQueueCapacity){

    IsOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;

    // maxQueueCapacity > 0 说明启动deque_阻塞队列和thread写入线程
    if(maxQueueCapacity > 0){
        isAsync_ = true;

        // 初始化不在构造函数中，更灵活，因为可以随时进行log.init()操作。
        if(!deque_){
            // 不合法，因为智能指针unique_ptr<>独占所有权，不能用=赋值
            // deque_ = new BlockDeque<std::string>;
            
            // 使用move语法 转移所有权
            std::unique_ptr<BlockDeque<std::string>> deque_t(new BlockDeque<std::string>);
            deque_ = std::move(deque_t);

            // 同理
            std::unique_ptr<std::thread> writeThread_t(new std::thread(FlushLogThread));
            writeThread_ = std::move(writeThread_t);
        }
    
    }else{
        isAsync_ = false;
    }
    
    lineCount_ = 0;
    
    // time(nullptr)获取当前时间戳 
    time_t timer = time(nullptr);
    // 使用localtime将时间戳转换成本地时间结构体（包含年月日时分秒）
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    
    char fileName[LOG_NAME_LEN] = {0};

    // tm_year有1900的偏移量要+上， tm_mon是从0开始计数的所以+1
    // sprintf没有指定缓冲区大小 容易导致溢出
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s" ,
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;
    
    {
        std::unique_lock<std::mutex> my_unique_lock(mtx_);
        buff_.RetrieveAll();
        if(fp_){
            flush();
            fclose(fp_);
        }

        fp_ = fopen(fileName, "a");

        if(fp_ == nullptr){
            int res = mkdir(fileName, 777);
            if(res != 0){ std::cout << "working directory error:" << getcwd(nullptr,256) << std::endl;}
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
    

}


void Log::write(int level, const char* format, ...){
    // 秒，微秒
    struct timeval now = {0 , 0};
    // 获取今天的秒数偏移
    gettimeofday(&now, nullptr);
    // 获取秒数时间戳
    time_t tSec = now.tv_sec;
    // 计算出时间戳的时间
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;


    // 如果日期更新了，或者行数超了，那么重新开一个fp_
    if(toDay_ != t.tm_mday || (lineCount_ && (lineCount_ > MAX_LINES)))
    {
        // 不自动上锁 
        std::unique_lock<std::mutex> my_unique_lock(mtx_, std::defer_lock);

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36 , "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 如果日期更新了
        if(toDay_ != t.tm_mday){
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }else{ // 如果行数超了
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", \
                path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }

        my_unique_lock.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile , "a");
        // my_unique_lock.unlock();

        assert(fp_ != nullptr);
    }

    {
        std::unique_lock<std::mutex> my_unique_lock(mtx_);

        lineCount_++;

        int n = snprintf(buff_.BeginWrite(), 128, "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        
        // 将writePos后移
        buff_.HasWritten(n);

        AppendLogLevelTitle_(level);
        
        // 获取...的多参数列表
        va_list vaList;

        // 用于初始化va_list， format是参数列表中 最后一个已知参数
        va_start(vaList, format);
        
        // 缓冲区可写首地址，缓冲区大小，格式化字符串形式，...参数列表
        int m = vsnprintf(buff_.BeginWrite(), buff_.WriteableBytes(), format, vaList);
        
        // 结束使用va
        va_end(vaList);

        buff_.HasWritten(m);
        
        // 为了换行和字符串的规范以\0结尾
        buff_.Append("\n\0", 2);
        
        // 如果开启了异步任务  那么将buff_中数据全部转移到deque_
        // 否则直接写入fp_
        if(isAsync_ && deque_ && !deque_->full()){
            deque_->push_back(buff_.RetrieveAlltoStr());
        }else{
            fputs(buff_.Peek(), fp_);
        }
        
        // 清空buff
        buff_.RetrieveAll();
    }
}


void Log::flush(){
    if(isAsync_){
        deque_->flush();
    }
    fflush(fp_);
}


void Log::AsyncWrite_(){
    std::string str = "";
    
    // 如果任务多的话，会导致写入非常快速 占用CPU资源
    while(deque_->pop(str)){
        std::unique_lock<std::mutex> my_unique_lock(mtx_);
        fputs(str.c_str(), fp_);
        
        //fputs虽然写入成功了，但是需要fflush(fp_)才会将文件的缓冲数据 刷新进磁盘
        flush();
    }
}

// 执行异步任务 将deque_写入fp_中
void Log::FlushLogThread(){
    Log::Instance()->AsyncWrite_();
}

void Log::AppendLogLevelTitle_(int level){
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}