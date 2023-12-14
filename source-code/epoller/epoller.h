#ifndef EPOLLER_H
#define EPOLLER_H

#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <assert.h>
#include <unistd.h>

class Epoller{
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();
    
    bool AddFd(int fd, uint32_t events);
    
    bool ModFd(int fd, uint32_t events);
    
    bool DelFd(int fd);
    
    int Wait(int timeoutMs = -1);
    
    int GetEventFd(size_t i ) const;

    uint32_t GetEvents(size_t i) const;

private:
    int epollFd_;

    std::vector<struct epoll_event> events_;
};


#endif