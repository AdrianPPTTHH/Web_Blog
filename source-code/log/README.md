1.实现阻塞队列，来存取需要log的消息（其实就是有互斥锁的队列类）



获取时间：

第一种方法：
1.先获取当前时间戳
time_t timer = time(nullptr);

2.将时间戳转换为本地时间结构体tm  （使用localtime）
struct tm *sysTime = localtime(&timer);
struct tm t = *sysTime;

3.取出时间
snprintf(filename, file_len, "%04d-%04d-%04d", t.tm_year + 1900, t.tm_year_mon+1; t.tm_day );



第二种方法：

1.获取时间偏移量
struct timeval t_now = {0,0};
gettimeofday(&t_now, nullptr);

2.获取当前时间戳
time_t timer = t_now.tv_sec;

3.将时间戳转换为本地时间结构体
struct tm *sysTime = localtime(timer);
struct tm t = *sysTime;