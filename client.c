#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "sockwrap.h"
#include "config.h"
#include "sniffwrap.h"

int main(int argc, char *argv[])
{
    int socketfd;
	
    if (argc < 2) {
       fprintf(stderr, "usage %s hostname\n", argv[0]);
       exit(EXIT_FAILURE);
    }
	
	char *hostname = argv[1];
	socketfd = connect_to_host(hostname, PUBLISHER_PORT);
	
	char buffer[SOCKET_BUFFER_SIZE];
	while( read(socketfd, buffer, SOCKET_BUFFER_SIZE) > 0) 
	{
		const struct sniff_ip *ip = (struct sniff_ip*)(buffer + SIZE_ETHERNET);
		if(ip->ip_p == 1)
		{
			char str[256];
			sprintf(str, "Jacked an ICMP packet from: %-16s to: ", inet_ntoa(ip->ip_src));
			sprintf(str + strlen(str), "%-16s\n", inet_ntoa(ip->ip_dst));
			printf("%s", str);
		}
	}
	
	printf("Disconnected due to server...\n");
    
	return 0;
}