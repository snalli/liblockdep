#ifndef SCHED_H
#define SCHED_H

#include "lockdep.h"
#include <pthread.h>

#define MAX_LOCK_DEPTH 8UL

struct task_struct {

	pthread_t thread;
	int 		 tid;
	unsigned int lockdep_recursion;
	unsigned int lockdep_depth;
	struct held_lock held_locks[MAX_LOCK_DEPTH];

};
extern struct task_struct *current;
#endif
