#include <stdio.h>
#include <stdlib.h>

#include "sockwrap.h"
#include "config.h"

int main(int argc, char *argv[])
{
    int socketfd;
	
    if (argc < 2) {
       fprintf(stderr, "usage %s hostname\n", argv[0]);
       exit(EXIT_FAILURE);
    }
	
	char *hostname = argv[1];
	socketfd = connect_to_host(hostname, PUBLISHER_PORT);
	
	char buffer[256];
	while( read(socketfd,buffer,255) > 0) 
	{
		printf("%s", buffer);
	}
	
	printf("Disconnected due to server...\n");
    
	return 0;
}