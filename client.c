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

#define BUFFER_SIZE		4
#define BUFFER_IN_S		3


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
struct report *reports_per_hr;
struct report *reports_per_day;
struct report *reports_per_week;
struct report *reports_per_month;
int count = 0;
double avg=0;

struct report *last;
int idx = -1;
int idx_m = -1;

void load_module(char *module_name)
{
	void *handle = dlopen ( module_name, RTLD_NOW );

	flush = dlsym ( handle, "flush_data" );
	get_metric_count = dlsym ( handle, "get_metric_count" );
	get_module_name = dlsym ( handle, "get_module_name" );
	got_packet = dlsym ( handle, "got_packet" );
	aggregate_data = dlsym ( handle, "aggregate_data" );
}

void send_to_aggregator(struct report *this)
{
	static int current_passed_time = 1;
	static double sum = 0;
	
	int metric_idx = 2;

	// sets the length of this period based on previous period
	int len = this->timestamp;
	if(idx == -1) len = 0;
		else len -= reports[idx].timestamp;

	if( ++idx >= BUFFER_SIZE ) idx -= BUFFER_SIZE;
	memcpy (&(reports[idx]), this, sizeof(struct report));
	reports[idx].metrics = malloc (this->metric_count * sizeof(struct metric));
	memcpy(reports[idx].metrics, this->metrics, this->metric_count * sizeof(struct metric));

	int remaining_unused_time = len;
	double residual = (this->metrics)[metric_idx].count;

	//printf("metric[2]->count: [%f]\n", residual);

	while ( current_passed_time + remaining_unused_time >= BUFFER_IN_S) {
		idx_m = (idx_m + 1) % BUFFER_SIZE;
		struct report *new_m_report = &(reports_per_m[idx_m]);

		double frac = ( BUFFER_IN_S - current_passed_time ) / ( 1.0 * len );

		new_m_report->metrics = malloc ( this->metric_count * sizeof (struct metric) );
		(new_m_report->metrics)[metric_idx].count = ( sum + frac * (this->metrics)[metric_idx].count );

		residual -= frac * (this->metrics)[metric_idx].count;

		remaining_unused_time += ( current_passed_time - BUFFER_IN_S );
		current_passed_time = 0;
		sum = 0;
		*last = *this;
	}

	current_passed_time += remaining_unused_time;
	sum += residual;

	
	
	
	printf("idx_m: [%d]\n", idx_m);
	int i;
	for ( i = 0 ; i <= idx_m ; i++)
	{
		printf("%f ", ((reports_per_m[i]).metrics[metric_idx]).count);
	}
    
	printf("\n----------------------\n");
}

void* reporting_start(void* args)
{
	struct report *report = malloc(sizeof(report));
	report->module_name = malloc(MAX_MODULE_NAME_LEN);
	strcpy(report->module_name, get_module_name());
	report->metric_count = get_metric_count();

	last = reports;

	while(!program_abort)
	{
		sleep( 1 );

		report->timestamp = time(NULL);

		report->metrics = aggregate_data();

		send_to_aggregator(report); // thread unsafe

		free(report->metrics);
		
		flush(); // thread unsafe
	}

	free(report->module_name);
	free(report);
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

	//make dynamic
	//reports = malloc(60 * 60 * 24 * 31 * 3 * sizeof(struct report));
	//memset(reports, 0, sizeof(reports));

	check_argument_list(argc, argv);

	load_module("modules/icmpcount.so");

	int socketfd = connect_to_publisher(argv[1]);			// connects to publisher by hostname/IP address

	pthread_t reportingThread = establish_reporting();		// connects to aggregator

	receive_packets(socketfd); 								// should be on another thread so blocking read can be interrupted

	printf("\nDisconnected from server.\n");

	return 0;
}