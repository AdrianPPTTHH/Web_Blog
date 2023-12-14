#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "./httprequest.h"
#include "./httpresponse.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/uio.h>  //readv、writev、iov
#include <atomic>


class HttpConn{
public:
    HttpConn();
    ~HttpConn();
    
    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int * saveErrno);
    
    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;

    sockaddr_in GetAddr() const;

    bool process();
    
    int ToWriteBytes(){
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    bool IsKeepAlive() const{
        return request_.IsKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

    
private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;
    struct iovec iov_[2];

    Buffer readBuff_;
    Buffer writeBuff_;
    

    HttpRequest request_;
    HttpResponse response_;
};

#endif