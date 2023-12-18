
### poll 服务器

```C++
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
// #include <poll.h>
#include <sys/poll.h>

using namespace std;

int main(){

    int lfd = socket(AF_INET,SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR , (void *) &opt, sizeof(opt));
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT , (void *) &opt, sizeof(opt));

        // 定义服务器地址
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8888);
        inet_pton(AF_INET, "127.0.0.1" , &server_addr.sin_addr.s_addr);

        socklen_t server_addr_len = sizeof(server_addr);

    // 绑定地址
    int res_bind = bind(lfd, (const struct sockaddr*)&server_addr, server_addr_len);
    if(res_bind == -1){ cout << "bind error!" << endl; return 0;}

    // 设置监听上限
    int res_listen = listen(lfd, 64);
    if(res_listen == -1){ cout << "set listen error!" << endl; return 0;};

        // 设置客户端地址
        struct sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(8888);
        // client_addr.sin_addr.s_addr;
        socklen_t client_addr_len = sizeof(client_addr);

    
    // 初始化 fds
    struct pollfd fds[2048];
    for(int i = 0 ; i < sizeof(fds) / sizeof(pollfd); i++){
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }
    
    fds[0].fd = lfd;
    
    int nfds = 0;

    while(1){
        // nfds+1 从0开始访问到nfds 包括nfds
        int nready = poll(fds, nfds + 1, -1);
        
        if(nready == 0){
            cout << "nothing!" <<endl;
        }else if(nready == -1){
            perror("poll error!");
        }else if(nready > 0 ){
            
            // 监听到连接请求
            if(fds[0].revents & POLLIN){
                int cfd = accept(fds[0].fd,(struct sockaddr*)&client_addr, &client_addr_len);

                char ip[512]; 
                inet_ntop (AF_INET,&client_addr.sin_addr.s_addr, ip, client_addr_len);
                cout << "accepted! IP: " << ip << " port: " << ntohs(client_addr.sin_port) << endl;

                for(int i = 0 ; i < sizeof(fds)/sizeof(struct pollfd); i++){
                    if(fds[i].fd == -1){
                        fds[i].fd = cfd;
                        fds[i].events = POLLIN;
                        nfds++;
                        break;
                    }
                }

                // 更新最大文件描述符索引
                // nfds = nfds > cfd ? nfds:cfd;

                if(--nready){
                    continue;
                }
            }

            for( int i = 1 ; i <= nfds ; i++){
                if(fds[i].fd < 0 ){
                    continue;
                }
    
                if(fds[i].fd > 0 && fds[i].revents & POLLIN){
                    char buf[512];
                    int read_size = read(fds[i].fd, buf, sizeof(buf));
                    if(read_size == -1){
                        cout << "读取错误！" <<endl;
                        break;
                    }

                    if(read_size == 0 ){
                        cout << "客户端断开连接" << endl;
                        close(fds[i].fd);
                        fds[i].fd = -1;
                    }

                    buf[read_size] = '\0';
                    
                    for(int i = 0 ; i < read_size; i++){
                        buf[i] = toupper(buf[i]);
                    }
                    
                    write(fds[i].fd, buf, read_size);     

                }
            }
        }
    }
    
    

}
```



### select 服务器

```C++
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;


int main(){
    
    // 创建socket 文件描述符
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    // 设置端口复用  因为先关闭的那一端需要等待2MSL（4次挥手，最后发送的ACK）  如果服务器先关闭 端口被占用
        // 设置SO_REUSEADDR 为 1：表示服务器断开连接时不等待TIME_WAIT
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));
        // 设置SO_REUSEPORT 为 1：表示设置同一个端口允许多个套接字绑定 
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, (void *)&opt, sizeof(opt));


        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8888);
        inet_pton(AF_INET, "127.0.0.1" , &server_addr.sin_addr.s_addr);

        socklen_t server_addr_len = sizeof(server_addr);

    // 绑定地址
    int res_bind = bind(lfd, (const struct sockaddr*)&server_addr, server_addr_len);
    if(res_bind == -1){ cout << "bind error!" << endl; return 0;}

    // 设置监听上限
    int res_listen = listen(lfd, 64);
    if(res_listen == -1){ cout << "set listen error!" << endl; return 0;};

        // 设置客户端地址
        struct sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(8888);
        // client_addr.sin_addr.s_addr;
        socklen_t client_addr_len = sizeof(client_addr);

    // 设置事件集合
    fd_set rset,allset;

    // 设置最大事件数量
    int max_fd = lfd;

    FD_ZERO(&rset);
    FD_ZERO(&allset);

    // 将lfd添加到监听集合中
    FD_SET(lfd, &allset);

    while(1){
        rset = allset;
        int nready = select(max_fd + 1 , &rset, NULL, NULL, NULL);
        cout << "监听到read事件...." << endl;

        if(nready < 0){
            cout << "select error!" <<endl;
            return 0 ;
        }

        if(nready > 0){
            if(FD_ISSET(lfd, &rset)){
                int cfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len );

                char ip[512]; 
                inet_ntop (AF_INET,&client_addr.sin_addr.s_addr, ip, client_addr_len);
                cout << "accepted! IP: " << ip << " port: " << ntohs(client_addr.sin_port) << endl;

                FD_SET(cfd, &allset);
                
                if(max_fd < cfd){
                    max_fd = cfd;
                }

                if(nready == 1){
                    continue;
                }
            }
            
            // 这里可以定义一个数组 将accept返回的cfd 存到一个数组里面 遍历这个数组
            for(int fd_temp = lfd + 1; fd_temp < max_fd + 1; fd_temp++){
                if(FD_ISSET(fd_temp, &rset)){
                    char buf[512];
                    int read_size = read(fd_temp, buf, sizeof(buf));
                    if(read_size == -1){
                        cout << "读取错误！" <<endl;
                        break;
                    }

                    if(read_size == 0 ){
                        cout << "客户端断开连接" << endl;
                        close(fd_temp);
                        FD_CLR(fd_temp, &allset);
                    }

                    buf[read_size] = '\0';
                    
                    for(int i = 0 ; i < read_size; i++){
                        buf[i] = toupper(buf[i]);
                    }
                    
                    write(fd_temp, buf, read_size);

                }
            }
        }
    }

    return 0;
}

```