#ifndef BUFFER_H
#define BUFFER_H

#include <iostream>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <assert.h>
#include <cstring>
#include <sys/uio.h>  // iovec

class Buffer{
public:

    // 构造函数 初始化buffer大小
    Buffer(int initBufferSize = 4096);
    ~Buffer() = default;
    
    // [已读数据][未读数据][未使用空间]
    // 返回可写入 可读取的缓冲区大小
    size_t WriteableBytes() const;
    size_t ReadableBytes() const;
    // 返回已读数据的大小
    size_t PrependableBytes() const;
    
    // 返回缓冲区当前位置的指针，以便查看缓冲区中的数据。
    const char* Peek() const;

    // 确保缓冲区有足够空间写入指定长度数据
    void EnsureWriteable(size_t len);

    // 更新缓冲区写入的位置，表示已经写入了指定长度的数据
    void HasWritten(size_t len);

    // 移除指定长度的数据，表示已经读取 (将readPos_向后移)
    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();
    std::string RetrieveAlltoStr();
    
    // 返回指向缓冲区当前可写入位置的指针
    const char* BeginWriteConst() const;
    char* BeginWrite();
    
    // 向缓冲区中追加数据 
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    // 向文件描述符中读写数据
    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);
    
private:
    char* BeginPtr_();
    const char* BeginPtr_() const;

    // 缓冲区大小不足时 扩充大小
    void MakeSpace_(size_t len);

    // 用来储存数据
    std::vector<char> buffer_;

    // 读写位置的原子变量
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif