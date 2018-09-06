#include <string>

#define LOOKUP "ZERO_SEVEN_COME_IN"
#define REXMIT "LOUDER_PLEASE"
#define REPLY "BOREWICZ_HERE"
#define MAKEREPLY(mcast_addr, data_port, station_name) "BOREWICZ_HERE " + mcast_addr + " " \
    + std::to_string(data_port) + " " + station_name

#define INPUT "INPUT"
#define EXIT "EXIT"
#define UDP_BUFFER_SIZE 1400



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