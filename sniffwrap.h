#include <pcap.h>

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);

pcap_t* get_handle(char *device);

char* get_device_name(int argc, char *argv[]);

void* sniffer_start(void* args);
