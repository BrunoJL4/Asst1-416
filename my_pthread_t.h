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
	the same stack.*/
	void *stackPtr;

	/* The context this thread runs on. This is specific to
	the thread, whereas multiple threads may share a stack.*/
	ucontext_t context;

	/* The number of time slices allocated to the thread.
	This is zero by default, and is allocated during the
	maintenance cycle.*/
	unsigned int timeSlices;

	/* TODO @all: Add anymore members to this struct
	which might be necessary. */


} tcb; 


/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	/* add something here */
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


/* WE THOUGHT THESE WERE NECESSARY, JUST SEPERATING THEM FROM WHAT WAS GIVEN */

/* intializes manager thread to oversee progress of other threads
   return: 0 - not init, return 1 - init */
int init_master_thread();

/* check masterthreads exists 
   return: 0 - does not exists, return 1 - exists */
int drug_check();

/* check thread exists
   return: 0 - does not exists, return 1 - exists */
int terrorist_check(my_pthread_t thread);

#endif
