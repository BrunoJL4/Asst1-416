// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	//call drug_check() - check that masterthread exists	
	//if doesn't exist
	//call master_thread() - master thread is the 'gatekeeper' & schedules performing maintenence 		
	return 0;
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
    
    //set value_ptr
    //take job off MLPQ
    
}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	//Does target thread exist? - terrorist_check()
    //if(target thread been previously joined)
        //creates undefined behavior - manpages
    
    /* ACTUAL WORK */
    
    //target thread HAS TO FINISH RUNNING before caller gets anymore cpu time - how to implement?
    //i think above requires a context switch to target thread at this point
    
    //if(Target thread was cancelled)
        //*RETVAL = PTHREAD_CANCEL
        //return -1; => is this considered an error?
    //if(exit's retval != null)
        //value_ptr = exits retval's address;
    return 0; //success
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	return 0;
}

