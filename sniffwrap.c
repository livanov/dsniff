#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "sniffwrap.h"
#include "data_models.h"
#include "config.h"


//#include <dlfcn.h>
/*
#include <unistd.h>
*/

//typedef void (*got_packet_f) (const char *, int );

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
	//const struct sniff_ethernet *ethernet = (struct sniff_ethernet*)(packet);
	//const struct sniff_ip *ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
	//int size_ip = IP_HL(ip)*4;
	//if (size_ip < 20) {
	//	printf("   * Invalid IP header length: %u bytes\n", size_ip);
	//	return;
	//}
	struct WorkerInfo *workerInfo = (struct WorkerInfo *) args;
	short i;
	for( i = 0 ; i < workerInfo->moduleCount; i++ )
	{
		workerInfo->modules[i].got_packet(packet, header->len);
	}
}

char* get_device_name(int argc, char *argv[])
{
	char *dev;						/* The device to sniff on */
	//bpf_u_int32 mask;				/* Our netmask */
	//bpf_u_int32 ip;				/* Our IP */
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
	struct WorkerInfo *workerInfo = (struct WorkerInfo *) args;
	char *device = workerInfo->device;
	pcap_t *handle = get_handle(device);
	
	/* set filter for ip packets only */
	char filter_exp[] = "ip";
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
	
	
	pcap_loop(handle, NUM_PACKETS_BFR_EXIT, got_packet, (u_char *) workerInfo);
	
	/* And close the session */
	pcap_close(handle);
}