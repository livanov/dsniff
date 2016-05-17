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

#define BUFFER_SIZE		100
#define BUFFER_IN_S		2

struct stats{
	int accumulated_time;
	int idx;
	int length_in_s;
	double accumulated_aggregate;
	struct report *last;
	struct report reports[BUFFER_SIZE];
};

struct stats *stats[6];

//TODO: change to interface struct
typedef void (*flush_f) ();
typedef int (*get_metric_count_f) ();
typedef char* (*get_module_name_f) ();
typedef void (*got_packet_f) (char *packetBytes, int packetLen);
typedef struct metric* (*aggregate_data_f)();

flush_f flush;
get_metric_count_f get_metric_count;
get_module_name_f get_module_name;
got_packet_f got_packet;
aggregate_data_f aggregate_data;

short program_abort = 0;
struct report reports[BUFFER_SIZE];
//struct report *reports;
struct report reports_per_m[BUFFER_SIZE];
struct report reports_per_hr[BUFFER_SIZE];
struct report *reports_per_day;
struct report *reports_per_week;
struct report *reports_per_month;

struct report *report_aggregates[3] = { reports, reports_per_m, reports_per_hr };

int accumulated_times[3] = { 1, 0, 0 };
double accumulated_aggregates[3] = { 0, 0, 0 };
int idxes[3] = { -1, -1, -1 };


void load_module(char *module_name)
{
	void *handle = dlopen ( module_name, RTLD_NOW );

	flush = dlsym ( handle, "flush_data" );
	get_metric_count = dlsym ( handle, "get_metric_count" );
	get_module_name = dlsym ( handle, "get_module_name" );
	got_packet = dlsym ( handle, "got_packet" );
	aggregate_data = dlsym ( handle, "aggregate_data" );
}

void printstats(int metric_idx, int level)
{
	int i, j;
		
	printf("----------------------------------- [ %d ] -----------------------------------\n", level);
	for(i = 0; i < 6; i++)
	{
		int idx = stats[i]->idx;
		
		printf("idx: [%5d]; accum_time: [%5d]; accum_aggre: [%14f]; ",
			idx, stats[i]->accumulated_time, stats[i]->accumulated_aggregate);

		if(idx > -1)
			printf("\n\tLast record %15s: [%14f] timestamp: [%d]", 
				stats[i]->reports[idx].metrics[metric_idx].name, 
				stats[i]->reports[idx].metrics[metric_idx].count,
				stats[i]->reports[idx].timestamp);
		
		printf("\n");
	}
	printf("----------------------------------- [ %d ] -----------------------------------\n\n", level);
}

void aggregate(struct report this, int level)
{
	int metric_idx = 2;

	if(level > 5) return;
	
	int metric_count =  this.metric_count;
	int *accumulated_time = &( stats[level]->accumulated_time );
	int *idx = &( stats[level]->idx );
	double *accumulated_aggregate = &( stats[level]->accumulated_aggregate );
	struct report *ss = stats[level]->reports;
	int *buffer_size = &( stats[level + 1]->length_in_s );
	
	// sets the length of this period based on previous period
	int last_time_window_len;
	if(*idx == -1) last_time_window_len = stats[level]->length_in_s;
		else last_time_window_len = this.timestamp - ss[*idx].timestamp;
		
	if( ++(*idx) >= BUFFER_SIZE ) 
		*idx -= BUFFER_SIZE;
	
	ss[*idx] = this;
	ss[*idx].metrics = malloc(metric_count * sizeof(struct metric));
	memcpy(ss[*idx].metrics, this.metrics, metric_count * sizeof(struct metric));
	
	int remaining_unused_time =  last_time_window_len;// / stats[level]->length_in_s;
	double residual = (this.metrics)[metric_idx].count;
	
	while ( *accumulated_time + remaining_unused_time >= *buffer_size ) {
		
		struct report aggregated_report;
		aggregated_report.module_name = malloc (sizeof(this.module_name));
		strcpy(aggregated_report.module_name, this.module_name);
		aggregated_report.metric_count = metric_count;
		
		aggregated_report.metrics = malloc ( metric_count * sizeof (struct metric) );
		
		int i;
		for( i = 0 ; i < metric_count ; i++)
		{
			aggregated_report.metrics[i].name = malloc (sizeof((this.metrics)[i].name));
			strcpy(aggregated_report.metrics[i].name, (this.metrics)[i].name);
		}
		
		double frac = ( *buffer_size - *accumulated_time ) / ( 1.0 * last_time_window_len );
		
		aggregated_report.metrics[metric_idx].count = *accumulated_aggregate 
						+ frac * (this.metrics)[metric_idx].count;
						
		if(level==-1) printf("\nlast_win:[%d]\t\tremain:[%d]\nresidual:[%f]\taccu_time[%d]\nbuff_size[%d]\t\taccu_agg:[%d]\nfrac:[%f]\t\tudp count:[%f]\n\n",
				last_time_window_len, remaining_unused_time, residual, *accumulated_time, 
				*buffer_size, *accumulated_aggregate, frac, aggregated_report.metrics[metric_idx].count);
		
		residual -= frac * ( this.metrics )[metric_idx].count;
    
		remaining_unused_time += ( *accumulated_time - *buffer_size );
		stats[level]->last = &this;
		
		aggregated_report.timestamp = this.timestamp - stats[level]->length_in_s;
		
		aggregate(aggregated_report, ++level);
		
		*accumulated_time = 0;
		*accumulated_aggregate = 0;
		level--;
	}
    
	*accumulated_time += remaining_unused_time;
	*accumulated_aggregate += residual;
	
	if(level == 0) printstats(metric_idx, level);
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

	//last = reports;

	while(!program_abort)
	{
		sleep( 1 );

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

int main(int argc, char *argv[])
{
	int i;
	for( i = 0; i < 6; i++)
	{
		stats[i] = malloc(sizeof(struct stats));
		stats[i]->idx = -1;
			
			stats[i]->accumulated_time = 0;
		if( i == 0 ) 
		{
			stats[i]->length_in_s = 1;
		}
		else 
			stats[i]->length_in_s = stats[i-1]->length_in_s * BUFFER_IN_S;
	}
	
	stats[0]->length_in_s = 1;
	stats[1]->length_in_s = stats[0]->length_in_s * 60;
	stats[2]->length_in_s = stats[1]->length_in_s * 60;
	stats[3]->length_in_s = stats[2]->length_in_s * 24;
	stats[4]->length_in_s = stats[3]->length_in_s * 30;
	stats[5]->length_in_s = stats[4]->length_in_s * 100;
	
	
	check_argument_list(argc, argv);

	load_module("modules/icmpcount.so");

	int socketfd = connect_to_publisher(argv[1]);			// connects to publisher by hostname/IP address

	pthread_t reportingThread = establish_reporting();		// connects to aggregator

	receive_packets(socketfd); 								// should be on another thread so blocking read can be interrupted

	printf("\nDisconnected from server.\n");

	return 0;
}