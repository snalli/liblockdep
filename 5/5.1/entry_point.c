#include "sched.h"
#include "mutex.h"
#include <stdio.h>

struct mutex a_lock,b_lock,c_lock, d_lock;
static int a;	
unsigned int b = 100000;
unsigned int c = 200000;

/**
 * Recursive locking
 */
void *func0(void * arg,...)
{
	unsigned int x = 100000;
	
	mutex_lock(&a_lock);
	
		mutex_lock(&a_lock);

			while (x--) 
				{ ++a; }
	
	mutex_unlock(&a_lock);

	return;
}

/**
 * A-B-B-A dependency
 */

/* A => B dependency */
void *func1(void * arg,...)
{

	mutex_lock(&a_lock);
	
		mutex_lock(&b_lock);

			while (b--) 
				{ ++a; }
	
		mutex_unlock(&b_lock);
		
	mutex_unlock(&a_lock);

	return;
}

/* B => A dependency */
void *func2(void * arg,...)
{

	mutex_lock(&b_lock);
	
		mutex_lock(&a_lock);

			while (a--) 
				{ ++b; }

		mutex_unlock(&a_lock);

	mutex_unlock(&b_lock);

	return;
}


/**
 * A-B-C-D dependency
 */
/* A => B dependency */
void *func3(void * arg,...)
{

	mutex_lock(&a_lock);
	
		mutex_lock(&b_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
			 
		mutex_unlock(&b_lock);
		
	mutex_unlock(&a_lock);

	return;
}
/* B => C dependency */
void *func4(void * arg,...)
{

	mutex_lock(&b_lock);
	
		mutex_lock(&c_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}

		mutex_unlock(&c_lock);
		
	mutex_unlock(&b_lock);

	return;
}
/* C => D dependency */
void *func5(void * arg,...)
{

	mutex_lock(&c_lock);

		mutex_lock(&d_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
	
		mutex_unlock(&d_lock);
		
	mutex_unlock(&c_lock);

	return;
}

/* D => A dependency */
void *func6(void * arg,...)
{

	mutex_lock(&d_lock);

		mutex_lock(&a_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
	
		mutex_unlock(&a_lock);
		
	mutex_unlock(&d_lock);

	return;
}

int main()
{

	system("clear");

	mutex_init(&a_lock);
	mutex_init(&b_lock);
	mutex_init(&c_lock);
	mutex_init(&d_lock);

	struct task_struct i_thd;
	struct task_struct j_thd, k_thd;
	struct task_struct l_thd, m_thd, n_thd, o_thd;

	i_thd.tid = 0;
	current = &i_thd;
	thread_create(&i_thd, func0);
	pthread_join(i_thd.thread,NULL);
/**
 * A-B-B-A dependency
 */
	j_thd.tid = 1;
	current = &j_thd;
	thread_create(&j_thd, func1);
	pthread_join(j_thd.thread,NULL);

	k_thd.tid = 2;
	current = &k_thd;
	thread_create(&k_thd, func2);
	pthread_join(k_thd.thread,NULL);

/**
 * A-B-C-D dependency
 */

	l_thd.tid = 3;
	current = &l_thd;
	thread_create(&l_thd, func3);
	pthread_join(l_thd.thread,NULL);

	m_thd.tid = 4;
	current = &m_thd;
	thread_create(&m_thd, func4);
	pthread_join(m_thd.thread,NULL);

	n_thd.tid = 5;
	current = &n_thd;
	thread_create(&n_thd, func5);
	pthread_join(n_thd.thread,NULL);

	o_thd.tid = 6;
	current = &o_thd;
	thread_create(&o_thd, func6);
	pthread_join(o_thd.thread,NULL);

	pthread_exit(NULL);
	return 0;
}

