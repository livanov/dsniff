#include "../data_models.h"

void got_packet(const char *packetBytes, int packetLength, void *persistentObject);
struct metric* aggregate_data(void *persistentObject);

char* get_module_name();
short get_metric_count();
void *create_persistent_object();
void free_persistent_object(void *persistentObject);
void flush_data(void *persistentObject);