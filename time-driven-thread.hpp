#include <thread>
#include <string.h>
#include <chrono>


#ifndef TIME_DRIVEN_THREAD
#define TIME_DRIVEN_THREAD

class time_driven_thread {
public:
    time_driven_thread(int rtime);
    //virtual according to cpp implementation of destructor being called on 
    //casted derived class to base class
    virtual ~time_driven_thread();

    void join();
    virtual void on_time_routine(){ }

private:
    void process();
    int rtime;

    std::thread* worker_thread;
};


//Implementation moved here from message-driven-thread.cpp file
//according to this: https://bytefreaks.net/programming-2/c/c-undefined-reference-to-templated-class-function
//brief: having clas templates`s implementation in cpp file cause linker errors,
// while its used in main, when calling its methods.

time_driven_thread<T>::time_driven_thread (int rtime){
    this->rtime = rtime;
    this->worker_thread = new std::thread(&time_driven_thread::process, this);
}

time_driven_thread<T>::~time_driven_thread(){
    worker_thread->join();
}

void time_driven_thread<T>::process(){
    
    std::this_thread::sleep_for(std::chrono::miliseconds(rtime));

    on_time_routine();
}

void time_driven_thread<T>::join(){
    worker_thread->join();
}

#endif