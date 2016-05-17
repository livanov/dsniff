#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

#include "sockwrap.h"
#include "config.h"
#include "data_models.h"
//#include "modules/icmpcount.h"

#define LEVEL_PER_SECOND	0
#define LEVEL_PER_MINUTE	1
#define LEVEL_PER_HOUR		2
#define LEVEL_PER_DAY		3
#define LEVEL_PER_MONTH		4
#define LEVEL_PER_YEAR		5

struct stats *stats[LEVEL_PER_YEAR + 1];

//TODO: change to interface struct
typedef void (*flush_f) ();
typedef int (*get_metric_count_f) ();
typedef char* (*get_module_name_f) ();
typedef void (*got_packet_f) (char *packetBytes, int packetLen);
typedef struct metric* (*aggregate_data_f)();

flush_f 				flush;
get_metric_count_f 		get_metric_count;
get_module_name_f 		get_module_name;
got_packet_f 			got_packet;
aggregate_data_f 		aggregate_data;

short 					program_abort = 0;


void load_module(char *module_name)
{
	void *handle = dlopen ( module_name, RTLD_NOW );

	flush = dlsym ( handle, "flush_data" );
	get_metric_count = dlsym ( handle, "get_metric_count" );
	get_module_name = dlsym ( handle, "get_module_name" );
	got_packet = dlsym ( handle, "got_packet" );
	aggregate_data = dlsym ( handle, "aggregate_data" );
}

static void printstats(int level)
{
	int i, j;
	int c=0;
		
	printf("----------------------------------- [ %d ] -----------------------------------\n", level);
	for(i = 0; i <= LEVEL_PER_YEAR; i++)
	{
		int idx = stats[i]->idx;
		
		printf("idx: [%5d]; accum_time: [%5d];", idx, stats[i]->accumulated_time);
		
		if(idx > -1)
		{
			printf("\n\tLast record - timestamp [%d]\n", stats[i]->reports[idx].timestamp);
			for(j = 0; j < stats[i]->reports[idx].metric_count; j++)
				printf("\t\t%15s: [%11.3f] accumulated: [%11.3f]\n", 
					stats[i]->reports[idx].metrics[j].name, 
					stats[i]->reports[idx].metrics[j].count,
					stats[i]->accumulated_aggregates[j]);
		}
		
		printf("\n");
	}
	printf("----------------------------------- [ %d ] -----------------------------------\n\n", level);
}

void aggregate(struct report this, int level)
{
	int metric_idx = 2;

	if(level > LEVEL_PER_YEAR) return;

	int metric_count =  this.metric_count;
	double residual[metric_count];
	int *accumulated_time = &( stats[level]->accumulated_time );
	int *last_report_idx = &( stats[level]->idx );
	double *accumulated_aggregates = stats[level]->accumulated_aggregates;
	struct report *reports = stats[level]->reports;
	int *this_level_buffer_capacity = &( stats[level]->period_len_in_s );
	int *next_level_buffer_capacity = &( stats[level + 1]->period_len_in_s );
	
	// sets the length of this period based on previous period
	int last_time_window_len;
	if(*last_report_idx == -1) last_time_window_len = *this_level_buffer_capacity;
		else last_time_window_len = this.timestamp - reports[*last_report_idx].timestamp;

		
	if( ++(*last_report_idx) >= REPORTS_TO_SAVE ) 
		*last_report_idx -= REPORTS_TO_SAVE;
	
	reports[*last_report_idx] = this;
	reports[*last_report_idx].metrics = malloc(metric_count * sizeof(struct metric));
	memcpy(reports[*last_report_idx].metrics, this.metrics, metric_count * sizeof(struct metric));
	
	int remaining_unused_time =  last_time_window_len;
	int i;
	for( i = 0 ; i < metric_count ; i++)
	{
		residual[i] = this.metrics[i].count;
	}
	
	//double residual = (this.metrics)[metric_idx].count;
	
	while ( *accumulated_time + remaining_unused_time >= *next_level_buffer_capacity ) {
		
		struct report aggregated_report;
		aggregated_report.module_name = malloc (sizeof(this.module_name));
		strcpy(aggregated_report.module_name, this.module_name);
		aggregated_report.metric_count = metric_count;
		
		aggregated_report.metrics = malloc ( metric_count * sizeof (struct metric) );
		
		double frac = ( *next_level_buffer_capacity - *accumulated_time ) / ( 1.0 * last_time_window_len );
		
		for( i = 0 ; i < metric_count ; i++)
		{
			aggregated_report.metrics[i].name = malloc (sizeof((this.metrics)[i].name));
			strcpy(aggregated_report.metrics[i].name, (this.metrics)[i].name);
		
			aggregated_report.metrics[i].count = accumulated_aggregates[i]
					+ frac * (this.metrics)[i].count;
					
			residual[i] -= frac * ( this.metrics )[i].count;
		}
		
    
		remaining_unused_time += ( *accumulated_time - *next_level_buffer_capacity );
		
		aggregated_report.timestamp = this.timestamp - *this_level_buffer_capacity;
		
		aggregate(aggregated_report, ++level);
		level--;
		
		*accumulated_time = 0;
		for( i = 0 ; i < metric_count ; i++)
			accumulated_aggregates[i] = 0;
		//*accumulated_aggregate = 0;
	}
    
	*accumulated_time += remaining_unused_time;
	for( i = 0 ; i < metric_count ; i++)
		accumulated_aggregates[i] += residual[i];
	//*accumulated_aggregate += residual;
	
	if(level == 0) printstats(level);
}

void send_to_aggregator(struct report this)
{
	//TODO: send over TCP
	
	aggregate(this, 0);
}

void* reporting_start(void* args)
{
	struct report report;
	report.module_name = malloc(MAX_MODULE_NAME_LEN);
	strcpy(report.module_name, get_module_name());
	report.metric_count = get_metric_count();
	
	int i;
	for( i = 0 ; i <= LEVEL_PER_YEAR; i++)
		stats[i]->accumulated_aggregates = malloc( report.metric_count * sizeof(double) );

	while(!program_abort)
	{
		sleep( 2 );

		report.timestamp = time(NULL);

		report.metrics = aggregate_data();
		
		send_to_aggregator(report); // thread unsafe

		free(report.metrics);
		
		flush(); // thread unsafe
	}

	free(report.module_name);
}

void check_argument_list(int argc, char *argv[])
{
    if (argc < 2) {
       fprintf(stderr, "usage %s hostname\n", argv[0]);
       exit(EXIT_FAILURE);
    }
}

int connect_to_publisher(char *hostname)
{
	int socketfd;

	if( (socketfd = connect_to_host(hostname, PUBLISHER_PORT)) == -1)
	{
		exit(EXIT_FAILURE);
	}

	return socketfd;
}

pthread_t establish_reporting()
{
	pthread_t reportingThread;
	int err = pthread_create(&reportingThread, NULL, &reporting_start, NULL);
	if(err != 0)
		fprintf(stderr, "Failed to create background reporting thread. Reason:[%s]\n", strerror(err));
	else
		fprintf(stdout, "Reporting started successfully!\n");

	return reportingThread;
}

void receive_packets(int socketfd)
{
	char buffer[SOCKET_BUFFER_SIZE];
	uint16_t networkLen;
	while(read(socketfd, &networkLen, sizeof(networkLen)) > 0)
	{
		networkLen = ntohs(networkLen);
		read(socketfd, buffer, networkLen);

		got_packet(buffer, networkLen);
    }
}

void initialize_stat_variables()
{
	int i;
	for( i = 0; i <= LEVEL_PER_YEAR; i++)
	{
		stats[i] = malloc(sizeof(struct stats));
		stats[i]->idx = -1;
		stats[i]->accumulated_time = 0;
		
		// read past records from db
	}
	
	stats[ LEVEL_PER_SECOND ]->period_len_in_s = 1;
	stats[ LEVEL_PER_MINUTE ]->period_len_in_s = stats[ LEVEL_PER_SECOND ]->period_len_in_s * SECONDS_IN_MINUTE;
	stats[ LEVEL_PER_HOUR	]->period_len_in_s = stats[ LEVEL_PER_MINUTE ]->period_len_in_s * MINUTES_IN_HOUR;
	stats[ LEVEL_PER_DAY	]->period_len_in_s = stats[ LEVEL_PER_HOUR	 ]->period_len_in_s * HOURS_IN_DAY;
	stats[ LEVEL_PER_MONTH	]->period_len_in_s = stats[ LEVEL_PER_DAY	 ]->period_len_in_s * DAYS_IN_MONTH;
	stats[ LEVEL_PER_YEAR	]->period_len_in_s = stats[ LEVEL_PER_MONTH	 ]->period_len_in_s * MONTHS_IN_YEAR;
}

int main(int argc, char *argv[])
{
	check_argument_list(argc, argv);

	load_module("modules/icmpcount.so");

	initialize_stat_variables();
	
	int socketfd = connect_to_publisher(argv[1]);			// connects to publisher by hostname/IP address

	pthread_t reportingThread = establish_reporting();		// connects to aggregator

	receive_packets(socketfd); 								// should be on another thread so blocking read can be interrupted

	printf("\nDisconnected from server.\n");

	return 0;
}