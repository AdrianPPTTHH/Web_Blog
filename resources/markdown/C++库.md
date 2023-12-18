### 1、静态库、动态库

##### 	静态库

​			1、生成.o文件： **g++ -c add.c -o add.o**

​			2、使用ar工具： **ar rcs lib库名.a add.o sub.o**

​			3、使用静态库编译： **g++ test.c lib库名.a -o test.out**

​		一般作者给出lib***.a文件 和 \*\*\*.h头文件。

```C++
#ifndef xxx_TYPE

#defince xxx_TYPE

int add(int, int);

...

#endif
```



##### 	动态库

​			1、生成.o文件： **g++ -c add.c -o add.o -fPIC**（生成与偏移位置无关的函数地址）

​			2、生成动态库文件：**g++ -shared -o lib库名.so add.o sub.o** 

​			3、使用动态库（-l指定库名 -L指定路径）：**g++ test.c -o test.out -lmymath -L./lib**  （库名要去掉lib和.so）

​			4、改变动态库路劲：**export LD_LIBRARY_PATH = 动态库路径**

​												**./test**



### 2、CMAKE、MAKE

##### 	cmake

​		CMakeLists.txt

```cmake
project(xxx_project)

# 指定C++版本
set (CMAKE_CXX_STANDARD_REQUIRED True)
set (CMAKE_CXX_STANDARD 17)

# 添加可执行文件
add_executable(run ./xxx.cpp)
```

写好CMakeLists.txt文件之后，执行cmake，生成makefile

##### 	make

​		用上述命令生成makefile之后，执行make生成可执行文件。



### 3、信号

##### 设置信号阻塞

```C++
	#include <signal.h>

	sigset_t set,old_set;

	// 初始化信号集合
	sigemptyset(&set);

	// 设置信号
	sigaddset(&set, SIGINT);  //ctrl+c的信号

	sigaddset(&set, SIGQUIT);  //ctrl+\的信号

	sigaddset(&set, SIGKILL);  //kill的信号，这个位设置了没有用

	//设置阻塞
	int res = sigprocmask(SIG_BLOCK, &set, &oldset);

```

##### 注册信号捕捉

```C++
    #include <signal.h>
    
    //处理僵尸进程
    void signal_handler(int signum){
        while(waitpid(-1, nullptr, WNOHANG > 0 ))
    }
    
	//注册SIGCHLD信号使用signal_handler函数
    signal(SIGCHLD, signal_handler);
```













