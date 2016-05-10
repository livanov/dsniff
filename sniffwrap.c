#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "sniffwrap.h"
#include "subscribers.h"
#include "config.h"

/*
#include <dlfcn.h>
#include <unistd.h>
*/

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
	//const struct sniff_ethernet *ethernet = (struct sniff_ethernet*)(packet);
	//const struct sniff_ip *ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
	//int size_ip = IP_HL(ip)*4;
	//if (size_ip < 20) {
	//	printf("   * Invalid IP header length: %u bytes\n", size_ip);
	//	return;
	//}

	uint16_t networkLen = htons(header->len);

	subscriber_list *subscribers = (subscriber_list *) args;
	subscriber *tmp, *prev = NULL;
	
	for( tmp = subscribers->first ; tmp != NULL ; tmp = tmp->next )
	{
		if (
			send(tmp->socketid, &networkLen, sizeof(networkLen), MSG_NOSIGNAL) == -1 || 
			send(tmp->socketid, packet, header->len, MSG_NOSIGNAL) == -1) // socket has been closed
		{
			close(tmp->socketid);
			subscribers->count--; 					//thread unsafe
			
			fprintf(stdout, "%s has been disconnected\n", tmp->ip_addr);
			
			if( prev == NULL ) // tmp is the first element in the linked list
			{
				subscribers->first = tmp->next; 	//thread unsafe
			}
			else 
			{ 
				prev->next = tmp->next;				//thread unsafe
			}
		}
		else // something has been transmitted, no guarantees it was the whole message
		{ 
			prev = tmp;
		}
	}
	
	//void *plugin;
	//char *func_name = "got_packet";
	//
	//typedef void (*got_packet_f) ();
    //got_packet_f got_packet;
	//
	//plugin = dlopen("modules/icmp.so", RTLD_NOW);
	//
	//got_packet = dlsym (plugin, func_name);
	//got_packet(args, header, packet);
	
	
	//plugin = dlopen("./tcp.so", RTLD_NOW);
	//got_packet = dlsym (plugin, func_name);
	//got_packet();
	//
	//got_ip_packet(args, header, packet);
}

char* get_device_name(int argc, char *argv[])
{
	char *dev;						/* The device to sniff on */
	//bpf_u_int32 mask;				/* Our netmask */
	//bpf_u_int32 ip;					/* Our IP */
	char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */

	// Set the device to sniff on
	if (argc > 1) {
		dev = argv[1];
	} 
	else {
		dev = pcap_lookupdev(errbuf);
		if (dev == NULL) {
			fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
			exit(EXIT_FAILURE);
		}
	}
		
	printf("Device sniffing on: %s\n", dev);	
	return dev;
}

pcap_t* get_handle(char *dev)
{
	char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
	pcap_t *handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
	
	if (handle == NULL) {
		fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
		exit(EXIT_FAILURE);
	}
	
	/* make sure we're capturing on an Ethernet device */
	if (pcap_datalink(handle) != DLT_EN10MB) {
		fprintf(stderr, "%s is not an Ethernet\n", dev);
		exit(EXIT_FAILURE);
	}
	
	return handle;
}


void* sniffer_start(void* args)
{
	subscriber_list *subscribers = (subscriber_list *) args;
	char *dev = subscribers->interface;
	pcap_t *handle = get_handle(dev);
	
	/* set filter for ip packets only */
	
	char filter_exp[] = "ip and not net 10.0.0.0/24";
	//sprintf(filter_exp + strlen(filter_exp), "10.0.0.0/24");	/* filter expression [3] */
	struct bpf_program fp;										/* compiled filter program (expression) */
	
	/* compile the filter expression */
	if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
		fprintf(stderr, "Couldn't parse filter %s: %s\n",
		    filter_exp, pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}

	/* apply the compiled filter */
	if (pcap_setfilter(handle, &fp) == -1) {
		fprintf(stderr, "Couldn't install filter %s: %s\n",
		    filter_exp, pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}
	
	/* set filter for ip packets only */
	
    
	
	pcap_loop(handle, NUM_PACKETS_BFR_EXIT, got_packet, (u_char *) subscribers);
	
	/* And close the session */
	pcap_close(handle);
}