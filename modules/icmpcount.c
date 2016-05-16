#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "icmpcount.h"
#include "../config.h"


#define SIZE_ETHERNET 14 /* CONSTANT - should not be modified - should not be here */

static int icmp = 0, tcp = 0, udp = 0, other = 0;
const int METRIC_COUNT = 4;
const char MODULE_NAME[] = "protocount";

char* get_module_name()
{
	return (char *)MODULE_NAME;
}

void got_packet(char *packetBytes, int packetLen)
{
	const struct ip *packet = (struct ip*)(packetBytes + SIZE_ETHERNET);
	
	switch(packet->ip_p)
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
	
	//char str[256];
	//sprintf(str, "ICMP: %-8d TCP: %-8d UDP: %-8d OTHER: %-8d ", icmp, tcp, udp, other);
	//sprintf(str + strlen(str), "src: %-16s ", inet_ntoa(packet->ip_src));
	//sprintf(str + strlen(str), "dst: %-16s", inet_ntoa(packet->ip_dst));
	//printf("%s\r", str);
	//fflush(stdout);
}

struct metric* aggregate_data()
{	
	struct metric *metrics = malloc(METRIC_COUNT * sizeof(struct metric));
	
	(metrics)[0].name = malloc(MAX_METRIC_NAME_LEN);
	strcpy((metrics)[0].name, "icmp count");
	(metrics)[0].count = icmp;
		
	(metrics)[1].name = malloc(MAX_METRIC_NAME_LEN);
	strcpy((metrics)[1].name, "tcp count");
	(metrics)[1].count = tcp;
	
	(metrics)[2].name = malloc(MAX_METRIC_NAME_LEN);
	strcpy((metrics)[2].name, "udp count");
	(metrics)[2].count = udp;
	
	(metrics)[3].name = malloc(MAX_METRIC_NAME_LEN);
	strcpy((metrics)[3].name, "others count");
	(metrics)[3].count = other;
	
	return metrics;
}

int get_metric_count()
{
	return METRIC_COUNT;
}

void flush_data()
{
	icmp = tcp = udp = other = 0;
}