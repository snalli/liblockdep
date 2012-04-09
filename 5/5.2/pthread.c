#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "lockdep.h"
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <fcntl.h>
#include <string.h>

#define NR_CPU		1
#define NR_GROUPS	1
#define NR_THREADS 	5
#define DURATION	1e+6	//in micro seconds
#define MAX_DATA_SIZE 	4
#define MAXCOUNT	100000

int ncpu;
int group_size;
int ngroups;
int shared_data_size;
int duration;
int test_stop = 0;
char mode;

pthread_mutex_t lock;// = PTHREAD_MUTEX_INITIALIZER;


struct thread {
	int gid;
	pthread_t tid;
	long loop_count;
};

struct group {
	struct thread *tids;
	unsigned int *data;
	pthread_mutex_t data_lock;
} *grp;

int maxcount = MAXCOUNT;
int counter;

inline pid_t gettid()
{
	return syscall(__NR_gettid);
}

void usage(char *cmd)
{
	printf("Usage: %s [options]\n"
		"-c\t\tNo. of cpus (default: 2)\n"
		"-t\t\tSize of each thread group (default: 10)\n"
		"-g\t\tNo. of thread groups (default: 2)\n"
		"-d\t\tSize of shared data in bytes (default: 4)\n"
		"-u\t\tDuration of test run in us\n"
		"-m\t\tDuration of test run in ms\n"
		"-s\t\tDuration	of test run in s\n"
		"-M\t\tMode of test run (default: normal)\n"
		"Note: If more than one option for test duration are specified then"
		"the value of last option will be considered\n", cmd);
}

void read_options(int argc, char *argv[]) {
	int option;

	ncpu = NR_CPU;
	group_size = NR_THREADS;
	ngroups = NR_GROUPS;
	shared_data_size = MAX_DATA_SIZE;
	duration = DURATION;
	
	while((option = getopt(argc, argv, "c:t:g:d:u:m:s:M:")) != -1)
	{
		switch(option)
		{
			case 'c':
				ncpu = atoi(optarg);	
				break;
			case 't':
				group_size = atoi(optarg);
				break;
			case 'g':
				ngroups = atoi(optarg);
				break;
			case 'd':
				shared_data_size = atoi(optarg);
				break;
			case 'u':
				duration = atoi(optarg);
				break;
			case 'm':
				duration = atoi(optarg) * 1000;
				break;
			case 's':
				duration = atoi(optarg) * DURATION;
				break;
			case 'M':
				mode = optarg[0];
				if(mode != 'b' && mode != 'n')
				{
					goto out;
				}
				break;
			default:
				goto out;
		}
	}
	return;
out:
	printf("Invalid option specified\n");
	usage(argv[0]);
	exit(1);
}

static inline void set_group(pid_t tid, char *group)
{
	int fd, rc;
	char buf[100];
	char pid[20];

	sprintf(buf, "/dev/cgroup/%s/tasks", group);
	fd = open(buf, O_RDWR);
	if (fd < 0)
		perror("open"), exit(1);

	sprintf(pid, "%d", tid);
	rc = write(fd, pid, strlen(pid));
	if (rc < 0)
		perror("write"), exit(1);

	close(fd);
}

pthread_mutex_t atomic_lock;// = PTHREAD_MUTEX_INITIALIZER;

int ready_count = 0;

static inline void atomic_inc(int *x)
{
	pthread_mutex_lock(&atomic_lock);
	*x++;
	pthread_mutex_unlock(&atomic_lock);
}

static inline int atomic_read(int *x)
{
	return *x;
}

void *foo(void *arg)
{
	struct thread *t;
	int gid;
	pid_t tid;

	t = (struct thread *)arg;
	gid = t->gid;
	tid = gettid();
/*	if(mode == 'b')
	{
		cpu_set_t mask;
		int rc;

		CPU_ZERO(&mask);
		if (gid == 0) {
			CPU_SET(0, &mask);
			CPU_SET(1, &mask);
			CPU_SET(2, &mask);
			CPU_SET(3, &mask);
			CPU_SET(4, &mask);
			CPU_SET(5, &mask);
			CPU_SET(6, &mask);
			CPU_SET(7, &mask);
			CPU_SET(8, &mask);
			CPU_SET(9, &mask);
			CPU_SET(10, &mask);
			CPU_SET(11, &mask);
			CPU_SET(12, &mask);
			CPU_SET(13, &mask);
			CPU_SET(14, &mask);
			CPU_SET(15, &mask);
			CPU_SET(16, &mask);
			CPU_SET(17, &mask);
			CPU_SET(18, &mask);
			CPU_SET(19, &mask);
			CPU_SET(20, &mask);
			CPU_SET(21, &mask);
			CPU_SET(22, &mask);
			CPU_SET(23, &mask);

		} else {
			CPU_SET(24, &mask);
			CPU_SET(25, &mask);
			CPU_SET(26, &mask);
			CPU_SET(27, &mask);
			CPU_SET(28, &mask);
			CPU_SET(29, &mask);
			CPU_SET(30, &mask);
			CPU_SET(31, &mask);
			CPU_SET(32, &mask);
			CPU_SET(33, &mask);
			CPU_SET(34, &mask);
			CPU_SET(35, &mask);
			CPU_SET(36, &mask);
			CPU_SET(37, &mask);
			CPU_SET(38, &mask);
			CPU_SET(39, &mask);
			CPU_SET(40, &mask);
			CPU_SET(41, &mask);
			CPU_SET(42, &mask);
			CPU_SET(43, &mask);
			CPU_SET(44, &mask);
			CPU_SET(45, &mask);
			CPU_SET(46, &mask);
			CPU_SET(47, &mask);
		}
		rc = sched_setaffinity(tid, sizeof(cpu_set_t), &mask);
		if (rc < 0)
			perror("sched_setaffinity"), exit(1);
	} else if (mode == 'c') {
		if (gid == 0)
			set_group(tid, "a");
		else
			set_group(tid, "b");
	}
*/
	pthread_mutex_lock(&atomic_lock);
	++ready_count;
	pthread_mutex_unlock(&atomic_lock);

	pthread_mutex_lock(&grp[gid].data_lock);
	while (!test_stop) {

		int i;
		for(i = 0; i < shared_data_size / sizeof(int); i++)
		{
			grp[gid].data[i]++;
			if(grp[gid].data[i] == 8)
				grp[gid].data[i] = 0;
		}
		t->loop_count++;

	}
	pthread_mutex_unlock(&grp[gid].data_lock);
}

void *boo(void *arg)
{

	pthread_mutex_lock(&lock);

	int i,j;
	for (i=0; i < ngroups; i++)
	{
		for(j = 0; j < group_size; j++)
		{
			int *temp = (int *)malloc(sizeof(int));
			*temp = i;
			grp[i].tids[j].gid = i;
			pthread_create(&grp[i].tids[j].tid, NULL, foo, &grp[i].tids[j]);
		}
	}

	while (atomic_read(&ready_count) != (ngroups * group_size))
		;

	pthread_mutex_unlock(&lock);
	usleep(duration);

	test_stop = 1;
}

main(int argc, char *argv[])
{

	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&atomic_lock, NULL);
	
	int i, j;
	struct timeval btv, etv;
	double delta, d1, d2;

	read_options(argc, argv);

	grp = (struct group *)malloc(sizeof(struct group) * ngroups);
	if(!grp)
	{
		perror("malloc");
		exit(1);
	}

	for(i = 0; i < ngroups; i++)
	{
		grp[i].tids = (struct thread *)malloc(sizeof(struct thread)*group_size);
		if (!grp[i].tids)
		{
			perror("malloc");
			exit(1);
		}
		memset(grp[i].tids, 0, sizeof(struct thread)*group_size);
		pthread_mutex_init(&grp[i].data_lock, NULL);
		grp[i].data = (unsigned int *)malloc(sizeof(unsigned int) * shared_data_size / sizeof(int));
		if(!grp[i].data)
		{
			perror("malloc");
			exit(1);
		}
	}

	pthread_t first_child;
	pthread_create(&first_child, NULL, boo, NULL);
	
	usleep(2*duration);
	
	pthread_join(first_child, NULL);
	for (i=0; i < ngroups; i++)
		for(j = 0; j < group_size; j++)
			pthread_join(grp[i].tids[j].tid, NULL);


	long int grp_count, tot_count;
	tot_count = 0;
	for(i = 0; i < ngroups; i++)
	{
		printf("group %d\n", i);
		grp_count = 0;
		for(j = 0; j < group_size; j++)
		{
			grp_count += grp[i].tids[j].loop_count;
			printf("thread %d: %ld\n", j, grp[i].tids[j].loop_count);
		}
		printf("total: %ld\n", grp_count);
		tot_count += grp_count;
		printf("\n");
	}	
	printf("Total count: %ld\n", tot_count);
	pthread_exit(NULL);
	/*d1 = etv.tv_sec + etv.tv_usec*1e-6;
	d2 = btv.tv_sec + btv.tv_usec*1e-6;

	delta = d1 - d2;

	printf ("Time taken = %f seconds \n", delta);
	*/
}
