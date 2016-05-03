#include "icmp.h"
 
//#define SNAP_LEN 				1518 	/* default snap length (maximum bytes per packet to capture) */
#define SIZE_ETHERNET 			14 		/* ethernet headers are always exactly 14 bytes [1] */


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

#define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)

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
	
	//Subscribers *subscriber_list = (Subscribers *) args;
	
	if(ip->ip_p == 1)
	{
		char str[256];
		strcpy(str, "Jacked an ICMP packet from: ");
		strcat(str, inet_ntoa(ip->ip_src));
		strcat(" to ", inet_ntoa(ip->ip_dst));
		
		printf("%s\n", str);
		
		//for(j = 0; j < subscriber_list->current_count; j++)
		//{
		//	write(subscriber_list->socketids[j], str, 255);
		//}
	}
}
