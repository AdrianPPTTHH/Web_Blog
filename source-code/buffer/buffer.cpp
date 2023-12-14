#include "buffer.h"

// 初始化构造函数
Buffer::Buffer(int initBufferSize):buffer_(initBufferSize),readPos_(0),writePos_(0){}

// [已读数据]readPos_[未读数据]writePos_[未使用空间]
size_t Buffer::WriteableBytes() const{
    return buffer_.size() - writePos_;
}

size_t Buffer::ReadableBytes() const{
    return writePos_ - readPos_;
}

size_t Buffer::PrependableBytes() const{
    return readPos_;
}

// 注意是位置的"字符的指针"
const char* Buffer::Peek() const{
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len){
    // 断言 移除的长度 小于 当前可读取的长度
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end){
    // 判断当前缓冲区的数据 是否在目标字符*end的指针之前
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    // 清空buffer_
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAlltoStr() {
    // Peek()返回当前读到的字符的指针， 然后ReadableBytes返回往后读几位
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 注意是开始写的位置的字符指针
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

void Buffer::Append(const std::string& str){
    // str.data()返回的指向string的指针
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len){
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWriteable(len);
    // 将str（开始指针）  str+len（结束指针）的字符串 写入到beginWrite()指针的位置
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buf){
    // 将另一个缓冲区的数据写入 （另一个缓冲区的未读数据）
    Append(buf.Peek(), buf.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len){
    // 如果不够则扩容
    if(WriteableBytes() < len){
        MakeSpace_(len);
    }
    assert(WriteableBytes() >= len);
}

char* Buffer::BeginPtr_(){
    // buffer_的迭代器第一个位置的地址
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const{
    return &*buffer_.begin();
}

void Buffer::MakeSpace_(size_t len){
    if(WriteableBytes() + PrependableBytes() < len){
        buffer_.resize(writePos_ + len + 1);
    }else{
        // 将缓冲区往前移动，把前面已经读取的覆盖掉
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno){
    char buf[65535];

    // 使用I\O vector结构体分散读取 （防止buf中的buffer_大小不够）
    struct iovec iov[2];
    const size_t writeable = WriteableBytes();
    
    // 分散读取
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writeable;
    iov[1].iov_base = buf;
    iov[1].iov_len = sizeof(buf);

    // iov[0]不够装的数据 装在iov[1]中 
    const ssize_t len = readv(fd, iov, 2);

    if(len < 0){
        *saveErrno = errno;
    }else if(static_cast<size_t>(len) <= writeable){
        // 正常读取移动指针
        writePos_ += len;
    }else{// 缓冲区长度不够
        // 将指针移动到buffer_末尾
        writePos_ = buffer_.size();
        // 向缓冲区添加数据， 之前的Append函数会自动MakeSpace（扩容）
        Append(buf, len - writeable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    // 将缓冲区中 未读取那部分数据 写入fd
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}


