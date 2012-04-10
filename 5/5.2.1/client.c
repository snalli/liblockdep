#include "lockdep.h"
#include <pthread.h>
#include <stdio.h>

//extern struct ldthread_t *curr ;//= current;
pthread_mutex_t mutex, mutex_1, mutex_2;
pthread_mutexattr_t attr, attr_1, attr_2;
pthread_t myThread, myThread_1;

void *func()
{
//	printf ("\n%lu\n",pthread_self());
	pthread_mutex_lock(&mutex);
	pthread_mutex_lock(&mutex_1);
	pthread_mutex_lock(&mutex_2);
	printf("\nHello\n");
	pthread_mutex_unlock(&mutex_2);
	pthread_mutex_unlock(&mutex_1);
	pthread_mutex_unlock(&mutex);
}
void *func_1()
{
//	printf ("\n%lu\n",pthread_self());
	pthread_mutex_lock(&mutex_1);
	pthread_mutex_lock(&mutex);
	printf("\nHi\n");
	pthread_mutex_unlock(&mutex);
	pthread_mutex_unlock(&mutex_1);
}

void main()
{
	pthread_mutex_init(&mutex, &attr);
	pthread_mutex_init(&mutex_1, &attr_1);
	pthread_mutex_init(&mutex_2, &attr_2);
//	curr = &myThread;
	pthread_create(&myThread, NULL, func, NULL);
	//pthread_join(myThread,NULL);
//	curr = &myThread_1;
	pthread_create(&myThread_1, NULL, func_1, NULL);
//	printf("%d",pthread_equal(myThread,myThread_1));
	pthread_exit(NULL);
}
