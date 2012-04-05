#include "mutex.h"
#include "lockdep.h"
#include "list.h"
#include <pthread.h>
#include <stdio.h>

int __mutex_init(struct mutex *lock, const char *name)
{
	lock->name = name;

//	#ifdef CONFIG_LOCKDEP
		struct lock_list *entry;
		entry = (struct lock_list *)alloc_list_entry();
		entry->lock = lock;
		entry->dep_gen_id = 0;
		INIT_LIST_HEAD(&entry->entry);
		INIT_LIST_HEAD(&entry->locks_after);
		INIT_LIST_HEAD(&entry->locks_before); 
		lock->lock_entry = entry;
//	#endif
	
	return pthread_mutex_init(&lock->lock, NULL);
}

int mutex_lock(struct mutex *lock)
{
//	#ifdef CONFIG_LOCKDEP
		if (lock_acquire(lock->lock_entry)) {
//	#endif
		return	pthread_mutex_lock(&lock->lock);}
		
		return -1;
}
int mutex_unlock(struct mutex *lock)
{
		return	pthread_mutex_unlock(&lock->lock);
}
