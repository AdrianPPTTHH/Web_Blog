#include "epoller.h"

// 构造函数
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(1024)),events_(maxEvent){
    // 判断是否初始化epollfd 和 struct epoll_event成功
    assert(epollFd_ >= 0 && events_.size() > 0);
}

// 析构函数
Epoller::~Epoller(){
    close(epollFd_);
}


bool Epoller::AddFd(int fd, uint32_t events){
    if(fd < 0){
        return false;
    }

    // 初始化一个epoll_event结构体
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    
    return epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoller::ModFd(int fd, uint32_t events){
    if(fd < 0){
        return false;
    }

    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoller::DelFd(int fd){
    if(fd < 0){
        return false;
    }
    
    return epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, NULL) == 0;
}

int Epoller::Wait(int timeoutMs){
    // &events_[0] 和 events_.data() 都是返回储存vector数据的地方首地址
    // epoll_wait()第二个参数 是struct * epoll_event类型（要求传一个epoll_event events[])

    // return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
    return epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const{
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const{
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}