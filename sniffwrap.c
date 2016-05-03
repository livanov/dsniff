#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "sniffwrap.h"
#include "subscribers.h"
#include "config.h"

/*
#include <dlfcn.h>
#include <unistd.h>
*/



#define SIZE_ETHERNET 			14 		/* ethernet headers are always exactly 14 bytes [1] */
#define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)


/* IP header */
struct sniff_ip {
        u_char  ip_vhl;                 /* version << 4 | header length >> 2 */
        u_char  ip_tos;                 /* type of service */
        u_short ip_len;                 /* total length */
        u_short ip_id;                  /* identification */
        u_short ip_off;                 /* fragment offset field */
        #define IP_RF 0x8000            /* reserved fragment flag */
        #define IP_DF 0x4000            /* dont fragment flag */
        #define IP_MF 0x2000            /* more fragments flag */
        #define IP_OFFMASK 0x1fff       /* mask for fragmenting bits */
        u_char  ip_ttl;                 /* time to live */
        u_char  ip_p;                   /* protocol */
        u_short ip_sum;                 /* checksum */
        struct  in_addr ip_src,ip_dst;  /* source and dest address */
};


void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
	char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
	int j;
	
	const struct sniff_ethernet *ethernet = (struct sniff_ethernet*)(packet);
	const struct sniff_ip *ip = (struct sniff_ip*)(packet + SIZE_ETHERNET);
	int size_ip = IP_HL(ip)*4;
	if (size_ip < 20) {
		printf("   * Invalid IP header length: %u bytes\n", size_ip);
		return;
	}
	
	subscribers *subscriber_list = (subscribers *) args;
	
	if(ip->ip_p == 1)
	{
		char str[256];
		sprintf(str, "Jacked an ICMP packet from: %-16s to: ", inet_ntoa(ip->ip_src));
		sprintf(str + strlen(str), "%-16s\n", inet_ntoa(ip->ip_dst));
		
		//printf("%s", str);
		
		for(j = 0; j < subscriber_list->current_count; j++)
		{
			write(subscriber_list->socketids[j], str, 255);
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
	subscribers *subscriber_list = (subscribers *) args;
	char *dev = subscriber_list->interface;
	pcap_t *handle = get_handle(dev);
	
	/* set filter for ip packets only */
	
	char filter_exp[] = "ip";		/* filter expression [3] */
	struct bpf_program fp;			/* compiled filter program (expression) */
	
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
	
    
	//int i; for(i=0;i<10;i++) printf("%d ", subscriber_list->socketids[i]); printf("from sniffer_start\n");
	
	pcap_loop(handle, NUM_PACKETS_BFR_EXIT, got_packet, (u_char *) subscriber_list);
	
	/* And close the session */
	pcap_close(handle);
}