#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../sqlconnpool/sqlconnpool.h"
#include "../sqlconnpool/sqlconnRAII.h"
#include "../cookie/cookie.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <string.h>

class HttpRequest{
public:
    //请求行、请求头、请求体、完成解析
    enum PARSE_STATE{
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH
    };

    enum HTTP_CODE{
        NO_REQUESTS = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    void Init();

    HttpRequest(){Init();}
    ~HttpRequest(){};
    
    // 调用这里解析buff中的内容
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    Cookie cookie() const;

    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseCookie_();
    void ParseBody_(const std::string& line);
    
    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& username, const std::string& pwd, bool isLogin);

    // 请求状态
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;

    //post参数
    std::unordered_map<std::string, std::string> post_;

    //cookie
    Cookie cookie_;

    // 默认页面
    static const std::unordered_set<std::string> DEFAULT_HTML;
    // 默认页面编号
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

    
    
    static int ConverHex(char ch);
    
};


#endif