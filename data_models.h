#include "config.h"

struct metric{
	//char* name;
	double value;
};

struct report{
	time_t 			start;
	time_t 			end;
	struct metric 	*metrics;
};

struct report_list{
	struct report 		*report;
	struct report_list 	*next;
};

struct normal_distribution{
	double 	std;
	double 	mean;
	long 	count;
};

struct aggregate{
	//int length; // int?
	time_t 						start;
	time_t 						end;
	struct normal_distribution 	normal_distribution;
	struct aggregate 			*next;
};

struct stats{
	char 				*module_name;
	long 				count;
	short 				metric_count;
	short 				accumulated_time;
	double 				residual;
	struct report_list 	*reports;
	struct aggregate 	*history;
};


struct ModuleInterface{
	void 			(*flush) 				();
	int 			(*get_metric_count) 	();
	char* 			(*get_module_name) 		();
	void 			(*got_packet) 			(const char *, int );
	struct metric* 	(*aggregate_data)		();
};

struct WorkerInfo{
	struct 	ModuleInterface *modules;
	short 					moduleCount;
	int 					reporting_socketfd;
	char 					*device;
};
