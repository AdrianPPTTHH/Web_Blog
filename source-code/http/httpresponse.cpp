#include "./httpresponse.h"


const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE{
    {".html", "text/html; charset=utf-8"},
    {".xml", "text/xml"},
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
    { ".md",    "text/html; charset=utf-8"},
    { ".MD",    "text/html; charset=utf-8"},
};


const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS{
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {302, "Temporarily Moved"},
};


const std::unordered_map<int, std::string> HttpResponse::CODE_PATH{
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};


HttpResponse::HttpResponse(){
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}


HttpResponse::~HttpResponse(){
    UnmapFile();
}


void HttpResponse::Init(const std::string& srcDir, std::string& path, 
        bool isKeepAlive, const Cookie& cookie, int code){
    assert(srcDir != "");
    if(mmFile_){ UnmapFile(); }
    srcDir_ = srcDir;
    path_ = path;
    code_ = code;
    cookie_ = cookie;
    isKeepAlive_ = isKeepAlive;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

// 将对象获取的内容 写入buff （在httpconn中将&buff写入fd中）
void HttpResponse::MakeResponse(Buffer & buff){

    // stat()获取文件元数据  成功返回0
    // S_ISDIR()用于获取stat结构中的st_mode 文件类型定位 是否是目录
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)){
        code_ = 404;
        ErrorHtml_();
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)){ //查看是否对文件有读权限
        code_ = 403;
    }
    else if(code_ == -1){
        code_ = 200;
    }
    
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

// 返回文件内容 映射到 内存的指针
char * HttpResponse::File(){
    // return mmFile_;
    return  const_cast<char*>(zip_content.data());
}


size_t HttpResponse::FileLen() const{
    return file_len;
}


void HttpResponse::ErrorHtml_(){
    if(CODE_PATH.count(code_) == 1){
        // path_ = "/404.html"
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}


void HttpResponse::AddStateLine_(Buffer& buff){
    std::string status;
    if(CODE_STATUS.count(code_) == 1){
        status = CODE_STATUS.find(code_)->second;
    }else{
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}


void HttpResponse::AddHeader_(Buffer & buff){
    buff.Append("Content-Encoding: gzip\r\n");
    buff.Append("Vary: Accept-Encoding\r\n");

    buff.Append("Connection:");
    if(isKeepAlive_){
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=8 , timeout=120\r\n");
    }else{
        buff.Append("close\r\n");
    }
    
    if(cookie_.GetUser() == ""){
        ;
    }else{
        buff.Append("Set-Cookie: " + cookie_.GetCookie() + "\r\n");
    }
    
    buff.Append("Content-type:" + GetFileType_() + "\r\n");

}


void HttpResponse::AddContent_(Buffer & buff){
    
    int srcFd = open((srcDir_ + path_ ).data(), O_RDONLY);
    if(srcFd < 0){
        ErrorContent(buff, "File NotFound!");
        return;
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());

    //判断是否是md格式的文件
    size_t index = path_.find_last_of(".");
    std::string extension;

    if(index != std::string::npos){
        extension  = path_.substr(index + 1);
    }

    if(extension != "md" && extension != "MD"){
        // 映射到内核的地址(null则自动分配）、要映射内存区域大小、需要保护的标志、映射对象类型、文件描述符、文件偏移量
        // 返回的是被映射区域的虚拟地址
        int* mmRet = (int*)mmap(NULL, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);

        if(*mmRet == -1){
            ErrorContent(buff, "File NotFound!");
            return;
        }

        // 强制转换mmRet指向内容的格式， 从指向int转换成指向char类型，即字符串指针
        mmFile_ = (char*)mmRet;

        file_len = mmFileStat_.st_size;

        zip_content = compressData(mmFile_, file_len);

        file_len = zip_content.size();
        
        buff.Append("Content-length: " + std::to_string(file_len) + "\r\n\r\n");


    }else{
        char file_content[mmFileStat_.st_size];
        int i = read(srcFd, &file_content, mmFileStat_.st_size);

        //创建cmark结点
        cmark_node * node = cmark_parse_document(file_content, strlen(file_content), CMARK_OPT_DEFAULT);

        //转HTML
        char *html = cmark_render_html(node, CMARK_OPT_DEFAULT);

        std::string html_all;

        char file_buf[8192];

        // 读取head.html 和 end.html
        int h_fd = open((srcDir_ + "markdown/head.html").data(), O_RDONLY);
        int e_fd = open((srcDir_ + "markdown/end.html").data(), O_RDONLY);

        if(h_fd == -1 || e_fd == -1){
            ErrorContent(buff, "File Error!");
            return;
        }
        
        // 拼接
        read(h_fd,file_buf, sizeof(file_buf));
        html_all += file_buf;

        memset(file_buf, 0, sizeof(file_buf));

        html_all += html;

        read(e_fd, file_buf, sizeof(file_buf));
        html_all += file_buf;
        
        close(h_fd);
        close(e_fd);

        mmFile_ = const_cast<char *>(html_all.data());

        file_len = mmFileStat_.st_size;

        zip_content = compressData(mmFile_, html_all.size());

        file_len = zip_content.size();
        
        buff.Append("Content-length: " + std::to_string(file_len) + "\r\n\r\n");

        cmark_node_free(node);
    }

    close(srcFd);
    // buff.Append(mmFile_, mmFileStat_.st_size);

}


void HttpResponse::ErrorContent(Buffer& buff, std::string message){
    std::string body,status;

    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1){
        status = CODE_STATUS.find(code_) -> second;
    }else{
        status = "Bad Request";
    }

    body += std::to_string(code_) + ":" + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>MyWebServer</em></body></html>";
    
    buff.Append("Content-length:" + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
    
}

std::string HttpResponse::GetFileType_(){
    if(path_ == ""){
        return "";
    }
    
    size_t index = path_.find_last_of('.');

    if(index == std::string::npos){
        return "text/plain";
    }
    else{
        std::string suffix = path_.substr(index);
        if(SUFFIX_TYPE.find(suffix) != SUFFIX_TYPE.end()){
            return SUFFIX_TYPE.find(suffix)->second;
        }
    }
    
    return "text/plain";
}


void HttpResponse::UnmapFile(){
    if(mmFile_){
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}



std::string HttpResponse::compressData(const char* data, size_t size) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return "";
    }

    zs.next_in = (Bytef*)data;
    zs.avail_in = size;

    int ret;
    char outbuffer[32768];
    std::string outstring;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }

    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        return "";
    }

    return outstring;
}