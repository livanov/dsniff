#include <stdio.h>
#include <stdlib.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>


#include "sniffwrap.h"
#include "data_models.h"
#include "sockwrap.h"
#include "config.h"
//#include "aggregator.h"

#include <dlfcn.h>

/*
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
*/



short load_modules(struct ModuleInterface **modules, char **moduleNames)
{
	int moduleNamesCount = sizeof(moduleNames) / sizeof(moduleNames[0]);
	*modules = malloc( sizeof(struct ModuleInterface) * moduleNamesCount);
	
	int i, count = 0;
	for( i = 0; i < moduleNamesCount ; i++)
	{
		void *handle = dlopen ( moduleNames[i], RTLD_NOW );
		
		modules[count]->flush = dlsym ( handle, "flush_data" );
		if(modules[count]->flush == 0)
		{
			printf("Function flush_data was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->get_metric_count = dlsym ( handle, "get_metric_count" );
		if(modules[count]->get_metric_count == 0)
		{
			printf("Function get_metric_count was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->get_module_name = dlsym ( handle, "get_module_name" );
		if(modules[count]->get_module_name == 0)
		{
			printf("Function get_module_name was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->aggregate_data = dlsym ( handle, "aggregate_data" );
		if(modules[count]->aggregate_data == 0)
		{
			printf("Function aggregate_data was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->got_packet = dlsym ( handle, "got_packet" );
		if(modules[count]->got_packet == 0)
		{
			printf("Function got_packet was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		count++;
	}
	
	return count;
}

void send_to_aggregator(struct report report)
{
	//TODO: send over TCP
	
	aggregate(report);
}

void* reporting_start(void* args)
{
	struct ModuleInterface *modules = (struct ModuleInterface *) args;
	
	//TODO: negotiate on TCP instead
	initialize_aggregator(modules[0].get_module_name(), modules[0].get_metric_count());
	
	struct report report;
	report.start = time(NULL);
	
	while(1)
	{
		sleep( 1 );
			
			//pthread_mutex_lock(&lock);
			
			//TODO: traverse all modules
			report.metrics = modules[0].aggregate_data();
				
			modules[0].flush();
			report.end = time(NULL);
    
			//pthread_mutex_unlock(&lock);
		
		send_to_aggregator(report);
		
		free(report.metrics);
		
		report.start = report.end;		
	}
}

void start_thread(char *threadName, pthread_t *thread, void *start_routine, void *arg)
{
	int err = pthread_create(thread, NULL, start_routine, arg);
	if(err != 0)
		fprintf(stderr, "Failed to create background %s thread. Reason:[%s]\n", threadName, strerror(err));
	else
		fprintf(stdout, "%s thread started successfully!\n", threadName);
}

int main(int argc, char *argv[])
{
	char *moduleNames[] = { "modules/icmpcount.so" };
	
	struct SnifferInfo *sniffer_info = malloc ( sizeof ( struct SnifferInfo ) );
	sniffer_info->moduleCount = load_modules(&(sniffer_info->modules), moduleNames);
	sniffer_info->device = strdup(get_device_name(argc, argv));
	 
	pthread_t snifferThread;
	start_thread("sniffer", &snifferThread, &sniffer_start, (void *) sniffer_info);
	
	pthread_t reportingThread;
	start_thread("reporting", &reportingThread, &reporting_start, (void *) sniffer_info->modules);		// connects to aggregator
    
	while(1){}
}