#include <thread>
#include <string.h>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <iostream>


#ifndef MESSAGE_DRIVEN_THREAD
#define MESSAGE_DRIVEN_THREAD

template <class T>
class message_driven_thread {
public:
    message_driven_thread();
    //virtual according to cpp implementation of destructor being called on 
    //casted derived class to base class
    virtual ~message_driven_thread();

    void post_message(T data);
    void join();
    virtual void on_message_received(T msg){ };

private:
    void process();

    std::thread* worker_thread;
    std::queue<T> message_queue;
    std::mutex queue_lock;
    std::condition_variable m_cv;
};


//Implementation moved here from message-driven-thread.cpp file
//according to this: https://bytefreaks.net/programming-2/c/c-undefined-reference-to-templated-class-function
//brief: having clas templates`s implementation in cpp file cause linker errors,
// while its used in main, when calling its methods.

template <class T>
message_driven_thread<T>::message_driven_thread (){

    this->worker_thread = new std::thread(&message_driven_thread::process, this);
}

template <class T>
message_driven_thread<T>::~message_driven_thread(){
    worker_thread->join();
}


template <class T>
void message_driven_thread<T>::post_message(T data){
    {
        std::lock_guard<std::mutex> lock(queue_lock);
        message_queue.push(std::move(data));
    }
    m_cv.notify_one();
}

template <class T>
void message_driven_thread<T>::process(){
    while(1){
        T msg;
        {
            std::unique_lock<std::mutex> lk(queue_lock);
            if(message_queue.empty())
                m_cv.wait(lk, [=]{ 
                    return !(this->message_queue.empty()); 
                });
            
            msg = std::move(message_queue.front());
            message_queue.pop();
        }
        
        on_message_received(msg);
    }
}

template <class T>
void message_driven_thread<T>::join(){
    worker_thread->join();
}

#endif