#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <string.h>

#include "sockwrap.h"

int get_portno(int socketfd)
{
	int portno = 0;
	
	struct sockaddr_in serv_addr;
	socklen_t len = sizeof (serv_addr);
	if (getsockname(socketfd, (struct sockaddr *) &serv_addr, &len) == -1)
	{
		fprintf(stderr, "Unable to get details for opened socket.");
	}
	else
	{
		portno = ntohs(serv_addr.sin_port);
		fprintf(stdout, "Socketfd %-9d listening on port: %d\n", socketfd, portno);
	}
	
	return portno;
}

int open_listening_socket(int *portno)
{
	struct sockaddr_in serv_addr;
	bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(*portno);
	
	int socketfd = socket(AF_INET, SOCK_STREAM, 0); //ipv4 only; TCP
	if (socketfd < 0) 
	{
		fprintf(stderr, "Unable to open listening socket. ");
		return -1;
	}
	
	if (bind(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	{
		fprintf(stderr, "Unable to bind to socket.");
		close(socketfd);
		return -1;
	}
	
	listen(socketfd, 5);  // Max 5 simultaneous ???
	
	*portno = get_portno(socketfd);

	return socketfd;
}

int connect_to_host(char *hostname, int portno)
{
	struct hostent *server;
	struct sockaddr_in serv_addr;

	server = gethostbyname(hostname);
	if (server == NULL)
	{
		fprintf(stderr, "No host found under the name: %s\n", hostname);
		return -1;
	}
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
	
	int socketfd = socket(AF_INET, SOCK_STREAM, 0); //ipv4 only; TCP
	if (socketfd < 0) 
	{
		fprintf(stderr, "Unable to open socket to connect to: %s:%d\n", hostname, portno);
		return -1;
	}
		
	fprintf(stdout, "Connecting to %s:%d .... ", hostname, portno);
	
	if (connect(socketfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
		fprintf(stderr, "Unable to connect to %s:%d\n", hostname, portno);
		close(socketfd);
		return -1;
	}
		
	fprintf(stdout, "connected\n");
	
	return socketfd;
}