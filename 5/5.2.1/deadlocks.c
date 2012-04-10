#include <pthread.h>
#include "lockdep.h"
#include <stdio.h>

pthread_t a_lock,b_lock,c_lock, d_lock;
static int a;	
unsigned int b = 100000;
unsigned int c = 200000;

/**
 * Recursive locking
 */
void *func0(void * arg,...)
{
	unsigned int x = 100000;
	
	pthread_mutex_lock(&a_lock);
	
		pthread_mutex_lock(&a_lock);

			while (x--) 
				{ ++a; }
	
	pthread_mutex_unlock(&a_lock);

	return;
}

/**
 * A-B-B-A dependency
 */

/* A => B dependency */
void *func1(void * arg,...)
{

	pthread_mutex_lock(&a_lock);
	
		pthread_mutex_lock(&b_lock);

			while (b--) 
				{ ++a; }
	
		pthread_mutex_unlock(&b_lock);
		
	pthread_mutex_unlock(&a_lock);

	return;
}

/* B => A dependency */
void *func2(void * arg,...)
{

	pthread_mutex_lock(&b_lock);
	
		pthread_mutex_lock(&a_lock);

			while (a--) 
				{ ++b; }

		pthread_mutex_unlock(&a_lock);

	pthread_mutex_unlock(&b_lock);

	return;
}


/**
 * A-B-C-D dependency
 */
/* A => B dependency */
void *func3(void * arg,...)
{

	pthread_mutex_lock(&a_lock);
	
		pthread_mutex_lock(&b_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
			 
		pthread_mutex_unlock(&b_lock);
		
	pthread_mutex_unlock(&a_lock);

	return;
}
/* B => C dependency */
void *func4(void * arg,...)
{

	pthread_mutex_lock(&b_lock);
	
		pthread_mutex_lock(&c_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}

		pthread_mutex_unlock(&c_lock);
		
	pthread_mutex_unlock(&b_lock);

	return;
}
/* C => D dependency */
void *func5(void * arg,...)
{

	pthread_mutex_lock(&c_lock);

		pthread_mutex_lock(&d_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
	
		pthread_mutex_unlock(&d_lock);
		
	pthread_mutex_unlock(&c_lock);

	return;
}

/* D => A dependency */
void *func6(void * arg,...)
{

	pthread_mutex_lock(&d_lock);

		pthread_mutex_lock(&a_lock);

		/*
		 * Do some computation
		 */
		unsigned int x = 100000;
		while (x--) {}
	
		pthread_mutex_unlock(&a_lock);
		
	pthread_mutex_unlock(&d_lock);

	return;
}

int main()
{

	system("clear");

	pthread_mutex_init(&a_lock, NULL);
	pthread_mutex_init(&b_lock, NULL);
	pthread_mutex_init(&c_lock, NULL);
	pthread_mutex_init(&d_lock, NULL);

	pthread_t i_thd;
	pthread_t j_thd, k_thd;
	pthread_t l_thd, m_thd, n_thd, o_thd;

// Recursive locking

//	pthread_create(&i_thd, func0);

/**
 * A-B-B-A dependency
 */


	pthread_create(&j_thd,  NULL,func1, NULL);
	pthread_create(&k_thd,  NULL,func2, NULL);


/**
 * A-B-C-D dependency
 */


	pthread_create(&l_thd,  NULL,func3, NULL);
	pthread_create(&m_thd,  NULL,func4, NULL);
	pthread_create(&n_thd,  NULL,func5, NULL);
	pthread_create(&o_thd,  NULL,func6, NULL);


	pthread_exit(NULL);
	return 0;
}

