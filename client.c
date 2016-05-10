#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "sockwrap.h"
#include "config.h"
#include "sniffwrap.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
       fprintf(stderr, "usage %s hostname\n", argv[0]);
       exit(EXIT_FAILURE);
    }
	
	char *hostname = argv[1];
	int socketfd = connect_to_host(hostname, PUBLISHER_PORT);
	
	char buffer[SOCKET_BUFFER_SIZE];
	
	int icmp = 0, tcp = 0, udp = 0, other = 0;
	uint16_t networkLen;
	while(socketfd != -1)
	{
		if(read(socketfd, &networkLen, sizeof(networkLen)) > 0) 
		{
			networkLen = ntohs(networkLen); 
			read(socketfd, buffer, networkLen);
			
			const struct sniff_ip *ip = (struct sniff_ip*)(buffer + SIZE_ETHERNET);
			
			switch(ip->ip_p)
			{
				case 1: 
					icmp++; 
					break;
				case 6: 
					tcp++; 
					break;
				case 17:
					udp++; 
					break;
				default:
					other++;
					break;
			}
			
			char str[256];
			sprintf(str, "ICMP: %-8d TCP: %-8d UDP: %-8d OTHER: %-8d ", icmp, tcp, udp, other);
			sprintf(str + strlen(str), "src: %-16s ", inet_ntoa(ip->ip_src));
			sprintf(str + strlen(str), "dst: %-16s", inet_ntoa(ip->ip_dst));
			printf("%s\r", str);
			fflush(stdout);
			
			//if(ip->ip_p == 1)
			//{
			//	char str[256];
			//	sprintf(str, "Jacked an ICMP packet from: %-16s to: ", inet_ntoa(ip->ip_src));
			//	sprintf(str + strlen(str), "%-16s\n", inet_ntoa(ip->ip_dst));
			//	fprintf(stdout, "%s", str);
			//}
		}
		else
		{
			printf("\nDisconnected from server.\n");
			break;
		}
    }
	
	return 0;
}