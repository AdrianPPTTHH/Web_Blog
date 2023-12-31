cmake_minimum_required(VERSION 3.16.0)

project(webserver)

add_executable(run.exe
    ./source-code/main.cpp
    ./source-code/buffer/buffer.cpp
    ./source-code/epoller/epoller.cpp
    ./source-code/http/httprequest.cpp
    ./source-code/http/httpresponse.cpp
    ./source-code/http/httpconn.cpp
    ./source-code/timer/heaptimer.cpp
    ./source-code/log/log.cpp
    ./source-code/threadpool/threadpool.cpp
    ./source-code/sqlconnpool/sqlconnpool.cpp
    ./source-code/sqlconnpool/sqlconnRAII.h
    ./source-code/server/server.cpp
    ./source-code/redisconnpool/redisconnpool.cpp
    ./source-code/redisconnpool/redisconnRAII.h
    ./source-code/cookie/cookie.cpp
)

#C++版本
target_compile_features(run.exe PRIVATE cxx_std_17)

#头文件搜索路劲
target_include_directories(run.exe PRIVATE ./source-code)

#link路劲添加
target_link_directories(run.exe PRIVATE /home/kali/PTH/cmark_include/lib)

target_link_libraries(run.exe mysqlclient)
target_link_libraries(run.exe hiredis)
target_link_libraries(run.exe libcmark.so.0.30.3)
target_link_libraries(run.exe pthread)
target_link_libraries(run.exe ${CMAKE_DL_LIBS} z)