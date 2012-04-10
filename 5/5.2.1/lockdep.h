#ifndef LOCKDEP_H
#define LOCKDEP_H

#include "ldthread.h"

#define pthread_t			ldthread_t
#define pthread_create(t,a1,f,a2)	ldthread_create(t,a1,f,a2,#t)
#define pthread_join			ldthread_join

#define pthread_mutex_t			ldthread_mutex_t
#define pthread_mutex_init(l,a)		ldthread_mutex_init(l,a,#l)
#define pthread_mutex_lock(l)		ldthread_mutex_lock(l,__LINE__,__func__,__FILE__)
#define pthread_mutex_unlock(l)		ldthread_mutex_unlock(l,__LINE__,__func__,__FILE__)
#define pthread_mutexattr_t		ldthread_mutexattr_t

#endif

