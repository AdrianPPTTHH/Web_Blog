#include "./server/server.h"
#include <iostream> 

using namespace std;

int main(){

    WebServer webserver(
        8876,        //port
        1,           //trigMode
        60000,       //timeoutMS
        true,        //OptLinger
        "localhost", //sqlHost
        3306,        //sqlPort
        "web_admin", //sqlUsername
        "", //sqlPassword
        "", //dbname
        30,          //sqlconnPoolNum
        "127.0.0.1", //redisHost
        6379,        //redisPort
        50,          //threadNum,
        true,        //openLog
        2,           //logLevel
        4096         //logSize
    );

    webserver.Start();
    
    return 0;
}