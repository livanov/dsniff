#include "../data_models.h"

void got_packet(const char *packetBytes, int packetLen);
struct metric* aggregate_data();

char* get_module_name();
short get_metric_count();
void flush_data();