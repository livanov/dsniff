#include "../data_models.h"

void got_packet(char *packetBytes, int packetLen);
struct metric* aggregate_data();

char* get_module_name();
int get_metric_count();
void flush_data();