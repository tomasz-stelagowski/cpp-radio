#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFLEN 512

struct audio_package {
    uint64_t session_id;
    uint64_t first_byte_num;
    std::string audio_data;
};

int main(void)
{
	int sockfd;
	struct sockaddr_in my_addr;	// my address information
	struct sockaddr_in their_addr; // connector's address information
	socklen_t addr_len;
	int numbytes;
	// char buf[MAXBUFLEN];
	audio_package buf;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	my_addr.sin_family = AF_INET;		 // host byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	my_addr.sin_port = htons(20235);	 // short, network byte order
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(sockfd, (struct sockaddr *)&my_addr,
		sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	addr_len = sizeof(struct sockaddr);
	if ((numbytes = recvfrom(sockfd, &buf, sizeof(audio_package) , 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}

	printf("got packet from %s\n",inet_ntoa(their_addr.sin_addr));
	printf("packet is %d bytes long\n",numbytes);
	printf("packet contains %d %d \"%s\"\n",buf.session_id, buf.first_byte_num, "aaa");

	close(sockfd);

	return 0;
}
