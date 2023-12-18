#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../cookie/cookie.h"

#include <string>
#include <unordered_map>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cmark.h>
#include <iconv.h>

class HttpResponse{
public:
    HttpResponse();
    ~HttpResponse();
    
    void Init(const std::string& srcDir, std::string& path, 
                         bool isKeepAlive = false, const Cookie& cookie = Cookie(), int code =-1);

    // 将用于回复的response封装在buff中
    void MakeResponse(Buffer & buff);

    // 解映射
    void UnmapFile();

    // 获取文件名
    char* File();

    // 获取文件大小
    size_t FileLen() const;

    void ErrorContent(Buffer& buff, std::string message);
    int Code() const {return code_;}

private:
    void AddStateLine_(Buffer &buff);
    void AddHeader_(Buffer &buff);
    void AddContent_(Buffer &buff);

    void ErrorHtml_();
    std::string GetFileType_();
    std::string convert_to_utf8(const char* source, const char* source_encoding);

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    // 文件映射到内存中，直接操作内存 一般是高效存储
    // filesystem 一般是跨平台 操作简单
    char* mmFile_;
    struct stat mmFileStat_; //储存文件元数据信息
    
    // 文件后缀:Content-Type
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;

    //cookie
    Cookie cookie_;

    size_t file_len;
};

#endif