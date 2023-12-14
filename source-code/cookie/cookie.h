#ifndef COOKIE_H
#define COOKIE_H

#include "../redisconnpool/redisconnpool.h"
#include "../redisconnpool/redisconnRAII.h"
#include "../log/log.h"

#include <string>
#include <random>
#include <sstream>
#include <assert.h>
#include <regex>

class Cookie{

public:
    Cookie();
    ~Cookie();
    
    void SetCookie();
    void SetUser(std::string &username);

    bool IsCookie();

    std::string GetCookie();
    std::string GetUser();

    std::string ValidCookie(const std::string & value);
    
private:
    std::string username_;
    std::string value_;

};

#endif