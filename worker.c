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
#include "utilities.h"
//#include "aggregator.h"

#include <dlfcn.h>

/*
#include <pthread.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
*/


char ** download_modules(int client_socket)
{
	char buffer[BUFSIZ];
	char **moduleNames = malloc(sizeof(char*));
	
	
	// gets module count
	int32_t module_count_netorder;
	read(client_socket, &module_count_netorder, sizeof(module_count_netorder));
	int module_count = ntohl(module_count_netorder);
	
	int i;
	for( i = 0; i < module_count; i++)
	{
		// gets the file size
		int32_t file_len_netorder;
		read(client_socket, &file_len_netorder, sizeof(file_len_netorder));
		long file_size = ntohl(file_len_netorder);
		
		// gets the file name
		read(client_socket, buffer, 256);
		moduleNames[i] = malloc(strlen("modules/") + strlen(buffer) + 1);
		strcpy(moduleNames[i], "modules/");
		strcat(moduleNames[i], buffer);
		
		// creates a file to write to
		FILE *received_file = fopen(moduleNames[i], "w");
		
		// writes to the file
		int remain_data = file_size;
		int len;
		while ((remain_data > 0) && ((len = recv(client_socket, buffer, BUFSIZ, 0)) > 0))
		{
			fwrite(buffer, sizeof(char), len, received_file);
			remain_data -= len;
		}
		
		printf("%s of size %.2f KB downloaded.\n", moduleNames[i], file_size / 1024.0);
		
		// closes the file
		fclose(received_file);
	}
	
	return moduleNames;
}

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
		
		printf("%s loaded.\n", moduleNames[count]);
		count++;
	}
	
	return count;
}

void send_to_aggregator(int socketfd, struct report report)
{
	//TODO: send over TCP
	
	//aggregate(report);
}

void* reporting_start(void* args)
{
	struct WorkerInfo *workerInfo = (struct WorkerInfo *) args;
	struct ModuleInterface *modules = (struct ModuleInterface *) workerInfo->modules;
	
	//TODO: negotiate on TCP instead
	//initialize_aggregator(modules[0].get_module_name(), modules[0].get_metric_count());
	
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
		
		send_to_aggregator(workerInfo->reporting_socketfd, report);
		
		free(report.metrics);
		
		report.start = report.end;		
	}
}

int main(int argc, char *argv[])
{
	struct WorkerInfo *workerInfo = malloc ( sizeof ( struct WorkerInfo ) );
	workerInfo->device = strdup(get_device_name(argc, argv));
	
	int client_socket = connect_to_host(argv[2], PUBLISHER_PORT);
	workerInfo->reporting_socketfd = client_socket;
	char ** modules = download_modules(client_socket);
	workerInfo->moduleCount = load_modules(&(workerInfo->modules), modules);
	
	pthread_t snifferThread;
	start_thread("sniffer", &snifferThread, &sniffer_start, (void *) workerInfo);
	
	pthread_t reportingThread;
	start_thread("reporting", &reportingThread, &reporting_start, (void *) workerInfo);		// connects to aggregator
    
	while(1)
	{
	}
}