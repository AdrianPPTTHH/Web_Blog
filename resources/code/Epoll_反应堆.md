```C++
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>

using namespace std;

#define BUFFLEN 256
#define MAX_EVENTS 1024

void acceptconn(int lfd, int events, void *arg);
void initlistenfd(int &efd, int &lfd);
void acceptconn(int lfd, int events, void *arg);
void recvdata(int fd, int event, void *arg);
void senddata(int fd, int event, void * arg);
void setevent(struct my_events* ev, int fd, void(*call_back)(int, int, void* arg), void * arg);
void addevent(int efd, int event, struct my_events *ev );
void delevent(int efd, struct my_events *ev);

// epoll_event.data联合体中的 void * ptr
struct my_events {
    int fd;
    int events;
    void *args;
    
    // 反应堆重要的回调函数   events是EPOLLIN、EPOLLOUT arg就是struct my_events
    void (*call_back)(int fd, int events, void *arg);
    
    //  是否在epfd上
    int status;

    // 每个fd的BUF
    char buf[BUFFLEN];

    // 缓冲区大小
    int len;

    // 最后一次活跃的时间
    long last_active;
    
};


int efd = epoll_create(MAX_EVENTS);
struct my_events m_events[MAX_EVENTS + 1];

void initlistenfd(int &efd, int &lfd){

    // 服务器地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "10.16.51.103", &server_addr.sin_addr.s_addr);
    socklen_t server_addr_len = sizeof(server_addr);
    
    int res1 = bind(lfd, (const sockaddr*)& server_addr, server_addr_len );
    if(res1 < 0){
        perror("bind error");
    }

    int res2 = listen(lfd, 1024);
    if(res2 < 0){
        perror("listen error");
    } 

    // void setevent(struct my_events* ev, int fd, void(*call_back)(int, int, void* arg), void * arg)
    setevent(&m_events[MAX_EVENTS], lfd, acceptconn, &m_events[MAX_EVENTS]);

    // void addevent(int efd, int event, struct my_events *ev )
    addevent(efd, EPOLLIN, &m_events[MAX_EVENTS]);

    return;

}

void acceptconn(int lfd, int events, void *arg){    
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    socklen_t client_addr_len = sizeof(client_addr);
    
    int cfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    if(cfd == -1){
        perror("accpet error");
    }
    
    char ip[128];
    cout << "Accept! ip: " << inet_ntop(AF_INET, &client_addr.sin_addr.s_addr,ip, client_addr_len) << 
    " port: " << ntohs(client_addr.sin_port) << endl;
    
    int i;
    do{
        for(i = 0 ; i < MAX_EVENTS; i++){
            if(m_events[i].status == 0){
                break;
            }
        }

        if(i == MAX_EVENTS){
            cout << "max connect limit!" << endl;
            break;
        }
        
        // 设置非阻塞
        int flag = fcntl(cfd, F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(cfd, F_SETFL, flag);

        setevent(&m_events[i], cfd, recvdata, &m_events[i]);
        addevent(efd, EPOLLIN | EPOLLET, &m_events[i]);
        cout << endl;
    }while(0);
}

void recvdata(int fd, int event, void *arg){
    struct my_events *ev = (struct my_events *) arg;

    // 读到buf中
    int len = recv(fd, ev->buf, sizeof(ev->buf), 0 );
    
    delevent(efd, ev);

    if(len > 0 ){
        ev->len = len;
        ev->buf[len] = '\0';
        cout << ev->buf << endl;

        setevent(ev, fd , senddata, ev);
        addevent(efd, EPOLLOUT, ev);

    }else if( len == 0 ){
        close(ev->fd);
        cout << "closed by client!" << endl;
    }else{
        close(ev->fd);
        cout << "something wrong" << endl;;
    }

    return;
}


void senddata(int fd, int event, void * arg){
    struct my_events *ev = (struct my_events *) arg;

    // 这里不能将之前的数据写回去 
    // 因为recvdata中调用setevent之后 ev的buf就被清空了 ev->args->buf和ev->buf指向同一个地址  也被清空了
    strcpy(ev->buf, "hahhaha");
    ev->len = sizeof(ev->buf);

    cout << ev->buf << ev->len <<endl;

    int len = send(fd, ev->buf, ev->len, 0);

    delevent(efd, ev);

    if(len > 0){
        setevent(ev, fd, recvdata, ev);
        addevent(efd, EPOLLIN | EPOLLET, ev);
    }else if(len == 0){
        close(ev->fd);
        cout << "nothing to send" << endl;
    }else{
        close(ev->fd);
        cout << "something wrong" << endl;
    }

    return;
}


void setevent(struct my_events* ev, int fd, void(*call_back)(int, int, void* arg), void * arg){

    ev->fd = fd;
    ev->call_back = call_back;
    ev->events = 0;
    ev->args = arg;
    memset(ev->buf, 0, sizeof(ev->buf));
    ev->len = 0;
    ev->last_active = time(NULL);

    return;
}



void addevent(int efd, int event, struct my_events *ev){

    if(ev->status == 0){
        cout << "add fd：" << ev->fd << endl;

        int op = EPOLL_CTL_ADD;
        epoll_event temp;

        temp.data.ptr = ev;
        temp.events = event;
        ev->events = event;
        ev->status = 1;

        if(epoll_ctl(efd, op, ev->fd, &temp) < 0){
            perror("add error");
        }else{
            cout << "add success！" << endl;
        }
    }

    return;
    
}


void delevent(int efd, struct my_events *ev){

    if(ev->status == 1){
        cout << "delete fd: " << ev->fd << endl;

        int op = EPOLL_CTL_DEL;
        epoll_event temp;

        temp.data.ptr = NULL;
        ev->status = 0;

        if(epoll_ctl(efd, op, ev->fd, &temp) < 0){
            perror("del error");
        }else{
            cout << "del success！" << endl;
        }
    }

    return;
    
}


int main(int argc, char *argv[]){
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    // 端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    initlistenfd(efd, lfd);

    struct epoll_event events[MAX_EVENTS];

    int i, checkpos=0;
    while(1){
        // 超时验证
        long now = time(NULL);
        for(i = 0 ; i < 100; checkpos++){
            if(checkpos == MAX_EVENTS){
                checkpos = 0;
                break;
            }

            if(m_events[checkpos].status == 1){
                long duration = now - m_events[checkpos].last_active;
                if(duration >= 60){
                    close(m_events[checkpos].fd);
                    cout << "fd:" << m_events[checkpos].fd <<" timeout." << endl;
                    delevent(efd, &m_events[checkpos]);
                }   
            }
        }

        // 监听
        int nready = epoll_wait(efd, events, MAX_EVENTS, -1);

        if(nready < 0){
            cout << " epoll_wait error!" << endl;
            break;
        }

        for(int k = 0; k < nready ; k++){
            struct my_events *ev = (struct my_events *)events[k].data.ptr;
            
            if((events[k].events & EPOLLIN) && (ev->events & EPOLLIN)){
                ev->call_back(ev->fd, events[k].events, ev->args);
            }

            if((events[k].events & EPOLLOUT) && (ev->events & EPOLLOUT)){
                ev->call_back(ev->fd, events[k].events, ev->args);
            }
        }
    }
    


    return 0;
}


```