```C++
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>
#include <fcntl.h>


using namespace std;

const int MAXSIZE = 2000;

struct event_ptr{
    int fd;
    uint32_t event;
    std::filesystem::path path;
    bool is_directory;
    
};

int get_line(int cfd, char* buf, int size){
    int i = 0 ;
    char c = '\0';
    int n;

    // 读到'\n'就结束 如果读到'\r'就将它也变成'\n'结束
    while((i < size - 1) && (c != '\n')){
        n = recv(cfd, &c, 1, 0);
        if(n > 0){
            // 读到\r 判断下一个字符是不是\n
            if(c == '\r'){
                // 试读一下  不从缓存区cfd中拿出
                n = recv(cfd, &c, 1, MSG_PEEK);
                if( n > 0 && (c == '\n')){
                    recv(cfd, &c, 1, 0);
                }else{
                    // 将'\r'变成'\n'
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }else{
            c = '\n';
        }
        // 每次在后面一位加上一个'\0'
        buf[i] = '\0';
        
        // 如果recv出错返回-1 
        if(n == -1){
            i = n;
        }
    }

    return i;
}


// 返回需要监听的套接字
int get_lfd(int port){
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    // 设置不等待2msl
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 设置端口可绑定多个IP
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t server_addr_len = sizeof(server_addr);

    int res = bind(lfd, reinterpret_cast<const struct sockaddr*>(&server_addr), server_addr_len);
    if(res < 0){
        perror("bind error");
        exit(0);
    }

    res = listen(lfd, MAXSIZE);
    if(res < 0){
        perror("set listen error");
        exit(0);
    }
    return lfd;
}

// 将套接字fd上树
void putin_read_epoll(int fd, int epfd){
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLIN | EPOLLET;
    
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

// 将套接字fd上树
void putin_write_epoll(int fd, int epfd){
    struct epoll_event temp;
    temp.data.fd = fd;
    temp.events = EPOLLOUT;
    
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &temp);
}

void do_accept(int epfd, int lfd){
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    // client_addr.sin_port = htons(0);
    // client_addr.sin_addr.s_addr = htonl(0);
    // memset(&client_addr, 0, sizeof(client_addr));
    char ip[512];

    socklen_t client_addr_len = sizeof(client_addr); 

    int cfd = accept(lfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);

    if(cfd < 0){
        perror("accept error");
        // exit(0);
        return;
    }
    
    cout << "accpeted! ip: " << inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof(client_addr))
    << "  port: " << ntohs(client_addr.sin_port) << endl;
    
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    
    putin_read_epoll(cfd, epfd);
}

void send_404(int epfd, int cfd){
    // 打开404文件
    ifstream file("./404.html");

    // response头
    string response("HTTP/1.1 404 Not Found\r\nContent-type: text/html;charset=utf-8\r\nConnection:close\r\n\r\n");
    
    // 文件body
    stringstream body;
    
    if(file.is_open()){
        body << file.rdbuf();
        response = response + body.str();
        
        send(cfd, response.c_str(), response.length(), 0);
        
    }else{
        perror("open 404.html error");
        exit(0);
    }
    
    file.close();
    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    close(cfd);

}

// 文件描述符 状态码 状态码描述 回发文件类型 文件长度
void send_http_head(int epfd, int cfd, int status, const char *disc, const char *type, int len, std::filesystem::path visit_path){
    ostringstream response;
    if(len > 0){
        response << "HTTP/1.1 " << status << " " << disc << "\r\n" 
        << type << "\r\n" 
        << "Content-Length: " << len << "\r\n" 
        << "Connection:close" << "\r\n\r\n"; 
    }else{
        response << "HTTP/1.1 " << status << " " << disc << "\r\n" 
        << type << "\r\n" 
        << "Connection:close" << "\r\n\r\n"; 
    }


    // ifstream f(visit_path.c_str());
    // response << f.rdbuf();
    
    int res = send(cfd, response.str().c_str(), response.str().length(), 0);

    if (res == -1) {
        if(errno == EAGAIN){
            cout << "EAGAIN" << endl;
        }else if(errno == EINTR){
            cout << "EINTR" << endl;
        }else{
            perror("send error1");
            close(cfd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            exit(0);
        }

        // 如果异常EAGAIN || EINTR，设置epoll监听EPOLLOUT时间，（等到可以发送了，再发送）
        epoll_ctl(epfd, cfd, EPOLL_CTL_DEL, NULL);

        struct epoll_event ev;

        struct event_ptr* ptr = new event_ptr;
        ptr->fd = cfd;
        ptr->event = EPOLLOUT;
        ptr->path = visit_path;
        ptr->is_directory = std::filesystem::is_directory(visit_path);
        ev.data.ptr = ptr;
        ev.events = EPOLLOUT;
        epoll_ctl(epfd, cfd, EPOLL_CTL_ADD, &ev);
        return;
    }
    // f.close();
}

void send_http_body(int epfd, int cfd, std::filesystem::path visit_path){
    ifstream f(visit_path.c_str(), ios::binary);
    
    ostringstream response;
    response << f.rdbuf();
    
    // MSG_NOSIGNAL设置管道不发出进程级别信号（SIGPIPE） 如果不设置的话：服务器还在发数据，客户端关闭连接，管道会破裂。
    int res = send(cfd, response.str().c_str(), response.str().length(), MSG_NOSIGNAL);
    if (res == -1) {
        if(errno == EAGAIN){
            cout << "EAGAIN" << endl;
        }else if(errno == EINTR){
            cout << "EINTR" << endl;
        }else{
            perror("send error2");
            f.close();
            close(cfd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            // exit(0);
            return;
        }

        // 如果异常EAGAIN || EINTR，设置epoll监听EPOLLOUT时间，（等到可以发送了，再发送）
        epoll_ctl(epfd, cfd, EPOLL_CTL_DEL, NULL);

        struct epoll_event ev;

        struct event_ptr* ptr = new event_ptr;
        ptr->fd = cfd;
        ptr->event = EPOLLOUT;
        ptr->path = visit_path;
        ptr->is_directory = std::filesystem::is_directory(visit_path);
        ev.data.ptr = ptr;
        ev.events = EPOLLOUT;
        epoll_ctl(epfd, cfd, EPOLL_CTL_ADD, &ev);
        return;
    }

    f.close();
    close(cfd);
    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
}

void send_directory(int epfd, int cfd, std::filesystem::path visit_path){
    string response;
    response += "<h1> DIRECTORY </h1></br>";
    
    for(const auto file: filesystem::directory_iterator(visit_path)){

        if(file.path().filename().string() == "404.html" || 
            file.path().filename().string() == "myhttp.cpp" ||
            file.path().filename().string() == "myhttp"
            ){
            continue;
        }
        response += "<li> <a href=\" " + file.path().string() + " \"> " + file.path().filename().string() +"</a></li>";
        response += "</br>";
    }
    int res = send(cfd, response.c_str(), response.size(), MSG_NOSIGNAL);

    if (res == -1) {
        if(errno == EAGAIN){
            cout << "EAGAIN" << endl;
        }else if(errno == EINTR){
            cout << "EINTR" << endl;
        }else{
            perror("send error2");
            close(cfd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            // exit(0);
            return;
        }

        // 如果异常EAGAIN || EINTR，设置epoll监听EPOLLOUT时间，（等到可以发送了，再发送）
        epoll_ctl(epfd, cfd, EPOLL_CTL_DEL, NULL);

        struct epoll_event ev;

        struct event_ptr* ptr = new event_ptr;
        ptr->fd = cfd;
        ptr->event = EPOLLOUT;
        ptr->path = visit_path;
        ptr->is_directory = std::filesystem::is_directory(visit_path);
        ev.data.ptr = ptr;
        ev.events = EPOLLOUT;
        epoll_ctl(epfd, cfd, EPOLL_CTL_ADD, &ev);
        return;
    }

    close(cfd);
    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
}

void http_request(int epfd, int cfd, std::filesystem::path visit_path){
    
    try{

        uintmax_t file_len = std::filesystem::file_size(visit_path);
        send_http_head(epfd, cfd, 200, "OK", "Content-type: text/html; charset=utf-8", file_len, visit_path);
        // send_http_head(epfd, cfd, 200, "OK", "Content-type: audio/mpeg; charset=utf-8", file_len, visit_path);

        send_http_body(epfd, cfd, visit_path);

    }catch(filesystem::filesystem_error &e){
        cerr << e.what() << endl;
    }
    
}

void http_request_directory(int epfd, int cfd, std::filesystem::path visit_path){
    
    try{
        send_http_head(epfd, cfd, 200, "OK", "Content-type: text/html; charset=utf-8", -1, visit_path);
        send_directory(epfd, cfd, visit_path);
    }catch(filesystem::filesystem_error &e){
        cerr << e.what() << endl;
    }
    
}

void do_read(int epfd, int cfd){
    char *first_line = new char[50];

    memset(first_line, 0 , 50);
    int n = get_line(cfd, first_line, 50);

    if(n <= 0){
        cout << "closed by client!" << endl;
        close(cfd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        return;
    }

    string line(first_line);
    string temp("");
    vector<string> tokens;
    cout << first_line << endl;
    

    // 解析第一行
    for(int i = 0 ; i < line.size(); i++){
        if(line[i] == ' ' || line[i] == '\n'){
            tokens.push_back(temp);
            temp.clear();
        }else{
            temp.push_back(line[i]);
        }
    }

    // 读取缓冲区其它数据  清除cfd管道里的数据
    while(n > 0){
        n = get_line(cfd, first_line, 50);
        cout << first_line << flush;
    }
    

    if(tokens.size() != 3){
        perror("read error");
        return;
    }

    delete[] first_line;

    // 解析出访问的文件名 tokens[1]
    std::filesystem::path visit_path{'.' + tokens[1]};

    if( !std::filesystem::exists(visit_path) ){
        // 文件不存在 写回404页面
        send_404(epfd, cfd);
    }else if(!filesystem::is_directory(visit_path)){
        http_request(epfd, cfd, visit_path);
    }else{
        http_request_directory(epfd, cfd, visit_path);
    }
}

// epoll开始监听
void start_epoll(int port){
    // 创建红黑树
    int epfd = epoll_create(MAXSIZE);
    if(epfd == -1){
        perror("create epfd error");
        exit(0);
    }

    // 创建绑定好一切的lfd
    int lfd = get_lfd(port);
    putin_read_epoll(lfd, epfd);

    // 定义监听到事件的 epoll_events
    struct epoll_event temp[MAXSIZE];
    while(1){
        int n = epoll_wait(epfd, temp, MAXSIZE, -1);
        if(n < 0){
            perror("listen error");
        }else{
            for(int i = 0; i < n ; i++){
                if(temp[i].data.fd == lfd){
                    do_accept(epfd, temp[i].data.fd);
                }else{
                    if(temp->events & EPOLLIN){
                        do_read(epfd, temp[i].data.fd);
                    }else if(temp->events & EPOLLOUT){
                        struct event_ptr *ptr = (struct event_ptr*)temp->data.ptr;
                        if(ptr->is_directory == true){
                            // send目录
                            http_request_directory(epfd, ptr->fd, ptr->path);
                        }else{
                            http_request(epfd, ptr->fd, ptr->path);
                        }

                    }
                    
                }
            }
        }
    }
}

int main(int argc, char *argv[]){
    if(argc < 3){
        cout << "you should use: ./myhttp.out port path" << endl;
        exit(0);
    }

    // 端口
    int port = atoi(argv[1]);

    // 工作根目录
    int ret = chdir(argv[2]);
    if(ret != 0){
        perror("chdir error");
        exit(0);
    }
    
    // 启动epoll
    start_epoll(port);
    
    return 0;

}
```

