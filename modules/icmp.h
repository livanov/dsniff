#include <pcap/pcap.h>
#include <arpa/inet.h>
 
void got_ip_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);