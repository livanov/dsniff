#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "data_models.h"

struct stats 			stats;

struct normal_distribution get_std(double *array, long count)
{
	struct normal_distribution distr;
	distr.mean = 0;
	
	long i;

	
	//printf("\n--------------------------------- STD DEV ---------------------------------\n");
	//for(i = 0; i < count; i++)
	//{
	//	printf("[%12.3f] ", array[i]);
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
	
	//printf("\n--------------------------------- STD DEV ---------------------------------\n");
	//printf("    RESULTS:\t\tMean:[%12.5f]\tDeviation: [%12.5f]\n", distr.mean, distr.std);
	//printf("--------------------------------- STD DEV ---------------------------------\n\n");
	
	return distr;
}

struct normal_distribution combine_std(struct report_list *reports, struct aggregate *history, struct normal_distribution distr)
{
	int c = 0;
	struct normal_distribution leftover, combined;
	double ess = 0, gss = 0;
	
	leftover.count 	= reports->report->start - history->start;
	leftover.mean  	= history->normal_distribution.mean;
	ess 			= pow(history->normal_distribution.std, 2) * (leftover.count - 1);
	
	//printf("distr %d : n=%7ld(%3ld) mean=%12.5f %12.5f\n", 
	//		++c, leftover.count, history->normal_distribution.count, leftover.mean, history->normal_distribution.std);
	
	combined.count 	= leftover.count;
	combined.mean	= leftover.count * leftover.mean;
		
	combined.count 	+= distr.count;
	combined.mean  	+= distr.count * distr.mean;
	ess 			+= pow(distr.std, 2) * (distr.count - 1);
	
	//printf("distr %d : n=%12ld mean=%12.5f %12.5f\n", ++c, distr.count, distr.mean, distr.std);
	
	struct aggregate *begin = history->next;
	for( history = begin; history != NULL; history = history->next )
	{
		combined.count 	+= history->normal_distribution.count;
		combined.mean  	+= history->normal_distribution.count * history->normal_distribution.mean;
		ess				+= pow(history->normal_distribution.std, 2) * history->normal_distribution.count;
		combined.std += history->normal_distribution.count * 
				( history->normal_distribution.std  * history->normal_distribution.std +
				  history->normal_distribution.mean * history->normal_distribution.mean );
	}
	
	combined.mean /= combined.count;
	
	gss += pow(leftover.mean - combined.mean, 2) * leftover.count;
	gss += pow(distr.mean - combined.mean, 2) * distr.count;
	
	for( history = begin; history != NULL; history = history->next )
	{
		gss += pow(history->normal_distribution.mean - combined.mean, 2) * history->normal_distribution.count;
	}

	combined.std = sqrt((ess + gss)/(combined.count - 1));
	
	//printf("RESULT  : n=%12ld mean=%12.5f %12.5f\n", combined.count, combined.mean, combined.std);
	
	return combined;
}

void aggregate(struct report report)
{
	//copies and prepends new item in the report list
	struct report_list *item = malloc(sizeof(struct report_list));
	item->next = stats.reports;
	item->report = malloc(sizeof(struct report));
	memcpy(item->report, &report, sizeof(struct report));
	item->report->metrics = malloc(stats.metric_count * sizeof(struct metric));
	memcpy(item->report->metrics, report.metrics, stats.metric_count * sizeof(struct metric));
	stats.reports = item;
	
	stats.accumulated_time += report.end - report.start + 1;
	
	
	// TODO: compare received report values with thresholds 
	
	int BUFFER_TIME = 5;
	int BUFFER_SIZE = 7; // BUFFER_TIME should be < BUFFER_SIZE
	
	if(stats.accumulated_time >= BUFFER_TIME)
	{
		stats.accumulated_time -= BUFFER_TIME;
		
		static long time_passed = 0;
		struct report_list *report, *temp;
		double values[BUFFER_SIZE];
		long count = 0;
	
		for(report = stats.reports ; report != NULL ; report = report->next) 
		{
			values[count++] = report->report->metrics[2].value;
		
			if(stats.history == NULL || report->report->start > stats.history->start)
			{
				int period_length = report->report->end - report->report->start + 1;
				time_passed += period_length;
				
				if(time_passed >= BUFFER_TIME)
				{
					time_passed -= BUFFER_TIME;
					double frac = (1.0 * time_passed) / period_length;
					if(stats.residual != 0) values[count++] = stats.residual;
					stats.residual = frac * values[0];
					values[0] = (1 - frac) * values[0];
					
					
					struct aggregate *aggregate = malloc(sizeof(struct aggregate));
					aggregate->start = time_passed + report->report->start;
					aggregate->end = BUFFER_TIME + aggregate->start - 1;
					aggregate->normal_distribution = get_std(values, count);
					aggregate->next = stats.history;
					stats.history = aggregate;
				}
			}
			
			if(count == BUFFER_SIZE) 
				break; 
		}
		
		struct normal_distribution distr = get_std(values, count);
		
		if(report == NULL) return;
	
		struct aggregate *history;
		history = stats.history;
		while( history!=NULL && history->start <= report->report->start)
			history = history->next;
	
		struct normal_distribution combined = combine_std(report, history, distr);
		
			
		// TODO: adjust values to fit the BUFFER_TIME defined timeframe

		
		// TODO: aggregate old reports and update thresholds
		
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

void initialize_aggregator(char *module_name, int metric_count)
{
	// initialize stats
	stats.count = 0;
	stats.residual = 0;
	stats.accumulated_time = 0;
	stats.reports = malloc (sizeof(struct report_list));
	stats.reports = NULL;
	stats.history = malloc (sizeof(struct aggregate));
	stats.history = NULL;
	stats.module_name = malloc(MAX_MODULE_NAME_LEN);
	strcpy(stats.module_name, module_name);
	stats.metric_count = metric_count;
}

void free_aggregator_resources()
{
	free(stats.module_name);
}
