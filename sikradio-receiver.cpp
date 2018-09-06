
#include <iostream>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <map>
#include <queue>
#include <vector>
#include <mutex>
#include <endian.h>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

#include "utils.hpp"
#include "message-driven-thread.hpp"
#include "time-driven-thread.hpp"
#include "err.h"

void parse_command_line(int argc, char** argv,
                        std::string& discovery_address,
                        std::string& sender_name,
                        int& ctrl_port,
                        int& ui_port,
                        int& bsize,
                        int& fsize,
                        int& rtime);


class timer : public time_driven_thread {
public:
    timer(int ctrl_port, std::string discovery_address);
    ~timer();
private:
    int sock;
    sockaddr_in destination_address;
    socklen_t destination_address_len;
    void on_time_routine();
    std::string lookup_msg;
}

int main(){
    std::string discovery_address;
    std::string sender_name;
    int ctrl_port;
    int ui_port;
    int bsize;
    int fsize;
    int rtime;

}

// ******************************************************
// Timer class responsible for every 5 second updates
// ******************************************************

// every 5 second is update time in app as its time needed for to send new lookups
timer::timer(int ctrl_port, std::string discovery_address) : time_driven_thread(5){ 
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) syserr("Could not create broadcast socket.");

    int flag = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&flag, sizeof flag) < 0)
        syserr("Could not set socket options for broadcast");

    memset(&destination_address, 0, sizeof destination_address);
    destination_address.sin_family = AF_INET;
	destination_address.sin_port = htons(ctrl_port);
    if (inet_aton(discovery_address.data(), &destination_address.sin_addr) == 0)
        syserr("Could not translate MCAST_ADDRESS to valid address");

    destination_address_len = sizeof destination_address;
    lookup_msg = LOOKUP;
}

timer::~~timer(){
    close(sock);
}

void timer::on_time_routine(){

    sendto(sock, lookup_msg.data(), lookup_msg.size(), 0, 
        (struct sockaddr *) &destination_address, destination_address_len);


    //TODO send update message to update list

}



// ******************************************************
// Function for parsing command line arguments with boost
// ******************************************************
void parse_command_line(int argc, char** argv,
                        std::string& discovery_address,
                        std::string& sender_name,
                        int& ctrl_port,
                        int& ui_port,
                        int& bsize,
                        int& fsize,
                        int& rtime){

    po::options_description desc("Allowed options");
    po::variables_map vm;

    try {
        desc.add_options()
            ("help,h", "prints help message")
            (",d", po::value<std::string>(&discovery_address)->value_name("ADDRESS")->default_value("255.255.255.255"), "address for detecting stations")
            (",C", po::value<int>(&ctrl_port)->value_name("PORT")->default_value(30234), "data port used for control packets transmition")
            (",U", po::value<int>(&ui_port)->value_name("PORT")->default_value(10234), "port used for TCP connection to GUI")
            (",b", po::value<int>(&bsize)->value_name("SIZE")->default_value(65536),"size of receiver buffor")
            (",R", po::value<int>(&rtime)->default_value(250), "time in miliseconds between retransmitions of requests for packets")
            (",n", po::value<std::string>(&sender_name)->value_name("NAME")->default_value("Nienazwany Nadajnik"),"name of the sender")
        ;

        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);


    }
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << desc << std::endl;
        exit(1);
    }

    if (vm.count("help")) {
        std::cerr << desc << "\n";
        exit(1);
    }
}