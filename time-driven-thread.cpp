#include <thread>
#include <string.h>
#include <chrono>

#include "time-driven-thread.hpp"

time_driven_thread::time_driven_thread (int rtime){
    this->rtime = rtime;
    this->worker_thread = new std::thread(&time_driven_thread::process, this);
}

time_driven_thread::~time_driven_thread(){
    worker_thread->join();
}

void time_driven_thread::process(){
    
    std::this_thread::sleep_for(std::chrono::milliseconds(rtime));

    on_time_routine();
}

void time_driven_thread::join(){
    worker_thread->join();
}
