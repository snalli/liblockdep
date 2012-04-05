#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

struct mutex {	

	pthread_mutex_t 	lock;	
	const char			*name;
	
	struct lock_list	*lock_entry;

};

#define mutex_init(lock) \
	__mutex_init((lock), #lock);
#endif

