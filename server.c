#include <stdio.h>
#include <stdlib.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>


#include "sniffwrap.h"
#include "data_models.h"
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


subscriber_list *subscribers; 

volatile short program_abort = 0;

void* publish(void* args)
{
	int portno = PUBLISHER_PORT;
	int sockfd = open_listening_socket(&portno);
	fprintf(stdout, "Sniffer service published to subscribers on port %d\n", portno);
	if (sockfd == -1)
		exit(EXIT_FAILURE);
	
	int connsockfd;
	while(!program_abort) 
	{		
		struct sockaddr_in cli_addr;
	    socklen_t clilen = sizeof(cli_addr);
    
		connsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // how to interrupt this?
		
		subscriber *client = malloc(sizeof(subscriber));
		client->socketid = connsockfd;
		client->next = subscribers->first;
		inet_ntop(AF_INET, &(cli_addr.sin_addr), client->ip_addr, INET_ADDRSTRLEN ); // only ipv4 address
		
		fprintf(stdout, "%s connected (conn sock fd %d)\n", client->ip_addr, client->socketid);
		
		subscribers->first = client; 				//thread unsafe
		subscribers->count++;						//thread unsafe
	}
	
	subscriber *tmp;
	for( tmp = subscribers->first ; tmp != NULL ; tmp = tmp->next ) 
		close(tmp->socketid);
	
	close(sockfd);
}

int main(int argc, char *argv[])
{
	subscribers = malloc(sizeof(subscribers));
	subscribers->interface = strdup(get_device_name(argc, argv));
	
	pthread_t snifferThread;
	int err = pthread_create(&snifferThread, NULL, &sniffer_start, (void *)subscribers);
	if(err != 0)
		fprintf(stderr, "Failed to create background sniffer thread. Reason:[%s]", strerror(err));
	else
		fprintf(stdout, "Sniffer started successfully!\n");
	
	pthread_t publishThread;
	err = pthread_create(&publishThread, NULL, &publish, NULL);
	// TODO: error reporting
	
	
	
	char str[256];
	int i = 0;
	do{
		fgets(str, 255, stdin);

		while(str[i]) { str[i] = tolower(str[i]); i++; } // converts string to all lower case
	}while(
		strncmp(str, COMMAND_EXIT, strlen(COMMAND_EXIT)) != 0 &&
		strncmp(str, COMMAND_QUIT, strlen(COMMAND_QUIT)) != 0);
		
	program_abort = 1;
	pthread_join(publishThread, NULL);

	free(subscribers);
}