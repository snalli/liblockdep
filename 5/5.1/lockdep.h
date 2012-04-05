#ifndef LOCKDEP_H
#define LOCKDEP_H

#include "list.h"
#include "mutex.h"

/*
 * Every lock has a list of other locks that were taken after it.
 * We only grow the list, never remove from it:
 */
struct lock_list {

	struct list_head		entry;
	unsigned int			dep_gen_id;
	struct mutex 			*lock;
	/*
	 * These fields represent a directed graph of lock dependencies,
	 * to every node we attach a list of "forward" and a list of
	 * "backward" graph nodes.
	 */
	struct list_head		locks_after, locks_before;
};

struct held_lock {
	struct lock_list *instance;
};

#endif
