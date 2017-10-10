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
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint my_pthread_t;

typedef struct threadControlBlock {
	/* add something here */
} tcb; 

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	/* add something here */
} my_pthread_mutex_t;

/* define your data structures here: */

/* Data structure for storing thread information.

This struct will be used in the tcb to store information about threads.
A thread's tid will correspond to its space in an array in the tcb consisting
of my_pthread_info structs. Thread ID #0 will be reserved for the manager
thread, while Threads #1 to the MaxNumThreads-1 will be used for future
thread instances. 

*/
typedef struct my_pthread_t_information {
	/* The thread's ID. 

	TID's will be allocated systematically by
	the manager thread, in ascending order from 1 to the max thread number
	minus one; thread #0 will be reserved for the manager thread itself.
	When the manager thread runs over the max threads allocated, it will go
	into a linked list of destroyed thread ID's and recycle a thread ID and
	its corresponding space in the my_pthread_info array. 

	*/
	my_pthread_t tid;

	/* The number of time slices/quanta left for this thread.

	Time slices/quanta are allocated by the manager thread to individual threads by
	their priority level. This member should become important only when the thread 
	is in the run queue.

	*/
	uint timeSlices;

	/* TODO @all: add any members pertinent to the design.*/


} my_pthread_info;

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
