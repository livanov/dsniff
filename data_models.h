#include "config.h"
#include "hashtable.h"

struct metric{
	//char* name;
	double value;
};

struct report{
	time_t 			start;
	time_t 			end;
	char			*uid;
	struct metric 	*metrics;
};

struct report_list{
	struct report 		*report;
	struct report_list 	*next;
};

struct module_report{
	time_t				start;
	time_t				end;
	short				count;
	struct report_list 	*list;
	char 				*moduleName;
	short				metricCount;
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
	struct normal_distribution 	*normal_distribution;
	struct aggregate 			*next;
};

struct stats{
	//char 				*userId;
	long 				count;
	short 				metricCount;
	short 				accumulated_time;
	double 				residual;
	struct report_list 	*reports;
	struct aggregate 	*history;
};

struct ModuleInterface{
	struct hashtable 							*persistentObjects;
	
	int 			(*get_metric_count) 		();
	int				(*get_delay_definition)		();
	char* 			(*get_module_name) 			();
	void 			(*got_packet) 				(const char *, int, void *);
	struct metric* 	(*report_data)				(void *);
	void*			(*create_persistent_object) ();
	void 			(*free_persistent_object) 	(void *);
};

struct WorkerInfo{
	struct 	ModuleInterface *modules;
	short 					moduleCount;
	int 					reporting_socketfd;
	char 					*device;
};
