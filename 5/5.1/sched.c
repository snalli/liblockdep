#include "sched.h"
#include <stdio.h>
#include <pthread.h>

struct task_struct *current;
int thread_create(struct task_struct *task, void *func)
{
	task->lockdep_depth = 0;
	task->lockdep_recursion = 0;
	return pthread_create(&task->thread, NULL, func, (void *)task);
}
