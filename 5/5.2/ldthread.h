#ifndef LDTHREAD_H
#define LDTHREAD_H

#include <pthread.h>
#include "list.h"

#define MAX_LOCK_DEPTH 64UL

typedef struct {

	pthread_mutex_t		lock;
	char 				*name;
	struct list_head	child;
}ldthread_mutex_t;

typedef struct {

	pthread_mutexattr_t	attr;
} ldthread_mutexattr_t ;

/*
 * Every lock has a list of other locks that were taken after it.
 * We only grow the list, never remove from it:
 */
typedef struct {

	struct list_head		sibling;
	ldthread_mutex_t		*lock;
	unsigned int			dep_gen_id;
} lock_list ;

typedef struct {
	ldthread_mutex_t *lock;
	int LINE;
	char *FUNC, *SRC_FILE;
} held_lock ;

typedef struct {
	pthread_t		thread;
	char 			*name;
	void 			*(*start_func)(void *);
	void 			*arg;
	int 			lockdep_depth;
	held_lock		held_locks[MAX_LOCK_DEPTH];
} ldthread_t ;

typedef struct {

	pthread_attr_t attr;
} ldthread_attr_t ;

#endif
