#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <functional>


// 线程池类
class Threadpool{
public:
    explicit Threadpool(int threadMin = 10, int threadMax = 50, int queueCapacity = 30);

    Threadpool() = default;

    Threadpool(Threadpool&&) = default;

    // ~Threadpool();
        
    // 工作线程函数
    void *work();

    // 管理者线程函数
    void *manager();

    //添加任务函数
    // template <class T>
    // void threadPoolAdd(T&& func);

    // T&& 通用引用，既可以传入左值引用又可以传入右值引用  如果需要参数可以使用std::bind() 返回一个右值引用
    template <class T>
    void threadPoolAdd(T&& func){
        std::unique_lock<std::mutex> my_unique_lock(mutexPool);
        
        while(queueSize != queueCapacity && !shutdown){
            notFull.wait(my_unique_lock, [this](){
                return queueSize < queueCapacity;
            });

            if(shutdown){
                my_unique_lock.unlock();
                return;
            }
            
            taskQ.push(func);
            queueSize++;
            my_unique_lock.unlock();
            
            notEmpty.notify_one();
            break;
        }
        
    }

    // 销毁池函数
    int threadPoolDestroy();

private:
    // 任务队列
    // std::queue<Task> taskQ;
    std::queue<std::function<void()>> taskQ;

    // 容量和大小
    int queueCapacity;
    int queueSize;
    
    // 管理者id
    std::thread managerID;
    
    // 工作线程存放的vector
    std::vector<std::thread> threadIDs;
    
    // 最大、最小线程
    int minNum;
    int maxNum;

    // 忙线程数
    std::atomic<int> busyNum;

    // 存活线程个数
    int liveNum;

    // 需要销毁的个数
    int exitNum;

    // 线程池的锁
    std::mutex mutexPool;

    // 忙线程锁
    std::mutex mutexBusy;

    std::condition_variable notEmpty;
    std::condition_variable notFull;

    // 是否需要销毁
    bool shutdown;
};


#endif