#include "./httpconn.h"

bool HttpConn::isET;
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;

HttpConn::HttpConn(){
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn(){
    Close();
}

void HttpConn::init(int sockFd, const sockaddr_in &addr){
    assert(sockFd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockFd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}


void HttpConn::Close(){
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}


int HttpConn::GetFd() const{
    return fd_;
}


int HttpConn::GetPort() const{
    return addr_.sin_port;
}


const char* HttpConn::GetIP() const{
    char* ip = new char[256];
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, ip, 256);
    return ip;
}


struct sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}

ssize_t HttpConn::read(int * saveErrno){
    ssize_t len = -1;
    do{
        len = readBuff_.ReadFd(fd_, saveErrno);
        if(len <= 0){break;}
    }while(isET);
    return len;
}


bool HttpConn::process(){
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0){
        return false;
    }
    else if(request_.parse(readBuff_)){
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), request_.cookie(), 200);
    }
    else{
        response_.Init(srcDir,request_.path(), false, Cookie(), 400);
    }

    // 将response的响应头存入writeBuff_
    response_.MakeResponse(writeBuff_);

    /* 响应头内容 */
    iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件内容 */
    if(response_.File() && response_.FileLen() > 0){
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }

    LOG_DEBUG("file size: %d, %d to %d", response_.FileLen(), iovCnt_, ToWriteBytes());

    return true;
}



ssize_t HttpConn::write(int * saveErrno){
    ssize_t len = -1;
    do{
        // 写入fd_中  因为响应头存入writeBuff 而文件内容在mmap中   所以采用iov分页存储
        // len = writeBuff_.WriteFd(fd_, saveErrno);
        len = writev(fd_, iov_, iovCnt_); // 有可能一次没有将数据发送完全 （发送之后数据iov中的数据任然不会清0）

        if(len <= 0){
            *saveErrno = errno;
            break;
        }

        // std::cout << (char*)iov_[1].iov_base << std::endl;

        if(iov_[0].iov_len + iov_[1].iov_len == 0){
            break;
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len){ //说明iov[1]中有数据 传了文件
            // 因为有可能writev没有将iov_的数据读取完，所以需要更新iov
            // 为了能进行指针的加减运算， 所以将void*类型的指针转换成uint8_t
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len = iov_[1].iov_len - (len - iov_[0].iov_len);

            if(iov_[0].iov_len){
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else{ //iov_[0]都没读完
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len = iov_[0].iov_len - len;
            writeBuff_.Retrieve(len);
        }
    }while(isET || ToWriteBytes() > 10240);
    return len;
}

