/*
 * Runtime locking correctness validator
 * - By Sanketh Nalli
 * Fourth year Engineering Student,
 * Dept. of Information Techonlogy,
 * NIT-K, India
 *
 * This code maps all the lock dependencies as they occur in a live program
 * and will warn about the following classes of locking bugs:
 *
 * - lock inversion scenarios
 * - circular lock dependencies
 *
 * Bugs are reported even if the current locking scenario does not cause
 * any deadlock at this point.
 *
 * I.e. if anytime in the past two locks were taken in a different order,
 * even if it happened for another task, even if those were different
 * locks (but of the same class as this lock), this code will detect it.
 *
 */
#include <stdio.h>
#include "ldthread.h"

#define MAX_LOCKDEP_ENTRIES	256UL
#define MAX_NR_THREADS		64UL

static unsigned long nr_list_entries;
static unsigned long nr_table_entries;
static lock_list list_entries[MAX_LOCKDEP_ENTRIES];
static pthread_mutex_t graph_lock = PTHREAD_MUTEX_INITIALIZER,
					spawn_lock = PTHREAD_MUTEX_INITIALIZER;
/*
 * The circular_queue and helpers are used to implement the
 * breadth-first search (BFS) algorithm, by which we can build
 * the shortest path from the next lock to be acquired to the
 * previous held lock, if there is a circular dependency between them.
 */
 
#define MAX_CIRCULAR_QUEUE_SIZE		128UL
#define CQ_MASK				(MAX_CIRCULAR_QUEUE_SIZE-1)


struct thread_table {
	pthread_t	sys_tid;
	ldthread_t	*usr_handle;	
};

struct circular_queue {
	unsigned long element[MAX_CIRCULAR_QUEUE_SIZE];
	unsigned int  front, rear;
};

static struct thread_table thd_tbl[MAX_NR_THREADS];
static struct circular_queue lock_cq;
static unsigned int lockdep_dependency_gen_id;
FILE *fp;

void _init() 
{
	fp = fopen("lockdep.log","w"); /* open for writing */
	fprintf(fp,"*************************************************************\n");	
	fprintf(fp,"Lock dependency validator: 2012 Sanketh Nalli\n");

	fprintf(fp,"... MAX_LOCK_DEPTH:          %lu\n", MAX_LOCK_DEPTH);
	fprintf(fp,"... MAX_LOCKDEP_ENTRIES:     %lu\n", MAX_LOCKDEP_ENTRIES);
	fprintf(fp,"... MAX_NR_THREADS:          %lu\n", MAX_NR_THREADS);


	fprintf(fp," memory used by lock dependency info: %lu bytes\n",
		(sizeof(lock_list) * MAX_LOCKDEP_ENTRIES +
		sizeof(struct circular_queue) +
		sizeof(struct thread_table) * MAX_NR_THREADS
		)
		);

	fprintf(fp," per thread memory footprint: %d bytes\n",
		sizeof(ldthread_t));
	fprintf(fp,"*************************************************************\n");	
} 
void _fini() 
{
	fclose(fp);
}

static inline void __cq_init(struct circular_queue *cq)
{
	cq->front = cq->rear = 0;
	lockdep_dependency_gen_id++;
}

static inline int __cq_empty(struct circular_queue *cq)
{
	return (cq->front == cq->rear);
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

static inline void mark_lock_accessed(lock_list *lock)
{
	unsigned long nr;

	nr = lock - list_entries;
	lock->dep_gen_id = lockdep_dependency_gen_id;
}

static inline unsigned long lock_accessed(lock_list *lock)
{
	unsigned long nr;

	nr = lock - list_entries;
	return lock->dep_gen_id == lockdep_dependency_gen_id;
}

static int __bfs(lock_list *source, ldthread_mutex_t *target)
{

	lock_list *entry;
	struct list_head *head;
	lock_list *lock;
	unsigned int cq_depth;
	struct circular_queue *cq = &lock_cq;
	int ret = 0;

	head = &source->lock->child;

	if (list_empty(head))
		return 0;

	__cq_init(cq);
	__cq_enqueue(cq, (unsigned long)source); 

	while (!ret) {

		ret = __cq_empty(cq); 
		if (!ret) {

			__cq_dequeue(cq, (unsigned long *)&lock); 

			head = &lock->lock->child;

			if (list_empty(head)) 
				continue;
		
			list_for_each_entry(entry, head, sibling) {

				if (!lock_accessed(entry)) {

					mark_lock_accessed(entry);

					if (entry->lock == target) {
						return 1;
					}

					__cq_enqueue(cq, (unsigned long)entry);

				}
				
				if (entry->sibling.next == head) 
					break;
			}
		}
	}
	return 0;
}

static inline int __bfs_forwards (lock_list *source, ldthread_mutex_t *target)
{
	return __bfs(source, target);
}

/*
 * Prove that the dependency graph starting at <entry> can not
 * lead to <target>. Print an error and return 0 if it does.
 */
static int check_noncircular (lock_list *source, ldthread_mutex_t *target)
{
	int result;

	result = __bfs_forwards(source, target);

	return result;
}

/*
 * Allocate a lockdep entry. (assumes the graph_lock held, returns
 * with NULL on failure)
 */
static lock_list *alloc_list_entry(void)
{
/*	if (nr_list_entries >= MAX_LOCKDEP_ENTRIES) {
		if (!debug_locks_off_graph_unlock())
			return NULL;

		fprintf(fp,"BUG: MAX_LOCKDEP_ENTRIES too low!\n");
		fprintf(fp,"turning off the locking correctness validator.\n");
		dump_stack();
		return NULL;
	}
*/	return list_entries + nr_list_entries++;
}

/*
 * Add a new dependency to the head of the list:
 */
static int add_lock_to_list(held_lock *prev,  held_lock *next)
{
	lock_list *entry;

	/*
	 * Lock not present yet - get a new dependency struct and
	 * add it to the list:
	 */
	entry = alloc_list_entry();
/*	if (!entry)
		return 0;
*/
	INIT_LIST_HEAD(&entry->sibling);
	entry->lock = next->lock;
	entry->dep_gen_id = 0;
	/*
	 * Since we never remove from the dependency list, the list can
	 * be walked lockless by other CPUs, it's only allocation
	 * that we need to take care of.
	 */
	list_add_tail(&entry->sibling, &prev->lock->child);

	return 1;
}

static inline void print_lock(held_lock *hlock)
{
	fprintf(fp,"(%s) ",hlock->lock->name);
	fprintf(fp,", at: %d, %s, %s", hlock->LINE, hlock->FUNC, hlock->SRC_FILE);
}

static int print_circular_bug(ldthread_t *curr, held_lock *prev, held_lock *next)
{
	
	lock_list *parent;
	lock_list *first_parent;
	int depth;

	fprintf(fp,"\n=======================================================\n");
	fprintf(fp,  "[ INFO: possible circular locking dependency detected ]\n");
	fprintf(fp,  "-------------------------------------------------------\n");
	fprintf(fp,"%s/%lu is trying to acquire lock:\n",
		curr->name,pthread_self());
	print_lock(next);
	fprintf(fp,"\nbut task is already holding lock:\n");
	print_lock(prev);
	fprintf(fp,"\nwhich already depends on the new lock.\n\n");
//	fprintf(fp,"\nthe existing dependency chain (in reverse order) is:\n");
/**
	if (!debug_locks_off_graph_unlock() || debug_locks_silent)
		return 0;

	if (!save_trace(&this->trace))
		return 0;

	depth = get_lock_depth(target);

	print_circular_bug_header(target, depth, check_src, check_tgt);

	parent = get_lock_parent(target);
	first_parent = parent;

	while (parent) {
		print_circular_bug_entry(parent, --depth);
		parent = get_lock_parent(parent);
	}

	fprintf(fp,"\nother info that might help us debug this:\n\n");
	print_circular_lock_scenario(check_src, check_tgt,
				     first_parent);

	lockdep_print_held_locks(curr);

	fprintf(fp,"\nstack backtrace:\n");
	dump_stack();
*/
	return 0;
}

static int
print_deadlock_bug(ldthread_t *curr, held_lock *next)
{
	printf("\n=============================================\n");
	printf("[ INFO: certain recursive locking detected ]\n");
	printf("---------------------------------------------\n");
	printf("%s/%lu is trying to acquire lock:\n",
		curr->name,pthread_self());
	printf("(%s) ",next->lock->name);
	printf(", at: %d, %s, %s", next->LINE, next->FUNC, next->SRC_FILE);
	printf("\nbut task is already holding it.\n");

/**
	fprintf(fp,"\nother info that might help us debug this:\n");
	print_deadlock_scenario(next, prev);
	lockdep_print_held_locks(curr);

	fprintf(fp,"\nstack backtrace:\n");
	dump_stack();
*/
	_fini();
	return 0;
}

static int
check_prev_add(ldthread_t *curr, held_lock *prev,
	        held_lock *next)
{

	lock_list source;
	int ret;

	/*
	 * Prove that the new <prev> -> <next> dependency would not
	 * create a circular dependency in the graph. (We do this by
	 * forward-recursing into the graph starting at <next>, and
	 * checking whether we can reach <prev>.)
	 */
	source.lock = next->lock;
	ret = check_noncircular(&source, prev->lock);
	if (ret)
		print_circular_bug(curr, prev, next);

	/*
	 * Is the <prev> -> <next> dependency already present?
	 *
	 * (this may occur even though this is a new chain: consider
	 *  e.g. the L1 -> L2 -> L3 -> L4 and the L5 -> L1 -> L2 -> L3
	 *  chains - the second one will be new, but L1 already has
	 *  L2 added to its dependency list, due to the first chain.)
	 */
	 lock_list *entry; 
	list_for_each_entry(entry, &prev->lock->child, sibling) {
		if (source.lock == entry->lock) {
			return 2;
		}
		
		if (entry->sibling.next == &prev->lock->child)
			break;
	}
	 
	/*
	 * Ok, all validations passed, add the new lock
	 * to the previous lock's dependency list:
	 */
	ret = add_lock_to_list(prev, next);

	fprintf(fp,"\nnew dependency:\n");
	print_lock(prev);
	fprintf(fp," => \n");
	print_lock(next);
	fprintf(fp,"\nby: %s/%lu \n",
		curr->name,pthread_self());
	fprintf(fp,"\n");


	return 1;
}

static int
check_prevs_add(ldthread_t *curr, held_lock *next)
{
	int depth = curr->lockdep_depth;
	held_lock *hlock;

		hlock = curr->held_locks + depth-1;
		/*
		 * Only non-recursive-read entries get new dependencies
		 * added:
		 */
			if (!check_prev_add(curr, hlock, next))
				return 0;

	return 1;
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
check_deadlock(ldthread_t *curr, held_lock *next)
{
	held_lock *prev;
	int i;

	for (i = 0; i < curr->lockdep_depth; i++) {
		prev = curr->held_locks + i;

		if (prev->lock != next->lock)
			continue;

		return print_deadlock_bug(curr, next);
	}
	return 1;
}

static int validate_chain(ldthread_t *curr,
		 held_lock *hlock)
{
	/*	if (!graph_lock())
			return 0;
		/*
		 * Check whether the new lock's dependency graph
		 * could lead back to the previous lock.
		 */
	
		if (!check_deadlock(curr, hlock))
			return 0;
		/*
		 * Add dependency only if this lock is not the head
		 * of the chain, and if it's not a secondary read-lock:
		 */

		if (!check_prevs_add(curr, hlock))
			return 0;
		//graph_unlock();

	return 1;
}

static ldthread_t * current()
{
	int i;
	pthread_t curr = pthread_self();
	for (i = 0; i < nr_table_entries; i++) {

		if (pthread_equal(curr, thd_tbl[i].sys_tid)){ 
//		fprintf(fp,"\nmatch %d\n",i);
			return thd_tbl[i].usr_handle; }
	}
}
static int lock_acquire(ldthread_mutex_t *lock, int line, char *func, char *file)
{
/**
 *	Take care of current macro
 */
	ldthread_t *curr = current();
	held_lock *hlock;
	unsigned int depth;
	int chain_head = 0;

	/*
	 * Add the lock to the list of currently held locks.
	 * We dont increase the depth just yet, up until the
	 * dependency checks are done.
	 */
	depth = curr->lockdep_depth;

	hlock = curr->held_locks + depth;
	hlock->lock = lock;
	hlock->LINE = line;
	hlock->FUNC = func;
	hlock->SRC_FILE = file;

/**
	/* mark it as used: 
	if (!mark_lock(curr, hlock, LOCK_USED))
		return 0;
*/
	if (depth) 
		if (!validate_chain(curr, hlock))
			return 0;

	curr->lockdep_depth++;
	return 1;
}

static int lock_release(ldthread_mutex_t *lock, int line, char *func, char *file)
{ 
	ldthread_t *curr = current();

	if (!curr->lockdep_depth) {
	printf("\n=================================\n");
	printf("[ BUG: bad contention detected! ]\n");
	printf("---------------------------------\n");
	printf("%s/%lu is trying to release a lock :\n",
		curr->name,pthread_self());
	printf(", at: %d, %s, %s", line, func, file);
	printf("\nbut there are no locks held!\n");
	return 0;
	
	} else {

		held_lock *prev;
		int i;
		for (i = 0; i < curr->lockdep_depth; i++) {
			prev = curr->held_locks + i;

			if (prev->lock != lock)
				continue;

			break;
		}
		
		if ( i == curr->lockdep_depth ) {
			printf("\n=================================\n");
			printf("[ BUG: bad contention detected! ]\n");
			printf("---------------------------------\n");
			printf("%s/%lu is trying to release lock :\n",
				curr->name,pthread_self());
			printf(", at: %d, %s, %s", line, func, file);
			printf("\nbut it does not hold it!\n");
			return 0;
		}
	}
	curr->lockdep_depth--;
	return 1; 
}

int ldthread_join( ldthread_t thread, void **value_ptr)
{
	pthread_join(thread.thread, value_ptr);
}

static void *ld_init (void *thread)
{
	pthread_mutex_lock(&spawn_lock);
	// Register yourself in the table using pthread_self
	thd_tbl[nr_table_entries].sys_tid = pthread_self();
	thd_tbl[nr_table_entries++].usr_handle = (ldthread_t *)thread;
	pthread_mutex_unlock(&spawn_lock);
	((ldthread_t *)thread)->start_func(((ldthread_t *)thread)->arg);
}

int ldthread_create( ldthread_t *thread, const ldthread_attr_t *attr, void *(*start_routine)(void*), void * arg, char *name)
{
	thread->lockdep_depth = 0;
	thread->name = name;
	thread->start_func = start_routine;
	thread->arg = arg;

	return pthread_create(&thread->thread, &attr->attr, ld_init, (void *)thread);
	//return pthread_create(&thread->thread, &attr->attr, start_routine, arg);
}

int ldthread_mutex_unlock(ldthread_mutex_t *mutex, int line, char *func, char *file)
{
	
	lock_release(mutex, line, func, file);	
	return pthread_mutex_unlock(&mutex->lock);
}

int ldthread_mutex_lock(ldthread_mutex_t *mutex, int line, char *func, char *file)
{
	pthread_mutex_lock(&graph_lock);
	lock_acquire(mutex, line, func, file);	
	pthread_mutex_unlock(&graph_lock);
	
	return pthread_mutex_lock(&mutex->lock);
}

int ldthread_mutex_init(ldthread_mutex_t *mutex,
              const  ldthread_mutexattr_t *attr, char *name)
{
	INIT_LIST_HEAD(&mutex->child);
	mutex->name = name;
	return pthread_mutex_init(&mutex->lock, &attr->attr);
}
              
