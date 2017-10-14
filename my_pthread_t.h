// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* include lib header files that you need here: */
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint my_pthread_t;

/* Our own enums describing thread status. */
enum threadStatus {
	THREAD_RUNNING = 0,
	THREAD_READY = 1,
	THREAD_BLOCKED = 2
	THREAD_WAITING = 3,
	THREAD_DONE = 4,
	THREAD_INTERRUPTED = 5
};

/* Our own enums describing mutex lock status. */
enum lockStatus {
	LOCKED = 0,
	UNLOCKED = 1
};


/* Data structure for storing thread information.

This struct is used for each thread we have, to store information
about it.

*/
typedef struct threadControlBlock {
	/* The thread's current status. It tells us whether the thread
	is currently running, whether it's ready to run but isn't, 
	or whether it's blocked. Can add additional enums
	as we go forward. */
	threadStatus status;

	/* The thread's ID. Due to structure for storing tcb's
	inherently being the thread's ID, this might be redundant.*/
	my_pthread_t tid;

	/* Pointer to the stack this thread runs on. This is not
	specific to the thread, as other threads may run on
	the same stack. May be redundant with context here.*/
	stack_t stack;

	/* The context this thread runs on. This is specific to
	the thread, whereas multiple threads may share a stack.*/
	ucontext_t context;

	/* The number of time slices allocated to the thread.
	This is zero by default, and is allocated during the
	maintenance cycle.*/
	unsigned int timeSlices;

	/* The thread's current priority. 0 by default, but priority
	level is decreased (the priority going from 0 to 1, 1 to 2,
	and so on) as the thread is interrupted/preempted more often*/
	unsigned int priority;

} tcb; 


/* Mutex struct definition */
typedef struct my_pthread_mutex_t {
	/* UNLOCKED or LOCKED */
	lockStatus status;

	/* Threads waiting for this lock */
	pnode *waitQueue;
	
	/* Current thread that owns this lock */
	my_pthread_t ownerID;
	
	/* Mutex attribute */
	const pthread_mutexattr_t *attr;
	
} my_pthread_mutex_t;


/* define your data structures here: */


/* Data structure for linked lists of my_pthread_t values.

This struct will be used in the tcb to store linked lists of my_pthread_t's, or
Thread ID's/TID's for short. The list of usages of pnodes is as follows:

1. Each "bucket" of the MLPQ
2. The run queue in the manager thread
3. The "recyclable thread ID" list used in the manager thread

 */
typedef struct my_pthread_node {
	/* The ID of the thread being referenced by this pnode. */
	my_pthread_t tid;

	/* The next pnode in the list. */
	my_pthread_node *next;


} pnode;


// Feel free to add your own auxiliary data structures


/* Function Declarations: */

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);


/* Our own functions below */

/* Initializes the manager thread, with the user's calling function
being saved as a child thread which is itself managed by the 
manager thread.*/
int init_master_thread();

/* Carries out the manager thread responsibilities.
Makes use of runQueueHelper() and maintenanceHelper() in order
to make debugging more modular. */
int my_pthread_manager();

/* This function is the helper function which performs most of
the work for the manager thread's run queue. */
void runQueueHelper();

/* Helper function which performs most of the work for
the manager thread's maintenance cycle. */
void maintancehelper();

/* Creates a new tcb instance. */
tcb *createTcb(threadStatus status, my_pthread_t id, stack_t stack, 
	ucontext_t context, unsigned int timeSlices);

/* Returns a pointer to a new pnode instance. */
pnode *createPnode(my_pthread_t tid);

/* Inserts a given pnode into a given level of the MLPQ, such
that it is the last node in that level's list (or first, if no others12
are present). */
int insertPnode(pnode *input, unsigned int level);

//TODO @all: add macro for run queue alarm signal handler.

#endif
