#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <netinet/in.h>

#include "data_models.h"
#include "config.h"
#include "utilities.h"

struct hashtable *statsPerModule;
int peer_socket;

//struct ModuleStats		icmpModuleStats;
//struct stats 			stats;
int 					portno = PUBLISHER_PORT;
const char 				*moduleNames[] = { "src_modules/icmpcount.so" };

static volatile int 	keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

void print_distr(struct normal_distribution *distr)
{
	printf("\n---------------------------------------- STD  DEV ----------------------------------------\n");
	printf("     RESULTS:\tCount: [%8ld]\tMean:[%12.5f]\tDeviation: [%12.5f]\n", distr->count, distr->mean, distr->std);
	printf("---------------------------------------- STD  DEV ----------------------------------------\n\n");
}

struct normal_distribution get_std(double *array, long count)
{
	struct normal_distribution distr;
	distr.mean = 0;
	
	long i;
	
	//printf("\n----------------------------------------  VALUES  ----------------------------------------\n");
	//for(i = 0; i < count; i++)
	//{
	//	printf("[%12.3f]     ", array[i]);
	//	if(i % 5 == 4) printf("\n");
	//}
	
	for(i = 0; i < count; i++) 
		distr.mean += array[i];
	
	distr.mean /= count;
	
	double var = 0;
	for(i = 0; i < count; i++) 
	{
		double diff = array[i] - distr.mean;
		var += diff * diff;
	}
	
	distr.std = sqrt(var/count);
	distr.count = count;
	
	//print_distr(&distr);
	
	return distr;
}

struct normal_distribution combine_std(struct report_list *reports, struct aggregate *history, 
										struct normal_distribution distr, short metricIdx)
{
	int c = 0;
	struct normal_distribution leftover, combined;
	double ess = 0, gss = 0;
	
	leftover.count 	= reports->report->start - history->start;
	leftover.mean  	= history->normal_distribution[metricIdx].mean;
	ess 			= pow(history->normal_distribution[metricIdx].std, 2) * (leftover.count - 1);
	
	combined.count 	= leftover.count;
	combined.mean	= leftover.count * leftover.mean;
		
	combined.count 	+= distr.count;
	combined.mean  	+= distr.count * distr.mean;
	ess 			+= pow(distr.std, 2) * (distr.count - 1);
	
	struct aggregate *begin = history->next;
	for( history = begin; history != NULL; history = history->next )
	{
		combined.count 	+= history->normal_distribution[metricIdx].count;
		combined.mean  	+= history->normal_distribution[metricIdx].count * history->normal_distribution[metricIdx].mean;
		ess				+= pow(history->normal_distribution[metricIdx].std, 2) * history->normal_distribution[metricIdx].count;
		combined.std += history->normal_distribution[metricIdx].count * 
				( history->normal_distribution[metricIdx].std  * history->normal_distribution[metricIdx].std +
				  history->normal_distribution[metricIdx].mean * history->normal_distribution[metricIdx].mean );
	}
	
	combined.mean /= combined.count;
	
	gss += pow(leftover.mean - combined.mean, 2) * leftover.count;
	gss += pow(distr.mean - combined.mean, 2) * distr.count;
	
	for( history = begin; history != NULL; history = history->next )
	{
		gss += pow(history->normal_distribution[metricIdx].mean - combined.mean, 2) * history->normal_distribution[metricIdx].count;
	}

	combined.std = sqrt((ess + gss)/(combined.count - 1));
	
	//if(metricIdx == 2) print_distr(&combined);
	
	return combined;
}

void aggregate(struct report *report, struct stats *stats)
{
	//prepends new item in the report list
	struct report_list *item = malloc(sizeof(struct report_list));
	item->report = report;
	free(item->report->uid); // free uid as it is not needed. contained in the bucket key
	
	item->next = stats->reports;
	stats->reports = item;
	
	stats->accumulated_time += report->end - report->start;
	
	// TODO: compare received report values with thresholds 
	
	
	int BUFFER_TIME = 5;
	int BUFFER_SIZE = 7; // BUFFER_TIME should be < BUFFER_SIZE
	
	if(stats->accumulated_time >= BUFFER_TIME)
	{
		stats->accumulated_time -= BUFFER_TIME;
		
		static long time_passed = 0;
		long count = 0;
		struct report_list *report, *temp;
		short i, breakOuter = 0;
		double values[stats->metricCount][BUFFER_SIZE];
		memset(values, 0, sizeof(double) * stats->metricCount * BUFFER_SIZE);
	
		for(report = stats->reports ; report != NULL ; report = report->next) 
		{
			for ( i = 0 ; i < stats->metricCount; i++ )
				values[ i ][ count ] = report->report->metrics[ i ].value;
			
			count++;
			
			if(stats->history == NULL || report->report->start > stats->history->start)
			{
				int period_length = report->report->end - report->report->start;
				time_passed += period_length;
				
				if(time_passed >= BUFFER_TIME)
				{
					struct aggregate *aggregate = malloc(sizeof(struct aggregate));
					aggregate->normal_distribution = malloc ( stats->metricCount * sizeof ( struct normal_distribution ) );

					time_passed -= BUFFER_TIME;
					double frac = (1.0 * time_passed) / period_length;

					aggregate->start = time_passed + report->report->start;
					aggregate->end = BUFFER_TIME + aggregate->start;// - 1;
					
					short increaseCount = 0;
					for ( i = 0 ; i < stats->metricCount; i++ )
					{
						if(stats->residual != 0) 
						{
							values[ i ][ count ] = stats->residual;
							increaseCount = 1;
						}
						
						stats->residual = frac * values[ i ][ 0 ];
						values[ i ][ 0 ] = ( 1 - frac ) * values[ i ][ 0 ];
						
						aggregate->normal_distribution[i] = get_std(values[ i ], count + increaseCount);
					}
					
					count += increaseCount;
					
					aggregate->next = stats->history;
					stats->history = aggregate;
				}
			}
						
			if(count == BUFFER_SIZE)
				break;
		}
		
		struct normal_distribution distr[stats->metricCount];
		for ( i = 0 ; i < stats->metricCount ; i++ )
			distr[i] = get_std(values[i], count);
		
		if ( report != NULL ) // same as hitting breakOuter in iterator loop
		{
			struct aggregate *history = stats->history;
			
			for ( i = 0 ; i < stats->metricCount ; i++ )
			{
				while( history != NULL && history->start <= report->report->start)
					history = history->next;
						
				distr[i] = combine_std(report, history, distr[i], i);
			}
		}
		
		
		// TODO: update thresholds
		
		if ( report != NULL )
		{
			temp = report->next;
			report->next = NULL; 
			report = temp;
			
			while(report != NULL)
			{
				// TODO: dump old reports in database
				
				// deallocate old reports
				temp = report;
				report = report->next;
				free(temp->report->metrics);
				free(temp->report);
				free(temp);
			}
		}
	}
}

void initialize_aggregator(char *moduleName, int metricCount)
{
	//struct ModuleStats icmpModuleStats;
	//icmpModuleStats.moduleName = malloc(MAX_MODULE_NAME_LEN);
	//strcpy(icmpModuleStats.moduleName, moduleName);
	//icmpModuleStats.metricCount = metricCount;
	//
	//icmpModuleStats.statsPerId = ht_create( 65536 );
	//
	//statsPerModule = ht_create( 32 );
	//ht_set( statsPerModule, moduleName, (void *) &icmpModuleStats);

	
	
	//icmpModuleStats.statsForKey = NULL;
	
	//icmpModuleStats.stats.count = 0;
	//icmpModuleStats.stats.residual = 0;
	//icmpModuleStats.stats.accumulated_time = 0;
	//icmpModuleStats.stats.reports = malloc (sizeof(struct report_list));
	//icmpModuleStats.stats.reports = NULL;
	//icmpModuleStats.stats.history = malloc (sizeof(struct aggregate));
	//icmpModuleStats.stats.history = NULL;
	
	
	//// initialize stats
	//stats.count = 0;
	//stats.residual = 0;
	//stats.accumulated_time = 0;
	//stats.reports = malloc (sizeof(struct report_list));
	//stats.reports = NULL;
	//stats.history = malloc (sizeof(struct aggregate));
	//stats.history = NULL;
	//stats.module_name = malloc(MAX_MODULE_NAME_LEN);
	//strcpy(stats.module_name, module_name);
	//stats.metric_count = metric_count;
}

void free_aggregator_resources()
{
	//free(stats.module_name);
}

void send_module(int peer_socket, const char *moduleName)
{
	int fd = open(moduleName, O_RDONLY);
	struct stat file_stat;
	fstat(fd, &file_stat);
	
	// sending file length
	uint32_t file_size = htonl(file_stat.st_size);
	write(peer_socket, &file_size, sizeof(file_size));
	
	// sending file name
	// TODO: get it from moduleName
	write(peer_socket, "icmpcount.so", 256);
	
	// sending file
	off_t offset = 0;
	unsigned long remaining_bytes = file_size;
	unsigned long sent_bytes;
	
	while (((sent_bytes = sendfile(peer_socket, fd, &offset, BUFSIZ)) > 0) && (remaining_bytes > 0))
	{
		remaining_bytes -= sent_bytes;
	}
	
	close(fd);
}

void send_modules_to_worker(int peer_socket)
{
	int moduleNamesCount = sizeof(moduleNames) / sizeof(moduleNames[0]);
	
	// TODO: check if all modules are present before sending count
	
	// sending module count
	int32_t module_count = htonl(moduleNamesCount);
	write(peer_socket, &module_count, sizeof(module_count));
	
	int i;
	for( i = 0 ; i < moduleNamesCount ; i++)
		send_module(peer_socket, moduleNames[i]);
}

struct module_report * read_module_report(int peer_socket, short metricCount)
{
	struct module_report *moduleReport = malloc( sizeof ( struct module_report ) );
	uint16_t ret;
	char *buffer;
	moduleReport->list = NULL;
	
	// TODO: should read which module is that 
	
	// read count
	buffer = (char *) &ret;
	if ( ! read( peer_socket, buffer, sizeof ( uint16_t ) ) )
		return NULL;

	moduleReport->count = ntohs( ret );
		
	// read module report start
	buffer = (char *) &( moduleReport->start );
	if ( ! read( peer_socket, buffer, sizeof ( time_t ) ) )
		return NULL;

	// read module report end
	buffer = (char *) &( moduleReport->end );
	if ( ! read( peer_socket, buffer, sizeof ( time_t ) ) )
		return NULL;
	
	// read reports one by one
	short i, j;
	for( i = 0 ; i < moduleReport->count ; i++ )
	{
		size_t uidLen;
		
		// read UID length
		buffer = (char *) &uidLen;
		if ( ! read( peer_socket, buffer, sizeof ( size_t ) ) )
			return NULL;
		
		// read UID
		buffer = malloc ( uidLen );
		if ( ! read( peer_socket, buffer, uidLen ) )
			return NULL;
			
		struct report_list *item = malloc ( sizeof ( struct report_list ) );
		item->report = malloc ( sizeof ( struct report ) );
		item->report->uid = buffer;
		
		item->report->metrics = malloc ( metricCount * sizeof ( struct metric ) );
		
		//read metrics
		for( j = 0 ; j < metricCount ; j++ )
		{
			buffer = (char *) &( item->report->metrics[j] );
			if( ! read( peer_socket, buffer, sizeof ( struct metric ) ) )
				return NULL;
		}
		
		//printf("%-15s : ICMP %6f TCP %6f UDP %6f Other %6f\n", 
		//		item->report->uid, 
		//		item->report->metrics[0].value, item->report->metrics[1].value, 
		//		item->report->metrics[2].value, item->report->metrics[3].value);
		
		
		item->next = moduleReport->list;
		moduleReport->list = item;
	}
	
	return moduleReport;
}

void harvester_start(void *args)
{
	// TODO: initialize stats buckets
	struct stats stats;
	// TODO: metricCount should be negotiated
	stats.metricCount = 4;

	struct module_report *moduleReport;
	while( ( moduleReport = read_module_report(peer_socket, stats.metricCount) ) != NULL )
	{
		//printf("Received report for %hi users\n", moduleReport->count);
			
		struct report_list *item = moduleReport->list;
			
		while ( item != NULL )
		{
			// TODO: retrieve stats object item from bucket instead
			if( strcmp("10.0.0.12", item->report->uid) == 0 )
			{
				// TODO: start/end should not be in report??
				item->report->start = moduleReport->start;
				item->report->end = moduleReport->end;
				
				//printf("got %f for udp packets count\n", item->report->metrics[2].value);
				//printf("%-15s : ICMP %6f TCP %6f UDP %6f Other %6f\n", 
				//		item->report->uid, 
				//		item->report->metrics[0].value, item->report->metrics[1].value, 
				//		item->report->metrics[2].value, item->report->metrics[3].value);
				
				aggregate( item->report, &stats );
			}
			item = item->next;
		}
	}
	
	printf("Worker %s disconnected.\n", (char *) args);
	close(peer_socket);
}

void orchestrator_start(void *args)
{
	struct sockaddr_in peer_addr;
	fd_set active_fd_set;
	int server_socket = open_listening_socket(&portno);
	
	const struct timespec timeout = { 1, 0};
	
	socklen_t sock_len = sizeof(struct sockaddr_in);
	
	while(keepRunning)
	{
		FD_ZERO(&active_fd_set);
		FD_SET(server_socket, &active_fd_set);
		
		//TODO: catch ctrl-c signal rather than timeouts 
		if(pselect(FD_SETSIZE, &active_fd_set, NULL, NULL, &timeout, NULL) > 0)
		{
			peer_socket = accept(server_socket, (struct sockaddr *) &peer_addr, &sock_len);
			
			char *ipaddress;
			inet_ntop(AF_INET, &(peer_addr.sin_addr), ipaddress, INET_ADDRSTRLEN ); // only ipv4 address
			
			printf("Worker %s connected - sending modules (socketfd: %d)\n", ipaddress, peer_socket);
			
			send_modules_to_worker(peer_socket);
			
			// TODO: negotiate module names and metric count
			
			pthread_t harvesterThread;
			start_thread("harvester", &harvesterThread, &harvester_start, (void *) ipaddress);
		}
	}
	
	close(server_socket);
}

int main(int argc, char *argv[])
{
	signal(SIGINT, intHandler);
	
	pthread_t orchestratorThread;
	start_thread("orchestrator", &orchestratorThread, &orchestrator_start, NULL);
	
	//pthread_t harvesterThread;
	//start_thread("harvester", &harvesterThread, &harvester_start, NULL);
	
	while (keepRunning)
	{
	}
	
	pthread_join(orchestratorThread, NULL);
}