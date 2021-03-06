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
#include "constants.hpp"
namespace po = boost::program_options;

#include "message-driven-thread.hpp"
#include "time-driven-thread.hpp"
#include "err.h"

void parse_command_line(int argc, char** argv,
                        std::string& mcast_address,
                        std::string& sender_name,
                        int& data_port,
                        int& ctrl_port,
                        int& psize,
                        int& fsize,
                        int& rtime);


// Struct representing messages sent to message_dirven_thread

struct audio_package {
    uint64_t session_id;
    uint64_t first_byte_num;
    std::string audio_data;
};

// Struct used for externaling package to one block of data
// its not used as main package tranportede between thread 
// because Flexible Array Member (FAM) could not be easily 
// passed by value, which is by desing of thread communication in this
// program, thats why in program audio_package is used leveraging std::string
// and just before send its rewritten into net_auio_package
struct net_audio_package {
    uint64_t session_id;
    uint64_t first_byte_num;
    uint8_t audio_data[1];
};


//message send between threads
struct message {
    std::string type;
    audio_package msg;
};


class sender_thread : public message_driven_thread<message> {
public:
    sender_thread(int data_port, int psize, std::string mcast_address);
    void on_message_received(message msg);
    virtual ~sender_thread();
private:
    int sock;
    int psize;
    sockaddr_in destination_address;
    socklen_t destination_address_len;
    void on_input_message(message msg);
};

class rexmit_thread : public time_driven_thread {
public:
    rexmit_thread(int rtime, int psize, int fsize, message_driven_thread<message>* packet_sender);
    void on_time_routine();
    void post_package(message msg);
    void post_rexmit(std::string package_numbers);
private:
    int history_count;
    message_driven_thread<message>* packet_sender;
    std::set<uint64_t> rexmits_nums;
    std::mutex rexmits_nums_mutex;
    std::queue<uint64_t> packets_buffor;
    std::map<uint64_t, audio_package> packets_storage;
    std::mutex packets_mutex;
};

void stdin_reader(uint64_t session_id, int psize, message_driven_thread<message>* packet_sender, rexmit_thread* history_manager);

void network_listener(int ctrl_port, std::string mcast_addr, int data_port, 
    std::string station_name, message_driven_thread<message>* packet_sender, rexmit_thread* history_manager);

int main(int argc, char** argv) {
    std::string station_name;
    int rtime;
    int fsize;
    int psize;
    int ctrl_port;
    int data_port;
    std::string mcast_address;

    //parsing command line arguments
    parse_command_line(argc, argv, mcast_address, station_name, data_port, ctrl_port, psize, fsize, rtime);

    //calculating session_id
    uint64_t session_id = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    message_driven_thread<message>* broadcast_sender = new sender_thread(data_port, psize, mcast_address);
    rexmit_thread* history_manager = new rexmit_thread(rtime, psize, fsize, broadcast_sender);
    
    std::thread stdin_reader_thread(stdin_reader, session_id, psize, broadcast_sender, history_manager);
    std::thread network_listener_thread(network_listener, ctrl_port, 
        mcast_address, data_port, station_name, broadcast_sender, history_manager);

    stdin_reader_thread.join();
    broadcast_sender->join();
    history_manager->exit();
    network_listener_thread.detach();

    return 0;
}

// ******************************************************
// Responsible for reading input in psize chunks of data
// works in its own thread, proceeds data to broadcasting thread
// ******************************************************
void stdin_reader(uint64_t session_id, int psize, message_driven_thread<message>* packet_sender, rexmit_thread* history_manager){
    char* input_buff = new char[psize];
    uint64_t packet_number = 0;

    do {
        memset(input_buff, 0, psize);
        std::cin.read(input_buff, psize);
        if(std::cin){
            packet_sender->post_message({ 
                INPUT, 
                { session_id, packet_number, std::string(input_buff, psize) }
            });
            history_manager->post_package({ 
                INPUT, 
                { session_id, packet_number, std::string(input_buff, psize) }
            });
            packet_number += psize;
        }
    } while(std::cin);

    packet_sender->post_message({EXIT, {0, 0, ""}});

    delete[] input_buff;
}


// ******************************************************
// Responsible for listening for LOOKUP and REXMIT messages
// ******************************************************
void network_listener(int ctrl_port, std::string mcast_addr, int data_port, 
    std::string station_name, message_driven_thread<message>* packet_sender, rexmit_thread* history_manager){

    char* udp_buffer = new char[UDP_BUFFER_SIZE];
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) syserr("Could not create listening socket");

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(ctrl_port);

    sockaddr client_address;
    socklen_t caddr_len = sizeof(client_address);

    if(bind(sock, (struct sockaddr*)&server_address, (socklen_t)sizeof(server_address)) < 0)
        syserr("Could not bind listening socket");

    for(;;){
        memset(udp_buffer, 0, (size_t)UDP_BUFFER_SIZE);
        if(recvfrom(sock, udp_buffer, UDP_BUFFER_SIZE, 0, &client_address, &caddr_len) < 0)
            syserr("Error with receiving datagram");
        
        std::string received_request(udp_buffer);

        std::vector<std::string> strs;
        boost::split(strs, received_request, boost::is_any_of(" "));
        
        if(strs[0] == LOOKUP){
            std::string response = REPLY(mcast_addr, data_port, station_name);
            if(sendto(sock, response.data(), response.size(), 0, &client_address, caddr_len) < 
                (ssize_t)response.size())
                syserr("Sending lookup message.");

        } else if(strs[0] == REXMIT){
            history_manager->post_rexmit(strs[1]);
        }
    }

    close(sock);
    delete[] udp_buffer;
}

// ******************************************************
// Sender class implementation
// ******************************************************
sender_thread::sender_thread(int data_port, int psize, std::string mcast_address){
    this->psize = psize;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) syserr("Could not create broadcast socket.");

    int flag = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&flag, sizeof flag) < 0)
        syserr("Could not set socket options for broadcast");

    //binding local address
    sockaddr_in local_address;
    memset(&local_address, 0, sizeof local_address);
    local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = htons(0);


    memset(&destination_address, 0, sizeof destination_address);
    destination_address.sin_family = AF_INET;
	destination_address.sin_port = htons(data_port);
    if (inet_aton(mcast_address.data(), &destination_address.sin_addr) == 0)
        syserr("Could not translate MCAST_ADDRESS to valid address");

    destination_address_len = sizeof destination_address;
}

sender_thread::~sender_thread(){
    close(sock);
}

void sender_thread::on_message_received(message msg){
    if(msg.type == INPUT){
        on_input_message(msg);
    } else if(msg.type == EXIT){
        exit = true;
    }
}

void sender_thread::on_input_message(message msg){
    // net audio package off cpp necesity has audio_data of size 1 byte.
    // it needs to be resized to psize so delta is +(psize-1)

    size_t net_audio_package_size = (sizeof(net_audio_package) + psize);
    net_audio_package* package = (net_audio_package*)malloc(net_audio_package_size);

    package->session_id = htobe64(msg.msg.session_id);
    package->first_byte_num = htobe64(msg.msg.first_byte_num);
    std::memcpy(package->audio_data, msg.msg.audio_data.data(), net_audio_package_size);

    sendto(sock, package, net_audio_package_size-1, 0, 
        (struct sockaddr *) &destination_address, destination_address_len);

    free(package);
}

// ******************************************************
// Rexmit and packet history manager class implementation
// ******************************************************

rexmit_thread::rexmit_thread(int rtime, int psize, int fsize, message_driven_thread<message>* packet_sender) 
    : time_driven_thread::time_driven_thread(rtime){
    
    this->packet_sender = packet_sender;
    history_count = fsize / psize;
}

void rexmit_thread::on_time_routine(){
    std::vector<uint64_t> nums;
    {
        std::lock_guard<std::mutex> lock(rexmits_nums_mutex);
        nums.assign(rexmits_nums.begin(), rexmits_nums.end());
        rexmits_nums.clear();
    }
    {
        std::lock_guard<std::mutex> lock(packets_mutex);
        std::for_each(std::begin(nums), std::end(nums), [=](uint64_t num){ 
            std::map<uint64_t, audio_package>::iterator it = packets_storage.find(num);
            
            if(it != std::end(packets_storage)){
                packet_sender->post_message({ INPUT, it->second });
            }
         });
    }
}

void rexmit_thread::post_package(message msg){
    std::lock_guard<std::mutex> lock(packets_mutex);

    packets_buffor.push(msg.msg.first_byte_num);
    packets_storage.insert( std::pair<uint64_t, audio_package>(msg.msg.first_byte_num, msg.msg) );

    if((int)packets_buffor.size() > history_count){
        uint64_t package_to_remove = packets_buffor.front();
        packets_buffor.pop();
        packets_storage.erase(package_to_remove);
    }
}

void rexmit_thread::post_rexmit(std::string package_numbers){
    std::vector<std::string> nums;
    boost::split(nums, package_numbers, boost::is_any_of(","));

    std::lock_guard<std::mutex> lock(rexmits_nums_mutex);
    for_each(nums.begin(), nums.end(), 
        [=](std::string num){ rexmits_nums.insert( (uint64_t)atoi(num.data()) ); });
}

// ******************************************************
// Function for parsing command line arguments with boost
// ******************************************************
void parse_command_line(int argc, char** argv,
                        std::string& mcast_address,
                        std::string& sender_name,
                        int& data_port,
                        int& ctrl_port,
                        int& psize,
                        int& fsize,
                        int& rtime){

    po::options_description desc("Allowed options");
    po::variables_map vm;

    try {
        desc.add_options()
            ("help,h", "prints help message")
            (",a", po::value<std::string>(&mcast_address)->value_name("ADDRESS")->required(), "targeted broadcast address. REQUIRED")
            (",P", po::value<int>(&data_port)->value_name("PORT")->default_value(20234), "data port used for data transmission")
            (",C", po::value<int>(&ctrl_port)->value_name("PORT")->default_value(30234), "data port used for control packets transmition")
            (",p", po::value<int>(&psize)->value_name("SIZE")->default_value(512),"size of send packets in bytes")
            (",f", po::value<int>(&fsize)->value_name("SIZE")->default_value(128000), "size of FIFO queue in bytes")
            (",R", po::value<int>(&rtime)->default_value(250), "time in miliseconds between retransmitions of packets")
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