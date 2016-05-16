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