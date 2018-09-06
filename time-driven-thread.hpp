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
    void exit();
    void detach();

private:
    void process();
    int rtime;
    bool exit_var;

    std::thread* worker_thread;
};

#endif