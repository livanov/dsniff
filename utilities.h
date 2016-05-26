#include <pthread.h>

void start_thread(char *threadName, pthread_t *thread, void *start_routine, void *arg);