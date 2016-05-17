#include "config.h"

struct metric{
	char* name;
	double count;
};

struct report{
	char *module_name;
	time_t timestamp;
	struct metric *metrics;
	int metric_count;
};

struct stats{
	int accumulated_time;
	int idx;
	int period_len_in_s;
	double *accumulated_aggregates;
	struct report reports[REPORTS_TO_SAVE];
};

typedef struct subscriber{
	int socketid;
	char ip_addr[16];
	struct subscriber *next;
} subscriber;

typedef struct subscriber_list{
	char *interface;
	int count;
	struct subscriber *first;
} subscriber_list;