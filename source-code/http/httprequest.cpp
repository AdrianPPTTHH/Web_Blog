#include "httprequest.h"

// 静态成员 类外初始化
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML = {
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/project",
    "/markdown",
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG = {
    {"/register.html", 0},
    {"/login.html", 1},
    {"/index.html", 2},
    {"/welcome.html", 3},
    {"/project.html", 4},
    {"/markdown.html", 5},
};

void HttpRequest::Init(){
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const{
    size_t index = path_.find_last_of(".");
    std::string extension = path_.substr(index + 1);


    // 防止css和js也keep-alive 出现解析顺序错误
    if(header_.count("Connection") == 1 && (extension == "md" || extension == "html" || extension == "mp4") ){
        return header_.find("Connection")->second == "keep-alive" && (version_ == "1.1" || version_ == "HTTP/1.1");
    }
    return false;
}

bool HttpRequest::parse(Buffer &buff){
    // 换行符\r\n 而不是\n
    const char CRLF[] = "\r\n";

    if(buff.ReadableBytes() <= 0){
        return false;
    }

    // 解析buff
    while(buff.ReadableBytes() && state_ != FINISH){
        // 找结尾字符串位置
        // strstr:在第一个参数中  搜索第二个参数  并且返回找到的第一个位置指针("\r\n"的指针)
        const char* end = strstr(buff.Peek(), CRLF);
        std::string line = "";

        // POST数据结尾没有\r\n
        if(end == nullptr){
            //相当于读取buff中所有的数据
            line.assign(buff.Peek(), buff.BeginWriteConst());
        }else{
            //相当于读到下一个\r\n
            line.assign(buff.Peek(), end);
        }
        
        switch(state_){
            // 1.解析请求行
            case REQUEST_LINE:
                if(!ParseRequestLine_(line)){
                    return false;
                }
                // 解析一下路劲 看是否需要+.html  是否满足访问条件
                ParsePath_();
                break;
            // 2.解析请求头
            case HEADERS:
                ParseHeader_(line);
                ParseCookie_();
                if(buff.ReadableBytes() <= 2){
                    state_ = FINISH;
                }
                break;
            // 3.解析请求体
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }

        // 说明buff中没有需要处理的数据了 
        if(end == "" || end == nullptr || end == buff.BeginWrite()){
            break;
        }

        // 读取一行（通过end + 2 找到每行的结尾）
        buff.RetrieveUntil(end + 2);
    }

    LOG_DEBUG("[%s],[%s],[%s]", method_.c_str(), path_.c_str(), version_.c_str());

    return true;
}


// 解析请求行
bool HttpRequest::ParseRequestLine_(const std::string &line){
    // 匹配请求行（从开头和结尾匹配、空格为间隔）
    std::regex pattern("^([^ ]*) ([^ ]*) ([^ ]*)$");

    // 用于储存匹配结果的容器（储存string或const char*)
    std::smatch result;

    if(std::regex_match(line, result, pattern)){
        // 从1开始  0是整个字符串的索引
        method_ = result[1];
        path_ = result[2];
        version_ = result[3];
        state_ = HEADERS;
        return true;
    }

    LOG_ERROR("RequestLine error!");

    return false;
}


// 解析路劲 看是否需要+.html
void HttpRequest::ParsePath_(){
    if(path_ == "/"){
        path_ = "/index.html";
    }else{
        for(auto &item: DEFAULT_HTML){
            if(item == path_){
                path_ += ".html";
                break;
            }
        }
    }

    std::string result;
    result.reserve(path_.size());

    for (std::size_t i = 0; i < path_.size(); ++i) {
        if (path_[i] == '%') {
            if (i + 2 < path_.size() && isxdigit(path_[i + 1]) && isxdigit(path_[i + 2])) {
                int hexValue;
                std::istringstream hexStream(path_.substr(i + 1, 2));
                hexStream >> std::hex >> hexValue;
                result.push_back(static_cast<char>(hexValue));
                i += 2;
            } else {
                // Invalid percent-encoded sequence, leave '%' as is.
                result.push_back('%');
            }
        }
        // else if (path_[i] == '+') {
        //     result.push_back(' ');
        // } 
        else{
            result.push_back(path_[i]);
        }
        
    }

    path_ = result;

}


// 解析请求头
void HttpRequest::ParseHeader_(const std::string& line){
    // 注意 冒号后面有个空格
    std::regex pattern("^([^:]*): (.*)$");
    
    std::smatch result;
    if(std::regex_match(line, result, pattern)){
        header_[result[1].str()] = result[2].str();
    }else{
        state_ = BODY;
    }

}


// 解析Cookie
void HttpRequest::ParseCookie_(){

    if(header_.count("Proxy-Connection") == 1){
        path_ = "/ErrorPorxy.html";
    }
    
    //对于进入默认页面  进行登录验证
    if(DEFAULT_HTML_TAG.find(path_) != DEFAULT_HTML_TAG.end()){
        //如果有cookie字段 处理cookie
        if(header_.find("Cookie") != header_.end()){
            std::string user = cookie_.ValidCookie(header_["Cookie"]);
            if(!user.empty()){
                cookie_.SetCookie();
            }else if(!(path_ == "/index.html" || path_ == "/login.html" || path_ == "/register.html")){
                path_ = "/error.html";
            }
        }
    }
}


void HttpRequest::ParseBody_(const std::string& line){
    // 请求体是POST字符串
    body_ = line;

    // 解析post参数
    ParsePost_();

    state_ = FINISH;

    LOG_DEBUG("RequestBody: %s, len: %d.", body_.c_str(), body_.size());
}


void HttpRequest::ParsePost_(){

    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded"){
        // urlencoded数据编码格式 需要解析 结果储存到post_变量中
        ParseFromUrlencoded_();

        // 如果是默认页面
        if(DEFAULT_HTML_TAG.count(path_)){
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1){
                // 是否是登录页面
                bool isLogin = (tag == 1);

                if(UserVerify(post_["username"], post_["password"], isLogin)){
                    //登录成功操作
                    cookie_.SetUser(post_["username"]);
                    cookie_.SetCookie();
                    //登录成功操作
                    path_ = "/welcome.html";
                }else{
                    path_ = "/passwrong.html";
                }
            }
        }
    }
}


void HttpRequest::ParseFromUrlencoded_(){
    if(body_.size() == 0 )return;
    
    std::string key, value, new_body;
    int num = 0;
    int n = body_.size();
    int i = 0 , j = 0;
    
    // 遍历body_："username=nihao123&passwd=123456"
    while(i < n){
        char ch = body_[i];
        switch(ch){
            // 如果是=那么切割 j 到 i - j
            case '=':
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            // 如果是+那么换成' '
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                if (i + 2 < n && isxdigit(body_[i + 1]) && isxdigit(body_[i + 2])) {
                    // 将16进制转换成字符
                    num = ConverHex(body_[i+1]) * 16 + ConverHex(body_[i+2]);
                    // 这种转换有问题
                    // body_[i + 2] = num % 10 + '0';
                    // body_[i + 1] = num / 10 + '0';
                    body_[i] = static_cast<char>(num);
                    body_.erase(i + 1 , 2);
                }
                break;
            case '&':
                value = body_.substr(j, i - j);
                post_[key] = value;
                j = i + 1;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
        
        // 这个别忘了啊！！！
        i++;
    }
    assert(j < i);

    // 最后一个键值对
    if(post_.count(key) == 0 && j < i){
        value = body_.substr(j, i - j);
        post_[key] = value;
    }


}

// 如果字符是A-F的16进制字符，那么转换成数字 比如A：10  B：11
int HttpRequest::ConverHex(char ch){
    if(ch > 'A' && ch < 'F')return ch - 'A' + 10;
    if(ch > 'a' && ch < 'f')return ch - 'a' + 10;
    return ch - '0';
}



bool HttpRequest::UserVerify(const std::string& username, const std::string& passwd, bool isLogin){
    if(username == "" || passwd == ""){return false;};
    LOG_INFO("Verify username:%s  passwd:%s", username.c_str(), passwd.c_str());

    // 是否是注册账号操作
    bool flag = !isLogin;


    MYSQL *sql; 
    SqlConnRAII(&sql, SqlConnPool::Instance());

    assert(sql);

    unsigned int j = 0;
    // 执行的SQL语句
    char order[256] = {0};

    // 查询出来的字段属性：name、table、type、length、flags....
    MYSQL_FIELD * fields = nullptr;

    // 查询出来的结果：row_count、field_count、current_field、current_row、fields...
    MYSQL_RES * res = nullptr;

    // 原始执行语句
    snprintf(order, 256, "SELECT username, passwd FROM user WHERE username ='%s' LIMIT 1;", username.c_str());
    
    // 执行普通sql查询语句 mysql_query() 执行成功返回0  执行失败mysql_error(sql)获取错误码
    if(mysql_query(sql, order)){
        LOG_DEBUG("Mysql query error:%s", mysql_error(sql));
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    while(MYSQL_ROW row = mysql_fetch_row(res)){
        if(isLogin){
            if(row[1] == passwd)
            {
                flag = true;
            }else{
                flag = false;
            }
        }else{
            flag = false;
        }
    }
    

    // // 预处理语句 使用参数绑定防止sql注入
    // MYSQL_STMT *stmt;
    // MYSQL_BIND bind[1];

    // // 1.初始化stmt绑定到sql
    // stmt = mysql_stmt_init(sql);
    // if (!stmt) {
    //     LOG_ERROR("mysql_stmt_init failed");
    //     return false;
    // }

    // const char* query = "SELECT username, passwd FROM user WHERE username =? LIMIT 1;";

    // // 2.绑定语句到stmt 
    // if(mysql_stmt_prepare(stmt, query, strlen(query))){
    //     LOG_ERROR("mysql_stmt_prepare failed: %s", mysql_stmt_error(stmt));
    //     mysql_stmt_close(stmt);
    //     return false;
    // }

    // // 设置参数属性
    // memset(bind, 0, sizeof(bind));
    // const char *user = username.c_str();
    // bind[0].buffer = (void*)user;
    // bind[0].buffer_length = strlen(user);
    // bind[0].buffer_type = MYSQL_TYPE_STRING;
    
    // // 3.绑定参数到stmt
    // if(mysql_stmt_bind_param(stmt, bind)){
    //     LOG_ERROR("mysql_stmt_bind_param failed: %s", mysql_stmt_error(stmt));
    //     mysql_stmt_close(stmt);
    //     return false;
    // }

    // // 4.执行stmt
    // if(mysql_stmt_execute(stmt)){
    //     LOG_ERROR("mysql_stmt_execute failed: %s", mysql_stmt_error(stmt));
    //     mysql_stmt_close(stmt);
    //     return false;
    // }

    // // 5.储存查询结果
    // if(mysql_stmt_store_result(stmt)){
    //     LOG_ERROR("mysql_stmt_store_result failed: %s", mysql_stmt_error(stmt));
    //     mysql_stmt_close(stmt);
    //     return false;
    // }

    // // 6.获取结果属性（mysql_stmt_result_metadata不会获取结果字段）
    // res = mysql_stmt_result_metadata(stmt);
    // if (res == NULL) {
    //     LOG_ERROR("mysql_stmt_result_metadata failed: %s", mysql_stmt_error(stmt));
    //     mysql_stmt_close(stmt);
    //     return false;
    // }

    // int num_fields = mysql_num_fields(res);
    
    // // 7.取出res中的row字段值 若果没查询到则res == NULL 
    // // C++中 while(*p = NULL) 不会导致循环执行
    // while(MYSQL_ROW row = mysql_fetch_row(res)){
    //     LOG_DEBUG("MYSQL ROW:%s %s",row[0], row[1]);
    //     std::string true_passwd(row[1]);
    //     if(isLogin){
    //         if(passwd == true_passwd){
    //             flag = true;
    //         }else{
    //             flag = false;
    //             LOG_DEBUG("passwd error!");
    //         }
    //     }else{
    //         flag = false;
    //         LOG_DEBUG("user used!");
    //     }
    // }

    mysql_free_result(res);
    // mysql_stmt_close(stmt);
    
    // 注册操作
    if(!isLogin && flag == true){
        LOG_DEBUG("register");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, passwd) VALUES('%s','%s');", 
            username.c_str(), passwd.c_str());
        if(mysql_query(sql, order)){
            LOG_DEBUG("Insert error:%s", mysql_error(sql));
            flag = false;
        }
        flag = true;
    }

    // RAII析构的时候会自动回收  不用手动回收
    // SqlConnPool::Instance()->RecyConn(sql);
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}

std::string HttpRequest::method() const{
    return method_;
}

Cookie HttpRequest::cookie() const{
    return cookie_;
}

std::string HttpRequest::version() const{
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const{
    assert(key != "");
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const{
    assert(key != nullptr);
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }

    return "";
}