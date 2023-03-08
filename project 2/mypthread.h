// File:	mypthread_t.h

// List all group member's name: Paul Kim
// username of iLab: obawolo 
// iLab Server: cp.csrutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <malloc.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

typedef uint mypthread_t;

typedef struct sigaction mySig;

// thread status numbers
#define READY 0
#define YIELD 1
#define WAIT 2
#define EXIT 3
#define JOIN 4
#define MUTEX_WAIT 5

typedef struct threadControlBlock {

	/* add important states in a thread control block */

	pthread_t tid;	// thread Id
	unsigned int priority;	// thread priority
	int status;	// thread status
	ucontext_t *context;	// thread context
	struct list *joinQueue;	// thread stack
	void *jVal;
    void *retVal;

} tcb;

//L: Linked list for the thread stack
typedef struct list
{
  tcb* thread;
  struct list* next;
}list;

/* mutex struct definition */
typedef struct mypthread_mutex_t {
	/* add something here */

	int locked;
	int available;
	int holder;
	int initialized;
	list* queue;

} mypthread_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

/* Function Declarations: */

//L: queue functions
void enqueue(list**, tcb*);
tcb* dequeue(list**);

//L: linked list functions
void l_insert(list**, tcb*);
tcb* l_remove(list**);

//L: table functions
tcb* thread_search(my_pthread_t);

//L: thread Queue Startup
void initializeQueues(list**);

//L: maintenance: boost thread priorities
void maintenance();

//L: free threads that don't exit properly
void garbage_collection();

// thread Context startup
void initializeMainContext();

// thread Cleaner startup
void initializeGarbageContext();

// singal handler
void scheduler(int signum);

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initial the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
