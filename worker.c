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
//#include "hashtable.h"

#include <dlfcn.h>

/*
#include <pthread.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
*/


struct WorkerInfo *workerInfo;
pthread_mutex_t lock;

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
		
		modules[count]->report_data = dlsym ( handle, "report_data" );
		if(modules[count]->report_data == 0)
		{
			printf("Function report_data was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->got_packet = dlsym ( handle, "got_packet" );
		if(modules[count]->got_packet == 0)
		{
			printf("Function got_packet was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->create_persistent_object = dlsym ( handle, "create_persistent_object" );
		if(modules[count]->create_persistent_object == 0)
		{
			printf("Function create_persistent_object was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->free_persistent_object = dlsym ( handle, "free_persistent_object" );
		if(modules[count]->free_persistent_object == 0)
		{
			printf("Function free_persistent_object was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->get_delay_definition = dlsym ( handle, "get_delay_definition" );
		if(modules[count]->get_delay_definition == 0)
		{
			printf("Function get_delay_definition was not found in module: %s. MODULE WILL NOT BE LOADED.\n", moduleNames[i]);
			continue;
		}
		
		modules[count]->persistentObjects = ht_create( MAX_UID_COUNT );		
		
		printf("%s loaded.\n", moduleNames[count]);
		count++;
	}
	
	return count;
}

int send_module_report(int socketfd, struct module_report *moduleReport)
{
	struct report_list *item;
	item = moduleReport->list;
	
	short len = strlen( moduleReport->moduleName );
	uint16_t netLen = htons( len );
	// send module name len
	if ( ! write( workerInfo->reporting_socketfd, &netLen, sizeof ( uint16_t ) ) )
		return 0;
	// send module name
	if ( ! write( workerInfo->reporting_socketfd, moduleReport->moduleName, len ) )
		return 0;
	
	uint16_t metricCount = htons( moduleReport->metricCount );
	// send metric count
	if ( ! write( workerInfo->reporting_socketfd, &metricCount, sizeof ( uint16_t ) ) )
		return 0;
	
	uint16_t count = htons( moduleReport->count );
	// send report count
	if ( ! write( workerInfo->reporting_socketfd, &count, sizeof ( uint16_t ) ) ) 
		return 0;
	// send module report start
	if ( ! write( workerInfo->reporting_socketfd, &( moduleReport->start ), sizeof ( time_t ) ) )
		return 0;
	// send module report end
	if ( ! write( workerInfo->reporting_socketfd, &( moduleReport->end ), sizeof ( time_t ) ) )
		return 0;
	
	// send report items 
	short i, j;
	for ( i = 0 ; i < moduleReport->count ; i++ )
	{
		size_t uidLen = strlen( item->report->uid );
		
		// send UID len and UID
		if ( ! write( workerInfo->reporting_socketfd, &uidLen, sizeof ( size_t ) ) ) 
			return 0;
		if ( ! write( workerInfo->reporting_socketfd, item->report->uid, uidLen ) )
			return 0;
			
		for ( j = 0 ; j < moduleReport->metricCount ; j++ )
		{
			if ( ! write( workerInfo->reporting_socketfd, &( item->report->metrics[j] ), sizeof ( struct metric ) ) )
				return 0;
		}
			
		item = item->next;
	}
	
	return 1;
}

void * start_reporting_thread(void* args)
{
	struct ModuleInterface *module = (struct ModuleInterface *) args;
	int delay = module->get_delay_definition();
	char *moduleName = module->get_module_name();
	
	struct module_report *moduleReport = malloc(sizeof(struct module_report));
	moduleReport->start = time(NULL);
	moduleReport->metricCount = module->get_metric_count();
	moduleReport->moduleName = malloc( strlen ( moduleName ) );
	strcpy( moduleReport->moduleName, moduleName );
	
	short disconnected = 0;
	while( !disconnected )
	{
		sleep(delay);
		
		pthread_mutex_lock(&lock);
			struct hashtable *oldObjects = module->persistentObjects;
			module->persistentObjects = ht_create( MAX_UID_COUNT );
		pthread_mutex_unlock(&lock);
		
		moduleReport->end = time(NULL);
		moduleReport->list = NULL;
        
		char **keys = ht_get_keys( oldObjects );
		moduleReport->count = ht_get_count( oldObjects );
			
		struct report_list *listMember, *tmp;
		
		short i;
		for ( i = 0 ; i < moduleReport->count ; i++ )
		{
			void *persistentObject = ht_get( oldObjects, keys[i] );
			
			struct report *report = malloc( sizeof( struct report ) );
			report->uid = malloc ( strlen ( keys[i] ) );
			strcpy( report->uid, keys[i] );
			report->metrics = module->report_data( persistentObject );
			
			listMember = malloc( sizeof( struct report_list ) );
			listMember->report = report;
			
			//if (i==1) printf("%f %f %f %f\n", 
			//	report->metrics[0].value,
			//	report->metrics[1].value,
			//	report->metrics[2].value,
			//	report->metrics[3].value
			//	);
			
			listMember->next = moduleReport->list;
			moduleReport->list = listMember;
			
			module->free_persistent_object( persistentObject );
		}
		
		ht_free( oldObjects );
		
		if ( ! send_module_report( workerInfo->reporting_socketfd, moduleReport ) )
			disconnected = 1;
		
		moduleReport->start = moduleReport->end;
		
		//free resources		
		listMember = moduleReport->list;
		while( listMember != NULL )
		{
			tmp = listMember;
			listMember = listMember->next;
			free(tmp->report->metrics);
			free(tmp->report->uid);
			free(tmp->report);
			free(tmp);
		}
	}
	
	printf("\nDisconnected!\n");
}

void * reporting_start(void* args)
{
	short threadCount;
	pthread_t reportingThreads[256];
	struct ModuleInterface *modules = (struct ModuleInterface *) workerInfo->modules;
	
	short i;
	for( i = 0 ; i < workerInfo->moduleCount ; i++ )
	{
		char threadName[256];
		sprintf(threadName, "reporting for %s", modules[i].get_module_name());
		start_thread(threadName, &reportingThreads[threadCount++], &start_reporting_thread, (void *) &modules[i]);
	}
}

int main(int argc, char *argv[])
{
	workerInfo = malloc ( sizeof ( struct WorkerInfo ) );
	workerInfo->device = strdup(get_device_name(argc, argv));
	
	int client_socket = connect_to_host(argv[2], PUBLISHER_PORT);
	if(client_socket == -1) return;
	workerInfo->reporting_socketfd = client_socket;
	char ** modules = download_modules(client_socket);
	workerInfo->moduleCount = load_modules(&(workerInfo->modules), modules);
	
	pthread_t snifferThread;
	start_thread("sniffer", &snifferThread, &sniffer_start, (void *)workerInfo);
	
	reporting_start(workerInfo);
    
	while(1)
	{
	}
}