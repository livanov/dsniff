#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "icmpcount.h"
#include "../config.h"


const short METRIC_COUNT = 4;
const short DELAY_DEFINITION = 1;
const char 	MODULE_NAME[] = "protocount";

char* get_module_name()
{
	return (char *)MODULE_NAME;
}

int get_delay_definition()
{
	return DELAY_DEFINITION;
}

struct PacketCount{
	int icmp;
	int tcp;
	int udp;
	int other;
};

void got_packet(const char *packetBytes, int packetLen, void *persistentObject)
{
	struct PacketCount *obj = (struct PacketCount *) persistentObject;
	const struct ip *packet = (struct ip*)(packetBytes + SIZE_ETHERNET);
	
	switch(packet->ip_p)
	{
		case 1:  obj->icmp++;  break;
		case 6:  obj->tcp++;   break;
		case 17: obj->udp++;   break;
		default: obj->other++; break;
	}
}

struct metric* report_data(void *persistentObject)
{	
	struct metric *metrics = malloc(METRIC_COUNT * sizeof(struct metric));
	
	struct PacketCount *obj = (struct PacketCount *) persistentObject;

	//metrics[0].name = malloc(MAX_METRIC_NAME_LEN);
	//strcpy(metrics[0].name, "icmp count");
	metrics[0].value = obj->icmp;
		
	//metrics[1].name = malloc(MAX_METRIC_NAME_LEN);
	//strcpy(metrics[1].name, "tcp count");
	metrics[1].value = obj->tcp;
	
	//metrics[2].name = malloc(MAX_METRIC_NAME_LEN);
	//strcpy(metrics[2].name, "udp count");
	metrics[2].value = obj->udp;
	
	//metrics[3].name = malloc(MAX_METRIC_NAME_LEN);
	//strcpy(metrics[3].name, "others count");
	metrics[3].value = obj->other;
	
	return metrics;
}

short get_metric_count()
{
	return METRIC_COUNT;
}

void* create_persistent_object()
{
	struct PacketCount *obj = malloc(sizeof(struct PacketCount));
	
	obj->icmp = 0;
	obj->tcp = 0;
	obj->udp = 0;
	obj->other = 0;
	
	return (void *) obj;
}

void free_persistent_object(void *persistentObject)
{
	struct PacketCount *obj = (struct PacketCount *) persistentObject;

	free(obj);
}