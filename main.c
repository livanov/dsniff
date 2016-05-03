#include <stdio.h>
#include <stdlib.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>


#include "sniffwrap.h"
#include "subscribers.h"
#include "sockwrap.h"
#include "config.h"

/*
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
*/


subscribers *subscriber_list; 


int main(int argc, char *argv[])
{
	int portno = PUBLISHER_PORT;
	subscriber_list = malloc(sizeof(subscribers));
	subscriber_list->interface = strdup(get_device_name(argc, argv));
	
	pthread_t workerId;
	int err = pthread_create(&workerId, NULL, &sniffer_start, (void *)subscriber_list);
	if(err != 0)
		printf("Failed to create background sniffer thread. Reason:[%s]", strerror(err));
	else
		printf("Sniffer started successfully!\n");
	
	
	
	
	fprintf(stdout, "Publisher opening to subscribers ... ");
	int sockfd = open_listening_socket(&portno);
	if (sockfd == -1)
		exit(EXIT_FAILURE);
	

	
	while(subscriber_list->current_count < 10) 
	{		
		struct sockaddr_in cli_addr;
	    socklen_t clilen = sizeof(cli_addr);
    
		int connsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		
		subscriber_list->socketids[subscriber_list->current_count] = connsockfd;
		subscriber_list->current_count++;
	}
	
	int i;
	for(i=0;i<subscriber_list->current_count;i++) close(subscriber_list->socketids[i]);
	close(sockfd);
	
	free(subscriber_list);
}