#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "icmpcount.h"
#include "../config.h"


#define SIZE_ETHERNET 14 /* CONSTANT - should not be modified - should not be here */
struct PacketCount{
	int icmp;
	int tcp;
	int udp;
	int other;
};

//static int icmp = 0, tcp = 0, udp = 0, other = 0;
const short METRIC_COUNT = 4;
const char MODULE_NAME[] = "protocount";

char* get_module_name()
{
	return (char *)MODULE_NAME;
}

void got_packet(const char *packetBytes, int packetLen, void *persistentObject)
{
	struct PacketCount *obj = (struct PacketCount *) persistentObject;
	
	const struct ip *packet = (struct ip*)(packetBytes + SIZE_ETHERNET);
	
	switch(packet->ip_p)
	{
		case 1: 
			obj->icmp++; 
			break;
		case 6: 
			obj->tcp++; 
			break;
		case 17:
			obj->udp++; 
			break;
		default:
			obj->other++;
			break;
	}
	
	//char str[256];
	//sprintf(str, "ICMP: %-8d TCP: %-8d UDP: %-8d OTHER: %-8d ", obj->icmp, obj->tcp, obj->udp, obj->other);
	//sprintf(str + strlen(str), "src: %-16s ", inet_ntoa(packet->ip_src));
	//sprintf(str + strlen(str), "dst: %-16s", inet_ntoa(packet->ip_dst));
	//printf("%s\r", str);
	//fflush(stdout);
}

struct metric* aggregate_data(void *persistentObject)
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

//void flush_data(void *persistentObject)
//{
//}

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