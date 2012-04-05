#include "lockdep.h"
#include "lockdep_internals.h"
#include "list.h"
#include "sched.h"
#include <stdio.h>

static inline int match (void *src, void *tgt) { }

/*
 * lockdep_lock: protects the lockdep graph, the hashes and the
 *               class/list/hash allocators.
 *
 * This is one of the rare exceptions where it's justified
 * to use a raw spinlock - we really dont want the spinlock
 * code to recurse back into the lockdep code...
 
static arch_spinlock_t lockdep_lock = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;

static int graph_lock(void)
{
	arch_spin_lock(&lockdep_lock);
	/*
	 * Make sure that if another CPU detected a bug while
	 * walking the graph we dont change it (while te other
	 * CPU is busy printing out stuff with the graph lock
	 * dropped already)
	 
	if (!debug_locks) {
		arch_spin_unlock(&lockdep_lock);
		return 0;
	}
	/* prevent any recursions within lockdep from causing deadlocks 
	current->lockdep_recursion++;
	return 1;
}

static inline int graph_unlock(void)
{
	if (debug_locks && !arch_spin_is_locked(&lockdep_lock))
		return DEBUG_LOCKS_WARN_ON(1);

	current->lockdep_recursion--;
	arch_spin_unlock(&lockdep_lock);
	return 0;
}
 */
 
static int graph_lock(void) { return 1; }
static int graph_unlock(void) { return 1; }

static int lockdep_initialized;

unsigned long nr_list_entries;
static struct lock_list list_entries[MAX_LOCKDEP_ENTRIES];

static inline int lock_equal(struct lock_list *entry, void *data)
{
	return entry == data;
}

/*
 * The circular_queue and helpers are used to implement the
 * breadth-first search (BFS) algorithm, by which we can build
 * the shortest path from the next lock to be acquired to the
 * previous held lock, if there is a circular dependency between them.
 */
 
#define MAX_CIRCULAR_QUEUE_SIZE		32UL
#define CQ_MASK				(MAX_CIRCULAR_QUEUE_SIZE-1)

struct circular_queue {
	unsigned long element[MAX_CIRCULAR_QUEUE_SIZE];
	unsigned int  front, rear;
};

static struct circular_queue lock_cq;

unsigned int max_bfs_queue_depth;

static unsigned int lockdep_dependency_gen_id;

static void __cq_init(struct circular_queue *cq)
{
	cq->front = cq->rear = 0;
	lockdep_dependency_gen_id++;
}

static inline int __cq_full(struct circular_queue *cq)
{
	return ((cq->rear + 1) & CQ_MASK) == cq->front;
}

static inline int __cq_enqueue(struct circular_queue *cq, unsigned long elem)
{
	if (__cq_full(cq))
		return -1;

	cq->element[cq->rear] = elem;
	
	cq->rear = (cq->rear + 1) & CQ_MASK;
	return 0;
}

static inline int __cq_empty(struct circular_queue *cq)
{
	return (cq->front == cq->rear);
}

static inline int __cq_dequeue(struct circular_queue *cq, unsigned long *elem)
{
	if (__cq_empty(cq))
		return -1;

	*elem = cq->element[cq->front];
	
	cq->front = (cq->front + 1) & CQ_MASK;
	return 0;
}

static inline unsigned int  __cq_get_elem_count(struct circular_queue *cq)
{
	return (cq->rear - cq->front) & CQ_MASK;
}


static inline unsigned long lock_accessed(struct lock_list *lock)
{
	return lock->dep_gen_id == lockdep_dependency_gen_id;
}

static inline void mark_lock_accessed(struct lock_list *lock)
{
	lock->dep_gen_id = lockdep_dependency_gen_id;
}


static int __bfs(struct lock_list *source_entry,
		 void *data,
		 int (*match)(struct lock_list *entry, void *data),
		 struct lock_list **target_entry,
		 int forward)
{
	struct lock_list *entry;
	struct list_head *head;
	struct lock_list *lock;
	unsigned int cq_depth;
	struct circular_queue *cq = &lock_cq;
	int ret = 0;

	if (forward)
		head = &source_entry->locks_after;
	else
		head = &source_entry->locks_before;

	if (list_empty(head))
		return 0;

	__cq_init(cq);
	__cq_enqueue(cq, (unsigned long)source_entry); 

	while (!ret) {

		ret = __cq_empty(cq); 
		if (!ret) {

			__cq_dequeue(cq, (unsigned long *)&lock); 

			if (forward)
				head = &lock->locks_after;
			else
				head = &lock->locks_before;

			list_for_each_entry(entry, head, entry) {

				if (!lock_accessed(entry)) {

					mark_lock_accessed(entry);

					if (match(entry, data)) {
						target_entry = &entry;
						return 1;
					}

					if (__cq_enqueue(cq, (unsigned long)entry))
						return -1;

					cq_depth = __cq_get_elem_count(cq);
					if (max_bfs_queue_depth < cq_depth)
						max_bfs_queue_depth = cq_depth;
				}
				
				if (entry->entry.next == head) 
					break;
			}
		}
	}
}

static inline int __bfs_forwards(struct lock_list *src_entry,
			void *data,
			int (*match)(struct lock_list *entry, void *data),
			struct lock_list **target_entry)
{
	return __bfs(src_entry, data, match, target_entry, 1);
}

static inline int __bfs_backwards(struct lock_list *src_entry,
			void *data,
			int (*match)(struct lock_list *entry, void *data),
			struct lock_list **target_entry)
{
	return __bfs(src_entry, data, match, target_entry, 0);

}

/*
 * Prove that the dependency graph starting at <entry> can not
 * lead to <target>. Print an error and return 0 if it does.
 */
static int check_noncircular(struct lock_list *source, struct lock_list *target,
		struct lock_list **target_entry)
{
	int result;

	result = __bfs_forwards(source, target, lock_equal, target_entry);

	return result;
}

static inline int __print_lock_name(struct mutex *lock)
{
	return printf("%c[1;35m%s", 27, lock->name);
}

static inline void print_lock_name(struct lock_list *lock)
{
	__print_lock_name(lock->lock);

}

static inline void print_lock(struct held_lock *hlock)
{
	print_lock_name(hlock->instance);

}

/*
 * When a circular dependency is detected, print the
 * header first:
 */
static int
print_circular_bug_header(struct lock_list **entry,
			struct held_lock *check_src,
			struct held_lock *check_tgt)
{
	struct task_struct *curr = current;

	printf("%c[0;32m\n=======================================================\n", 27);
	printf(  "%c[1;31m[ INFO: possible circular locking dependency detected ]\n", 27);
	printf(  "%c[0;32m-------------------------------------------------------\n", 27);
	printf("task ");
	printf("%c[1;33m(tid : %d)",27, curr->tid);
	printf("%c[0;32m is trying to acquire lock:\n", 27);
	print_lock(check_src);
	printf("%c[0;32m\nbut task is already holding lock:\n", 27);
	print_lock(check_tgt);
	printf("%c[0;32m\nwhich already depends on the new lock.\n", 27);
	printf("%c[0;32m=======================================================\n\n", 27);
/**
	printf("\nThe existing dependency chain (in reverse order) is:\n");

	print_circular_bug_entry(entry, depth);
*/
	return 0;
}


static int print_circular_bug(struct lock_list **target,
				struct held_lock *check_src,
				struct held_lock *check_tgt)
{
	struct task_struct *curr = current;

	print_circular_bug_header(target, check_src, check_tgt);
/**
	parent = get_lock_parent(target);
	first_parent = parent;

	while (parent) {
		print_circular_bug_entry(parent, --depth);
		parent = get_lock_parent(parent);
	}

	printf("\nother info that might help us debug this:\n\n");
	print_circular_lock_scenario(check_src, check_tgt,
				     first_parent);

	lockdep_print_held_locks(curr);

	printf("\nstack backtrace:\n");
	dump_stack();
*/
	return 0;
}

/*
 * Allocate a lockdep entry. (assumes the graph_lock held, returns
 * with NULL on failure)
 */
struct lock_list * alloc_list_entry(void)
{
	if (nr_list_entries >= MAX_LOCKDEP_ENTRIES) {
			return NULL;

		printf("BUG: MAX_LOCKDEP_ENTRIES too low!\n");
		printf("turning off the locking correctness validator.\n");
	}
	return list_entries + nr_list_entries++;
}

/*
 * Add a new dependency to the head of the list:
 */
static int add_lock_to_list(struct held_lock *master, struct held_lock *slave,
			    struct list_head *head)
{
	struct lock_list *entry;
	entry = slave->instance;

	/*
	 * Lock not present yet - get a new dependency struct and
	 * add it to the list:
	 
	entry = alloc_list_entry();
	if (!entry)
		return 0;

	entry->class = this;
	entry->distance = distance;
	entry->trace = *trace;
	/*
	 * Since we never remove from the dependency list, the list can
	 * be walked lockless by other CPUs, it's only allocation
	 * that we need to take care of.
	 */
	list_add_tail(&entry->entry, head);
	
	

	return 1;
}

static int
check_prev_add(struct task_struct *curr, struct held_lock *prev,
	       struct held_lock *next)
{

	struct lock_list *entry;
	int ret;
	struct lock_list **target_entry;

	/*
	 * Prove that the new <prev> -> <next> dependency would not
	 * create a circular dependency in the graph. (We do this by
	 * forward-recursing into the graph starting at <next>, and
	 * checking whether we can reach <prev>.)
	 */
	ret = check_noncircular(next->instance, prev->instance, target_entry);
	if (ret)
		return print_circular_bug(target_entry, next, prev);

	/*
	 * Is the <prev> -> <next> dependency already present?
	 *
	 * (this may occur even though this is a new chain: consider
	 *  e.g. the L1 -> L2 -> L3 -> L4 and the L5 -> L1 -> L2 -> L3
	 *  chains - the second one will be new, but L1 already has
	 *  L2 added to its dependency list, due to the first chain.)
	 */
	 
	struct list_head *head = &prev->instance->locks_after;
	list_for_each_entry(entry, head, entry) {
		if (entry == next->instance) {
			return 2;
		}
		if (entry->entry.next == head)
			break;		
	}
	 
	/*
	 * Ok, all validations passed, add the new lock
	 * to the previous lock's dependency list:
	 */
	ret = add_lock_to_list(prev, next, &prev->instance->locks_after);

	if (!ret)
		return 0;
/*
	ret = add_lock_to_list(next , prev, &next->instance->locks_before);
	
	if (!ret)
		return 0;
*/
	/*
	 * Debugging printouts:
     */
//	if (verbose(hlock_class(prev)) || verbose(hlock_class(next))) {
//		graph_unlock();
		printf("%c[1;32m\n new dependency: ", 27);
		print_lock(prev);
		printf("%c[1;32m => ", 27);
		print_lock(next);
		printf("%c[0;32m\n\n", 27);
//		return graph_lock();
//	}

	return 1;
}

static int
print_deadlock_bug(struct task_struct *curr, struct held_lock *prev,
		   struct held_lock *next)
{
	printf("%c[0;32m\n=============================================\n",27);
	printf(  "%c[1;31m[ INFO: possible recursive locking detected ]\n",27);
	printf(  "%c[0;32m---------------------------------------------\n",27);
	printf("task ");
	printf("%c[1;33m(tid : %d)",27, curr->tid);
	printf("%c[0;32m is trying to acquire lock:\n", 27);
	print_lock(next);
	printf("%c[0;32m\nbut task is already holding lock:\n",27);
	print_lock(prev);
	printf("%c[0;32m\n=============================================\n\n", 27);
	
	return 0;
}

/*
 * Check whether we are holding such a lock already.
 *
 * (Note that this has to be done separately, because the graph cannot
 * detect such classes of deadlocks.)
 *
 * Returns: 0 on deadlock detected, 1 on OK
 */
static int
check_deadlock(struct task_struct *curr, struct held_lock *next)
{
	struct held_lock *prev;
	int i;

	for (i = 0; i < curr->lockdep_depth; i++) {
		prev = curr->held_locks + i;

		if (prev->instance != next->instance)
			continue;

		return print_deadlock_bug(curr, prev, next);
	}
	return 1;
}

static int
check_prevs_add(struct task_struct *curr, struct held_lock *next)
{
	int depth = curr->lockdep_depth;
	struct held_lock *hlock;

	if (!depth)
		return 0;

		hlock = curr->held_locks + depth-1;
		/*
		 * Only non-recursive-read entries get new dependencies
		 * added:
		 */
			if (!check_prev_add(curr, hlock, next))
				return 0;

	return 1;
}


static int validate_chain(struct task_struct *curr,
		struct held_lock *hlock, int chain_head)
{
		if (!graph_lock())
			return 0;
		/*
		 * Check whether the new lock's dependency graph
		 * could lead back to the previous lock.
		 */
		int ret = check_deadlock(curr, hlock);

		if (!ret)
			return 0;
		/*
		 * Add dependency only if this lock is not the head
		 * of the chain, and if it's not a secondary read-lock:
		 */
		if (!chain_head)
			if (!check_prevs_add(curr, hlock))
				return 0;
		graph_unlock();

	return 1;
}
/*
 * This gets called for every mutex_lock*() operation.
 * We maintain the dependency maps and validate the locking attempt.
 */
int lock_acquire(struct lock_list *lock)
{
/**
 *	Take care of current macro
 */
	struct task_struct *curr = current;
	struct held_lock *hlock;
	unsigned int depth;
	int chain_head = 0;

	/*
	 * Add the lock to the list of currently held locks.
	 * We dont increase the depth just yet, up until the
	 * dependency checks are done.
	 */
	depth = curr->lockdep_depth;

	hlock = curr->held_locks + depth;
	hlock->instance = lock;
/**
	/* mark it as used: 
	if (!mark_lock(curr, hlock, LOCK_USED))
		return 0;
*/
	if (!depth) {
		chain_head = 1;
	}

	if (!validate_chain(curr, hlock, chain_head))
		return 0;

	curr->lockdep_depth++;
/**
	if (unlikely(curr->lockdep_depth > max_lockdep_depth))
		max_lockdep_depth = curr->lockdep_depth;
*/

	return 1;
}

