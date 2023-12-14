#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <unistd.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <assert.h>

template <class T>
class BlockDeque{
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();
    
    void clear();
    
    bool full();

    bool empty();
    
    void close();
    
    size_t size();

    size_t capacity();
    
    T front();
    
    T back();

    void push_back(const T &i);

    void push_front(const T &i);

    bool pop(T &i);

    bool pop(T &i, int timeout);

    // 唤醒所有消费者线程
    void flush();

private:
    std::deque<T> deq_;
    
    size_t capacity_;

    std::mutex mtx_;
    
    bool isClose_;
    
    std::condition_variable conConsumer_;
    std::condition_variable conProducer_;

};

template <class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity):capacity_(MaxCapacity){
    assert(MaxCapacity);
    isClose_ = false;
}

template <class T>
BlockDeque<T>::~BlockDeque(){
    close();
}

template <class T>
void BlockDeque<T>::close(){
    {
        std::unique_lock<std::mutex> my_unique_lock(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    conConsumer_.notify_all();
    conProducer_.notify_all();
}

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template <class T>
void BlockDeque<T>::flush(){
    conConsumer_.notify_one();
}


template <class T>
T BlockDeque<T>::front(){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}



template<class T>
void BlockDeque<T>::push_back(const T &i){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    
    while(deq_.size() >= capacity_){
        conProducer_.wait(my_unique_lock);
    }

    // 如果没有抢到my_unique_lock中的mtx_ 那么将会停在这等notify  所以不能用下面的
    // conProducer_.wait(my_unique_lock, [this](){
    //     return deq_.size() < capacity_;
    // });

    deq_.push_back(i);
    conConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T &i){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    
    // 如果deq满了则阻塞
    while(deq_.size() >= capacity_){
        conProducer_.wait(my_unique_lock);
    }

    // conProducer_.wait(my_unique_lock, [this](){
    //     return deq_.size() < capacity_;
    // });

    deq_.push_front(i);
    conConsumer_.notify_one();
}


template<class T>
bool BlockDeque<T>::pop(T &i){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);

    // 如果deq空则阻塞
    while(deq_.empty()){
        conConsumer_.wait(my_unique_lock);
        if(isClose_){
            return false;
        }
    }
    
    i = deq_.front();
    deq_.pop_front();
    conProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &i, int timeout){
    std::unique_lock<std::mutex> my_unique_lock(mtx_);
    
    // 如果deq空则阻塞
    while(deq_.empty()){
        if(conConsumer_.wait_for(my_unique_lock, std::chrono::seconds(timeout)) == std::cv_status::timeout ){
            return false;
        }
        if(isClose_){
            return false;
        }
    }

    i = deq_.front();
    deq_.pop_front();
    conProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}


#endif