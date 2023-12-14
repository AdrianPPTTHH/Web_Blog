#include "cookie.h"


Cookie::Cookie(){
    username_.clear();
    value_.clear();
    
}

Cookie::~Cookie(){
    value_.clear();
}


void Cookie::SetCookie(){
    
    // 字符串
    std::string charSet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    //随机种子
    std::random_device rd;
    
    //随机整数生成器 (范围)
    std::uniform_int_distribution<> dis(0, charSet.size() - 1);

    //字符串流
    std::stringstream ss;
    ss << "Session_id=";
    for (int i = 0; i < 20; ++i) {
        char randomChar = charSet[dis(rd)];
        ss << randomChar;
    }

    // Set-Cookie: Session_id=0EinOcly5Gxtvgz46VfT; Path=/; Domain=.yourdomain.com; HttpOnly
    ss << "username:" << username_;

    
    //存入redis  格式：Session_id:username
    redisContext * r;
    RedisConnRAII(&r, RedisConnPool::Instance());
    assert(r);
    
    redisReply *reply = reinterpret_cast<redisReply*>(redisCommand(r, "set %s %s EX 3600", ss.str().c_str(), username_.c_str()));
    
    if(reply == NULL){
        LOG_INFO("redis set err!");
    }else{
        LOG_INFO("redis set success %s %s", username_.c_str(), value_.c_str());
    }

    ss << "; ";
    
    //cookie值设置
    value_ = ss.str();

}

std::string Cookie::ValidCookie(const std::string & value){
    //在redis中找是否有 value
    redisContext * r;
    RedisConnRAII(&r, RedisConnPool::Instance());
    assert(r);


    // 因为 index.html 中可能还会加载 其它的js等文件   所以需要处理 请求头多Session的格式
    // (?: ) 非捕获组  这样$才能匹配结尾符号
    std::regex pattern("(Session_id=.*?)(?:;|$)");
    
    std::smatch result;

    std::sregex_iterator iter(value.begin(), value.end(), pattern);
    std::sregex_iterator end; //构造没有指定范围的空迭代器  就相当于end了
    
    std::string value_new;
    
    while(iter != end){
        value_new = iter->str(1);
        iter++;
    }
    
    redisReply * reply = reinterpret_cast<redisReply*>(redisCommand(r, "GET %s", value_new.c_str()));

    if(reply->str == NULL){
        username_.clear();
        value_.clear();
        return "";
    }else{
        username_ = reply->str;
        LOG_INFO("redis get success %s %s", username_.c_str(), value_new.c_str());
        return username_;

    }
    
}


void Cookie::SetUser(std::string &username){
    username_ = username;
}


bool Cookie::IsCookie(){
    return value_.empty();
}


std::string Cookie::GetCookie(){
    return value_;
}

std::string Cookie::GetUser(){
    return username_;
} 