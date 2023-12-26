#include "./server.h"

WebServer::WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        const char * sqlHost, int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, const char* redisIP, int redisPort,
        int threadNum,bool openLog, int logLevel, int logQueSize):
        //初始化
        port_(port), openLinger_(OptLinger),timeoutMS_(timeoutMS),isClose_(false),
        timer_(new HeapTimer()),
        threadpool_(new Threadpool(threadNum, threadNum, threadNum)),
        epoller_(new Epoller(2048))

        {
    
    // 先获取工作目录
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);

    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    
    // sql连接池初始化
    SqlConnPool::Instance()->Init(sqlHost, sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    RedisConnPool::Instance()->Init(redisIP, redisPort, connPoolNum);

    // 初始化触发模式 水平还是垂直触发
    InitEventMode_(trigMode);

    // 初始化socket套接字
    if(!InitSocket_()){isClose_ = true;}

    // 初始化日志
    if(openLog){
        // logQueSize 如果<=0 则
        // 如果logQueSize>0 则使用queue存储日志，异步输出到文件
        Log::Instance()->Init(logLevel, "./log", ".log", logQueSize);
        // Log::Instance()->Init(logLevel, "./log", ".log", 0);

        if(isClose_){
            LOG_ERROR("Server init error!");
        }else{
            LOG_INFO("Server init");
            LOG_INFO("Port:%d , OpenLinger: %s", port_, OptLinger?"true":"false");
            LOG_INFO("Listen Mode:%s, OpenConn Mode:%s", 
                (listenEvent_ & EPOLLET?"ET":"LT"),
                (connEvent_ & EPOLLET?"ET":"LT"));
            LOG_INFO("LogSys level:%d", logLevel);
            LOG_INFO("srcDir:%s", srcDir_);
            LOG_INFO("SqlConnPool num:%d, ThreadPool num:%d", connPoolNum, threadNum);
        }
    }

}


WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
    threadpool_->threadPoolDestroy();
}


void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP; // 是一种读半关闭状态  表示可以关闭读套接字 一般是客户端发起关闭 或者 服务端发起关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; //某一事件只触发一次  触发之后需要重新使用EPOLL_CTL注册
    
    switch(trigMode){
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}


void WebServer::Start(){
    int timeMS= -1;
    if(!isClose_){LOG_INFO("Server start!");}
    
    // 防止mysql超时, 定时执行sql语句  (一般是28800秒(8小时)， 为了保险起见设置6小时21600秒)
    auto a = std::bind(&WebServer::Mysql_StateHold, this, std::chrono::seconds(21600 / SqlConnPool::Instance()->GetFreeConnCount()));
    threadpool_->threadPoolAdd(a);
    
    // 死循环监听epoll
    while(!isClose_){ 
        if(timeoutMS_ > 0){
            // 清除超时任务
            timer_->tick();
            // 获取下一个事件的超时时间 
            timeMS = timer_->GetNextTick();
        }

        
        // 启动epoll监听，返回监听到的数量（监听到的epoll_event储存到event_中，调用函数即可获取）
        int eventCnt = epoller_->Wait(timeMS);

        for(int i = 0 ; i < eventCnt; i++){
            
            // 获取监听监听到的 fd和event 
            int fd = epoller_->GetEventFd(i);
            uint32_t event = epoller_->GetEvents(i);
            
            if(fd == listenFd_){
                DealListen_();
            }
            else if(event & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(event & EPOLLIN){
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(event & EPOLLOUT){
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else{
                LOG_ERROR("Unexpected event");
            }
        }
    }
}


void WebServer::SendError_(int fd, const char* info){
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0){
        LOG_WARN("send error info to client[%d] error!" , fd);
    }
    close(fd);
}


void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    
    do{
        int fd = accept(listenFd_, reinterpret_cast<struct sockaddr*>(&addr), &len);
        if(fd <= 0 ){
            return;
        }
        else if(HttpConn::userCount >= MAX_FD){
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    }while(listenEvent_ & EPOLLET);
    
}


void WebServer::AddClient_(int fd, sockaddr_in addr){
    assert(fd > 0);
    users_[fd].init(fd, addr);

    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }

    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());    
}


void WebServer::CloseConn_(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}


void WebServer::DealRead_(HttpConn * client){
    assert(client);
    ExtentTime_(client);
    
    // 使用std::bind()绑定OnRead_()因为OnRead需要传入参数
    auto func = std::bind(&WebServer::OnRead_, this , client);
    // auto func = [this, client](){
    //     this->OnRead_(client);
    // };

    // 加入到线程池的任务队列中
        // 这里有个坑，因为在在写threadPoolAdd的时候，.h文件没有明确threadPoolAdd的定义，
        // 导致编译器 在编译阶段 不能将func传入threadPoolAdd(T&& func)中
        // 解决办法：在.h头文件中 完成threadPoolAdd的定义
    threadpool_->threadPoolAdd(std::move(func));
    // threadpool_->AddTask(func);

}


void WebServer::DealWrite_(HttpConn * client){
    assert(client);
    ExtentTime_(client);
    
    auto func = std::bind(&WebServer::OnWrite_, this , client);
    // auto func = [this, client](){
    //     this->OnWrite_(client);
    // };
    threadpool_->threadPoolAdd(std::move(func));
    // threadpool_->AddTask(func);
}


void WebServer::ExtentTime_(HttpConn * client){
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}


void WebServer::OnRead_(HttpConn * client){
    assert(client);
    int ret = -1;
    int readError = 0;
    
    // 调用httpConn的read函数 读取httpConn中fd_的内容到 readbuff
    ret = client->read(&readError);

    if(ret <= 0 && readError != EAGAIN){
        CloseConn_(client);
        return;
    }
    
    // 继续处理client
    OnProcess_(client);
}


void WebServer::OnProcess_(HttpConn * client){
    // 调用HttpConn的process函数
    // ->调用HttpRequest的parse函数，解析readbuff->交给HttpResponse初始化
    // ->调用HttpResponse的MakeResponse函数 生成response头和file内容，分别储存在iov[2]中
    if(client->process()){
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    }else{
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}


void WebServer::OnWrite_(HttpConn * client){
    assert(client);
    int ret = -1;
    int writeError = 0;

    // 调用HttpConn的write 写回iov[2]中的数据
    ret = client->write(&writeError);
    
    if(client->ToWriteBytes() == 0){
        // 传输完成了，如果alive 继续获取管道数据
        if(client->IsKeepAlive()){
            ExtentTime_(client);
            OnProcess_(client);
            return;
        }
    }
    else if(ret < 0){
        if(writeError == EAGAIN){
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
        }
    }
    CloseConn_(client);
}


bool WebServer::InitSocket_(){

    if(port_ > 65535 || port_ < 1024){
        LOG_ERROR("Port:%d error!", port_);
        return false;
    }

    struct sockaddr_in addr;
    socklen_t addr_len;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    // inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_len = sizeof(addr);
    
    // 1.socket
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    
    // 端口复用
    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    // 优雅关闭：直到所有数据发送完毕或超时再关闭
    if(openLinger_){
        struct linger optLinger = {0};
        optLinger.l_linger = 2; //延迟关闭等待时间（s为单位）
        optLinger.l_onoff = 1; //启用、禁用延迟关闭
        setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    }

    //非阻塞
    int flag = fcntl(listenFd_, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(listenFd_, F_SETFL, flag);

    // 2.bind
    int ret = bind(listenFd_, reinterpret_cast<const sockaddr*>(&addr), addr_len);
    if(ret < 0 ){
        LOG_ERROR("bind error!");
        close(listenFd_);
        return false;
    }

    // 3.listen
    // 设置监听上限（listenFd_的等待队列数量）
    ret = listen(listenFd_, 64);
    if(ret < 0){
        LOG_ERROR("listen error!");
        close(listenFd_);
        return false;
    }

    // 4.accept
    // 使用epoller监听listenfd套接字，监听到了读时间之后再accpet
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret == 0){
        LOG_ERROR("add listenfd to epoller error!");
        close(listenFd_);
        return false;
    }
    
    // 设置非阻塞模式
    SetFdNonblock(listenFd_);

    LOG_INFO("server start port:%d", port_);

    return true;
}


int WebServer::SetFdNonblock(int fd){
    assert(fd > 0);
    
    // 获取fd原来的FL值
    uint32_t flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;

    return fcntl(fd, F_SETFL, flag);
}

void WebServer::Mysql_StateHold(std::chrono::seconds sec){
    
    while(!isClose_){
        MYSQL * sql = mysql_init(nullptr);
        
        SqlConnRAII(&sql, SqlConnPool::Instance());
        
        assert(sql);

        //每隔2小时 执行一次WebServer::Time_mysql_callback()
        

        if(mysql_query(sql, "select 1;")){
            LOG_ERROR("mysql error: %s", mysql_error(sql));
        }

        std::this_thread::sleep_for(std::chrono::seconds(sec));
    }
 
}