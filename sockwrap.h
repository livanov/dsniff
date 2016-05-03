// returns the port number of the source of a socket
int get_portno(int socketfd);

// opens a socket and starts listening on it
// if portno supplied - tries to open on the specified portno
// if not 			 - random free portno is assigned
int open_listening_socket(int *portno);

// attempts socket connection to a host name
int connect_to_host(char *hostname, int portno);