#include <stdio.h>

#include "utilities.h"

void start_thread(char *threadName, pthread_t *thread, void *start_routine, void *arg)
{
	int err = pthread_create(thread, NULL, start_routine, arg);
	if(err != 0)
		fprintf(stderr, "Failed to create background %s thread. Reason:[%s]\n", threadName, strerror(err));
	else
		fprintf(stdout, "%s thread started successfully!\n", threadName);
}
