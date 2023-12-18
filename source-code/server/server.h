#ifndef SERVER_H
#define SERVER_H

#include "../http/httpconn.h"
#include "../threadpool/threadpool.h"
#include "../epoller/epoller.h"
#include "../sqlconnpool/sqlconnpool.h"
#include "../sqlconnpool/sqlconnRAII.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../redisconnpool/redisconnpool.h"
#include "../redisconnpool/redisconnRAII.h"

#include <arpa/inet.h>
#include <unordered_map>
#include <unistd.h>
#include <functional>
#include <fcntl.h>
#include <memory>


class WebServer{
public:
WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        const char * sqlHost, int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, const char* redisIP, int redisPort,
        int threadNum,bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);
    void Mysql_StateHold(std::chrono::seconds sec);
    

    static const int MAX_FD = 65536;
    
    static int SetFdNonblock(int fd);
    
    int port_;
    bool openLinger_; //关闭连接时，是否等待未发送完的数据
    int timeoutMS_;
    bool isClose_;
    int listenFd_;
    char * srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
    
    std::unique_ptr<HeapTimer> timer_;
    // std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Threadpool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;

};


#endif