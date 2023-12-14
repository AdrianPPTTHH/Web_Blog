#include "./threadpool.h"

// 管理者线程的操作次数 （一次创建销毁多少个线程）
const int Control_Num = 3;

// 构造函数，初始化线程池属性，创建线程
Threadpool::Threadpool(int threadMin, int threadMax, int queueCapacity):minNum(threadMin),\
maxNum(threadMax),queueCapacity(queueCapacity),busyNum(0), \
liveNum(threadMin),exitNum(0),queueSize(0),shutdown(0) 
{
    // 启动管理者线程
    managerID = std::thread(&Threadpool::manager, this);

    // 创建工作线程
    for(int i = 0 ; i < threadMin ; i++){
        this->threadIDs.push_back(std::thread(&Threadpool::work, this));
    }
}

void* Threadpool::work(){
    while(1){
        std::function<void()>func;
        {
            // 加锁
            std::unique_lock<std::mutex> my_unique_lock(mutexPool);

            // 判断任务队列是否为空，如果空则阻塞（其它情况继续执行，shutdown也是让线程自杀）
            notEmpty.wait(my_unique_lock, [this](){
                return queueSize != 0 || shutdown || exitNum > 0;
            });
            
            // 如果是线程池是shutdown状态，或者exitNum > 0（有线程需要退出）
            if(shutdown){
                return NULL;
            }else if(exitNum > 0){
                // 需要退出的线程数-1
                exitNum--;
                
                // 存活线程数-1
                if(liveNum > minNum){
                    liveNum--;
                }
                return NULL;
            }
            
            // cout << "thread_id:" << std::this_thread::get_id() << " working..." << endl; 
            
            // 如果能执行到这里，说明任务队列有任务需要处理，并且线程正常执行。
            // Task task;
            // task.function = taskQ.front().function;
            // task.arg = taskQ.front().arg;
            func = taskQ.front();

            taskQ.pop();
            queueSize--;

            // 解锁（因为线程池中的数据已经调用完了 除了忙线程数） 执行后面任务
        }
        
        // 给忙线程数 这个变量加锁
        mutexBusy.lock();
        busyNum++;
        mutexBusy.unlock();
        
        // 执行任务
        // task.function(task.arg);
        func();

        mutexBusy.lock();
        busyNum--;
        mutexBusy.unlock();
        
        notFull.notify_all();
        
    }
}


void* Threadpool::manager(){
    while(!shutdown){
        // 每间隔3秒管理一次
        std::chrono::milliseconds t(1000);
        std::this_thread::sleep_for(t);

        std::unique_lock<std::mutex> my_unique_lock(mutexPool);
        int queueSize_ = queueSize;
        int liveNum_ = liveNum;
        int maxNum_ = maxNum;
        int minNum_ = minNum;
        my_unique_lock.unlock();

        mutexBusy.lock();
        int busyNum_ = busyNum;
        mutexBusy.unlock();
        
        // 1.创建线程

        // 当任务个数 > 存活的线程个数 && 存活的线程个数 < 最大线程数量
        if(queueSize_ > liveNum_ && liveNum_ < maxNum_){
            // cout << "线程不够，正在创建线程...." << endl;

            int count = 0;
            for(int i = 0; count < Control_Num && i < maxNum_ ; i++){
                my_unique_lock.lock();
                // 如果之前有线程被销毁了（return） 那么重新启用
                if(threadIDs[i].joinable()){
                    this->threadIDs[i].join();
                    this->threadIDs[i] = std::thread(&Threadpool::work, this);
                }else{
                    this->threadIDs.push_back(std::thread(&Threadpool::work, this));
                }
                count++;
                liveNum++;
                my_unique_lock.unlock();
            }
        }

        // 2.销毁线程

        // 忙的线程个数*1.8 < 存活的线程个数 && 存活的线程个数 > 0

        if(busyNum * 1.8 < liveNum && liveNum > minNum){
            // cout << "空线程过多，正在杀死线程...." << endl;

            my_unique_lock.lock();
            exitNum = Control_Num;
            my_unique_lock.unlock();

            for(int i = 0 ; i < Control_Num; i++){
                notEmpty.notify_one();
            }
        }
        
    }

    return NULL;
}


// T&& 通用引用，既可以传入左值引用又可以传入右值引用  如果需要参数可以使用std::bind() 返回一个右值引用
// template <class T>
// void Threadpool::threadPoolAdd(T&& func){
// // void Threadpool::threadPoolAdd(void(*func)(void *), void*arg){
//     std::unique_lock<std::mutex> my_unique_lock(mutexPool);
    
//     while(queueSize != queueCapacity && !shutdown){
//         notFull.wait(my_unique_lock, [this](){
//             return queueSize < queueCapacity;
//         });

//         if(shutdown){
//             my_unique_lock.unlock();
//             return;
//         }
        
//         // Task temp_t;
//         // temp_t.function = func;
//         // temp_t.arg = arg;
        
//         // taskQ.push(temp_t);
        
//         taskQ.push(func);
//         queueSize++;
//         my_unique_lock.unlock();
        
//         notEmpty.notify_one();
//         break;
//     }
    
// }



// 销毁池函数
int Threadpool::threadPoolDestroy(){

    // 关闭线程池
    shutdown = 1;
    notEmpty.notify_all(); // 通知所有等待线程退出


    // cout << "回收管理者线程" << pool->managerID.get_id() << endl;
    managerID.join();

    for (int i = 0; i < threadIDs.size(); i++) {
        if (threadIDs[i].joinable()) {
            threadIDs[i].join();
        }
    }

    // 释放taskQ
    while(!taskQ.empty()){
        taskQ.pop();
    }


    return 1;
}

