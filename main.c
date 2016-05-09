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

volatile short program_abort = 0;

void* publish(void* args)
{
	int portno = PUBLISHER_PORT;
	fprintf(stdout, "Publisher opening to subscribers ... ");
	int sockfd = open_listening_socket(&portno);
	if (sockfd == -1)
		exit(EXIT_FAILURE);
	

	
	while(subscriber_list->current_count < 10 && !program_abort) 
	{		
		struct sockaddr_in cli_addr;
	    socklen_t clilen = sizeof(cli_addr);
    
		int connsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // how to interrupt this?
		
		subscriber_list->socketids[subscriber_list->current_count] = connsockfd;
		subscriber_list->current_count++;
	}
	
	int i;
	for(i=0;i<subscriber_list->current_count;i++) close(subscriber_list->socketids[i]);
	
	close(sockfd);
}

int main(int argc, char *argv[])
{
	subscriber_list = malloc(sizeof(subscribers));
	subscriber_list->interface = strdup(get_device_name(argc, argv));
	
	pthread_t snifferId;
	int err = pthread_create(&snifferId, NULL, &sniffer_start, (void *)subscriber_list);
	if(err != 0)
		printf("Failed to create background sniffer thread. Reason:[%s]", strerror(err));
	else
		fprintf(stdout, "Sniffer started successfully!\n");
	
	pthread_t publishId;
	err = pthread_create(&publishId, NULL, &publish, NULL);
	
	
	
	
	char str[256];
	int i = 0;
	do{
		fgets(str, 255, stdin);

		while(str[i]) { str[i] = tolower(str[i]); i++; } // converts string to upper
	}while(
		strncmp(str, COMMAND_EXIT, strlen(COMMAND_EXIT)) != 0 &&
		strncmp(str, COMMAND_QUIT, strlen(COMMAND_QUIT)) != 0);
		
	program_abort = 1;
	pthread_join(publishId, NULL);

	free(subscriber_list);
}