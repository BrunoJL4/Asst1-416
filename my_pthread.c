// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"
#define MEM 16384 //Amount of memory used for a new context stack
#define NUM_PRIORITY_LEVELS 5 //number of priority levels
#define MAX_NUM_THREADS 64 //max number of threads allowed at once
#define QUANTA_LENGTH 25


/* Define global variables here. */

/* The Multi-Level Priority Queue (MLPQ).

The MLPQ is an array of pnodes. Since each "pnode" will
really just be a pointer to a pnode in this array, MLPQ is
a variable of type pointer-to-a-pnode-pointer, or pnode **MLPQ.

The initialization function for building the tcb will initialize
the MLPQ to an array of an allocated length equal to the number
of priority levels. 5 levels = 5 cells in the array.
*/
pnode **MLPQ;



/* The array that stores pointers to all Thread Control Blocks.

tcbList has one cell allocated per possible tcb. The index of
a cell in tcbList corresponds to the thread's ID; so Thread #200
should be located in tcbList[200]. tcbList will be NULL when
initialized by the manager thread, but otherwise should be treated
as an array of pointers.

*/
tcb **tcbList;


/* This is the Run Queue.

The Run queue, or runQueue, is a linked list of pnodes that
will be run from start to finish. runQueue will be NULL by
default or if no nodes remain to be run. During the maintenance
cycle of the manager thread, runQueue will be populated with
pnodes in the order in which they are run. Threads remaining
in runQueue which haven't run yet when the maintenance cycle
begins, will have their priority increased in that cycle.
It should also be noted that runQueue is populated until
we run out of time slices to allocate to threads...
right now we're looking at 20 time slices, or quanta, of
25ms each.

The initialization function for building the tcb will initialize
runQueue to NULL. */
pnode *runQueue;


/* This is the Recyclable Queue.

The Recyclable Queue, or recyclableQueue, is a linked list of pnodes
that contains all "recyclable" thread ID's. A thread ID is recyclable
when the thread holding that ID has been destroyed and not yet
reused. recyclableQueue will be used once the max number of
threads has been exceeded.
*/
pnode *recyclableQueue;

/* Number of threads created so far.
This will be initialized to 0 when the manager thread is initialized. */
uint threadsSoFar;

/* contexts */
ucontext_t Manager, Main, CurrentContext;

/* info on current thread */

//indicates whether the current thread has explicitly called
//pthread_exit(). 0 if false, 1 if true. used in manager thread
//to determine whether one calls pthread_exit()'s functionality
//on a thread that didn't explicitly call it.
uint current_exited;

/*ID of the currently-running thread. MAX_NUM_THREADS+1 if manager,
otherwise then some child thread. */
my_pthread_t current_thread;

/* The status of the currently-running thread (under the manager).
Will either be THREAD_RUNNING or THREAD_INTERRUPTED. */
int current_status;

/* Boolean 1 if manager thread is active, otherwise 0 as globals
are initialized to by default*/
uint manager_active;

/* Status of currently-running thread. */
enum threadStatus currentStatus;

/* Signal action struct used by runQueueHelper() for alarms. 
Declared up here to prevent allocations from occurring
each time the runQueueHelper() runs.*/
struct sigaction sa;

/* itimerval struct used by runQueueHelper() for alarms.
Declared up here to prevent allocations from occurring
each time the runQueueHelper() runs.*/
struct itimerval timer;

/* End global variable declarations. */

/* my_pthread and mutex function implementations */

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	printf("entered my_pthread_create()!\n");
	testMsg();
	// flag that is 1 if we're initializing the manager thread,
	// 0 if not. we'll use this at the end of the function to decide
	// whether or not to swap contexts back to manager (ONLY swap
	// contexts if we've just initialized the manager!)
	int initializingManager = 0;
	//check that manager thread exists	
	//init if it does not
	if (manager_active != 1) {
		initializingManager = 1;
		init_manager_thread();
	}
	//set information for new child thread's context
	//thread should be ready to run by default
	printf("Setting attributes for new thread!\n");
	int status = THREAD_READY;
	my_pthread_t tid = threadsSoFar;
	// set aside a stack for the user
	char stack[MEM];
	// set the stack pointer for the user's context
	//time slices is 0 by default
	uint timeSlices = 0;
	ucontext_t context;
	//initialize context, fill it in with current
	//context's information
	getcontext(&context);
	// any child context should link back to Manager
	// upon finishing execution/being interrupted or preempted
	context.uc_link = &Manager;
	// set new context's stack to our allocated stack
	context.uc_stack.ss_sp = stack;
	// set new context's stack size to size of allocated stack
	context.uc_stack.ss_size = sizeof(stack);
	//turns out that functions called through pthread always take 0 or 1 arguments
	//therefore the functions called by the user must always take some type of struct (void *) 
	//if they wish to pass multiple args
	printf("Setting args for new thread!\n");
	if (arg == NULL) {
		printf("No args for new thread!\n");
		makecontext(&context, (void*)&function, 0);
	} else {
		printf("One or more args for new thread!\n");
		makecontext(&context, (void*)&function, 1, arg);
	}
	//check if we've exceeded max number of threads
	if (threadsSoFar >= MAX_NUM_THREADS) {
		printf("Exceeded MAX_NUM_THREADS, checking for recyclable TID's\n");
		//if so, check recyclableQueue, return -1 if there are no available thread ID's
		if (recyclableQueue == NULL) {
			printf("No more available thread ID's!\n"); 
			return -1; 
		}
		//remove first available ID from queue
		pnode *ptr = recyclableQueue;
		recyclableQueue = recyclableQueue->next;
		//take the ID
		tid = ptr->tid;
		//free the pnode for the recycled ID
		free(ptr);
		//make a new TCB from the gathered information
		tcb *newTcb = createTcb(status, tid, context.uc_stack, context, timeSlices);
		//change the tcb instance in tcbList[id] to this tcb
		tcbList[(uint) tid] = newTcb;
		// insert a pnode containing the ID at Level 0 of MLPQ
		pnode *node = createPnode(tid);
		insertPnodeMLPQ(node, 0);
		// increment number of threads so far
		threadsSoFar ++;
		*thread = tid;
		return 0;
	}
	// if still using new ID's, just use threadsSoFar as the index and increment it
	if(MLPQ[0] == NULL) {
		printf("MLPQ level 0 is NULL!\n");
	}
	else{
		printf("MLPQ level 0 is NOT NULL!\n");
	}
	printf("Creating newTcb for new thread #%d\n", tid);
	tcb *newTcb = createTcb(status, tid, context.uc_stack, context, timeSlices);
	// add the new tcb to the tcbList at the cell corresponding to its ID
	if(MLPQ[0] == NULL) {
		printf("MLPQ level 0 is NULL!\n");
	}
	else{
		printf("MLPQ level 0 is NOT NULL!\n");
	}
	printf("modifying tcbList with new thread's tcb!\n");
	tcbList[threadsSoFar] = newTcb;
	if(MLPQ[0] == NULL) {
		printf("MLPQ level 0 is NULL!\n");
	}
	else{
		printf("MLPQ level 0 is NOT NULL!\n");
	}
	// create new pnode for new thread
	printf("Creating pnode for new thread #%d\n", tid);
	pnode *node = createPnode(tid);
	// insert new node to Level 0 of MLPQ
	printf("Inserting new thread into MLPQ level 0!\n");
	insertPnodeMLPQ(node, 0);
	// we've added another thread, so increase this
	threadsSoFar ++;
	// if we've just initialized the manager thread, swap to it because
	// we're in the Main context and need to give the Manager control
	if(initializingManager == 1) {
		printf("Just initialized manager thread, swapping context to it.\n");
		current_thread = MAX_NUM_THREADS + 1;	
		swapcontext(&CurrentContext, &Manager);
	}
	//returns the new thread id on success
	*thread = tid;
	printf("finished my_pthread_create()!\n");
	return 0; 
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	printf("entered my_pthread_yield()!\n");
	testMsg();
	//set thread to yield, set current_thread to manager, swap contexts.
	//manager will yield job in stage 1 of maintenance
	tcbList[(uint) current_thread]->status = THREAD_YIELDED;
	current_thread = MAX_NUM_THREADS + 1;
	swapcontext(&CurrentContext, &Manager);
	printf("finished my_pthread_yield()!\n");
	return 1;

}

/* terminate a thread and fill in the value_ptr of the
thread waiting on it, if any */
void my_pthread_exit(void *value_ptr) {
	printf("entered my_pthread_exit()!\n");
	testMsg();
	// create uint version of current thread to reduce casts
    uint current_thread_int = (uint) current_thread;

    // thread that the calling thread is joined to
    my_pthread_t joinedThread = tcbList[current_thread_int]->waitingThread;

    // if the thread has another thread waiting on it (joined this thread),
    // set its valuePtr member accordingly
    if((uint)joinedThread != MAX_NUM_THREADS + 2) {
    	tcbList[joinedThread]->valuePtr = value_ptr;
    }
    
    // swap back to the Manager context
    current_thread = MAX_NUM_THREADS + 1;
    current_exited = 1;
    swapcontext(&CurrentContext, &Manager);
    printf("finished my_pthread_exit()!\n");
}


/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	printf("entered my_pthread_join()!\n");
	testMsg();
     // create uint version of current thread to reduce casts
    uint thread_int = (uint) thread;

    //what if thread doesn't exist?
    if((tcbList[thread_int]) == NULL){
        fprintf(stderr, "pthread_join(): Target thread %d does not exist!\n", thread_int);
        return -1;
    }
  
    // set target thread's waitingThread to this thread
    tcbList[thread_int]->waitingThread = current_thread;

    // set this thread's status to THREAD_WAITING
    tcbList[(uint) current_thread]->status = THREAD_WAITING;

    // set the value_ptr to point to this thread's valuePtr, so that
    // the caller has access to the value. trying to access the
    // target thread's valuePtr might be undefined because it could
    // have been terminated by the manager thread before the user
    // acceses value_ptr.
    *value_ptr = tcbList[(uint) current_thread]->valuePtr;

    // swap back to the manager
    swapcontext(&CurrentContext, &Manager);
     printf("finished my_pthread_join()!\n");
    return 0; // success
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	printf("entered my_mutex_init()!\n");
	testMsg();
	//Check if mutex is initialized
	//if so, return
	if (&mutex != NULL) {
		return -1;
	}
	//otherwise, initialize mutex
	mutex = malloc(sizeof(my_pthread_mutex_t));
	mutex->status = UNLOCKED;
	mutex->waitQueue = NULL;
	mutex->ownerID = -1;
	mutex->attr = attr;
	printf("finished my_mutex_init()\n");
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_lock()!\n");
	testMsg();
	//Call my_pthread_mutex_init
	//calling with NULL attr argument sets the property to default
	my_pthread_mutex_init(mutex, NULL);
	//If mutex is locked, enter waitQueue and yield
	//NOTE: yield should set this thread status to BLOCKED
	if (mutex->status == LOCKED) {
		//Create pnode of current thread
		pnode *new = malloc(sizeof(pnode));
		new->tid = current_thread;
		new->next = NULL;
		//start a waitQueue if it is empty
		if (mutex->waitQueue == NULL) {
			mutex->waitQueue = new;
		//add to the end of the waitQueue
		} else {
			pnode *ptr = mutex->waitQueue;
			while (ptr->next != NULL) {
				ptr = ptr->next;
			}
			ptr->next = new;
		}
		//set thread status to BLOCKED and change context
		tcbList[(uint) current_thread]->status = THREAD_BLOCKED;
		//let the manager continue in the run queue
		current_thread = MAX_NUM_THREADS + 1;
		setcontext(&Manager);
	} 
	//continue running after the end of yielding OR did not have to yield
	//Set mutex value to locked
	mutex->status = LOCKED;
	//Set mutex owner to current thread
	mutex->ownerID = current_thread;
	printf("finished my_pthread_mutex_lock()!\n");
	return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_unlock()!\n");
	testMsg();
	//If mutex is NOT initialized
	//user did something bad
	if (&mutex == NULL) {
		return -1;
	//Elif mutex does not belong to us
	//we can't unlock it
	} else if (mutex->ownerID != current_thread) {
		return -1;
	}
	//otherwise unlock mutex
	mutex->status = UNLOCKED;
	//Check waiting queue, destroy mutex if there is no more use
	if (mutex->waitQueue == NULL) {
		my_pthread_mutex_destroy(mutex);
		return 0;
	}
	//alert the next available thread & remove it from queue/add back to run queue
	pnode *ptr = mutex->waitQueue;
	mutex->waitQueue = mutex->waitQueue->next;
	//make this thread ready so it can now acquire this lock
	tcbList[(uint) ptr->tid]->status = THREAD_READY;
	free(ptr);
	printf("finished my_pthread_mutex_unlock()\n");
	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_destroy()!\n");
	testMsg();
	//If mutex is NOT initialized
	if (&mutex == NULL) {
		return -1;
	//Elif mutex is locked
	} else if (mutex->status == LOCKED) {
		return -1;
	}	
	//otherwise, free the memory used
	free(mutex);
		
	return 0;
}


/* Essential support functions go here (e.g: manager thread) */

/* Carries out the manager thread responsibilities.
Returns 0 on failure, 1 on success. */
int my_pthread_manager() {
	printf("entered my_pthread_manager()!\n");
	testMsg();
	// check if manager is still considered "active"
	while(manager_active == 1) {
		// perform maintenance cycle
		if(maintenanceHelper() != 0) {
			printf("Error in maintenanceHelper!\n");
			return -1;
		}
		// perform run queue functions
		if(runQueueHelper() != 0) {
			printf("Error in runQueueHelper!\n");
			return -1;
		}
	}
	// We only reach this point when maintenanceHelper()
	// has set manager_active to 0. Leave the function.
	return 0;
}


/* Helper function which performs most of the work for
the manager thread's maintenance cycle. Returns 0 on failure,
1 on success.*/
int maintenanceHelper() {
	printf("entered maintenanceHelper()!\n");
	testMsg();
	// first part: clearing the run queue, and performing
	// housekeeping depending on the thread's status
	pnode *currPnode = runQueue;
	printf("going into first part loop\n");
	while(currPnode != NULL) {
		printf("setting variables for current node in runQueue\n");
		my_pthread_t currId = currPnode->tid;
		tcb *currTcb = tcbList[(uint)currId];
		// if a runQueue thread's status is THREAD_DONE:
		if(currTcb->status == THREAD_DONE) {
			printf("runQueue thread's status is THREAD_DONE\n");
			// deallocate the thread's tcb through tcbList
			free(currTcb);
			// set tcbList[tid] to NULL
			tcbList[(uint)currId] = NULL;
			// then deallocate its pnode in the run queue while
			// moving currPnode to the next node.
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			free(temp);
			// TODO @bruno: add functionality for storing the number of cycles a
			// thread has waited in the MLPQ to get a chance to run. increase
			// the thread's priority if it's waited x cycles. we will set the
			// cycles to 0 once a thread is added to the runQueue, and will
			// increase the cycles of each THREAD_READY thread by 1 each time
			// we look through the MLPQ.
		}
		// if a runQueue thread's status is THREAD_INTERRUPTED:
		else if(currTcb->status == THREAD_INTERRUPTED) {
			printf("runQueue thread's status is THREAD_INTERRUPTED\n");
			// we insert the thread back into the MLPQ but at one lower
			// priority level, also changing its priority member.
			// then change its status to READY.
			currTcb->priority ++;
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			insertPnodeMLPQ(temp, currTcb->priority);
			currTcb->status = THREAD_READY;
		}
		// if a runQueue thread is waiting or yielding
		else if(currTcb->status == THREAD_WAITING || currTcb->status == THREAD_YIELDED) {
			printf("runQueue thread's status is WAITING or YIELDING\n");
			// put the thread into the MLPQ at the same priority level,
			// so that it can resume in subsequent runs when it's
			// set to READY as the thread it's waiting on finishes
			// execution.
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			insertPnodeMLPQ(temp, currTcb->priority);
		}
		// if a runQueue thread's status isn't any of the four above:
		else{
			printf("Error! Thread in runQueue found to have invalid status during maintenance cycle.\n");
			return -1;
		}
	}

	// second part: populating the run queue and allocating time slices.
	// go through MLPQ, starting at highest priority level and going
	// down until we've given out time slices, putting valid threads
	// into the run queue and setting their time slices accordingly.
	// a "valid" thread is one that is READY; any other thread status
	// is invalid and not ready to go in the run queue.
	int timeSlicesLeft = 20;
	int i;
	printf("going into part 2 loop\n");
	for(i = 0; i < sizeof(MLPQ); i++) {
		printf("current level in part 2 loop: %d\n", i);
		// formula for priority levels v. time slices: 2^(level)
		int numSlices = level_slices(i);
		// if we don't have enough timeSlices left to distribute to any node in
		// the current level, break (prevents searching further levels)
		if(numSlices > timeSlicesLeft) {
			break;
		}
		printf("setting variables to go through queue at level: %d\n",i);
		// go through this level's queue, if at all applicable.
		pnode *currPnode = MLPQ[i];
		pnode *prev = currPnode;
		printf("beginning to run through queue\n");
		while(currPnode != NULL) {
			// don't search the current level further if not enough
			// time slices are left.
			if(numSlices > timeSlicesLeft) {
				break;
			}
			printf("getting current pnode's ID\n");
			my_pthread_t currId = currPnode->tid;
			printf("accessing current node's tcbList\n");
			tcb *currTcb = tcbList[(uint) currId];
			printf("checking if current thread's status is THREAD_READY\n");
			// if the current pnode's thread is ready to run:
			if(currTcb->status == THREAD_READY) {
				printf("current thread's status is THREAD_READY\n");
				// make a temp ptr to the current pnode.
				pnode *tempCurr = currPnode;
				// first case: pnode is first node in queue
				if(currPnode == MLPQ[i]) {
					printf("pnode is first node in queue\n");
					// set MLPQ[i]'s pointer to the next node
					MLPQ[i] = MLPQ[i]->next;
				}
				// second case: pnode isn't first node (e.g. is
				// in the middle or is the last node)
				else{
					printf("pnode isn't first node\n");
					prev->next = currPnode->next;
				}
				// add the tempCurr ptr to the end of the runQueue.
				pnode *temp = runQueue;
				printf("adding tempCurr to end of runQueue\n");
				while(temp->next != NULL) {
					temp = temp->next;
				}
				temp->next = tempCurr;
				// point its next member to NULL.
				printf("setting tempCurr->next\n");
				tempCurr->next = NULL;
				// set the thread's cyclesWaited to 0, as it's being
				// given a chance to run.
				printf("setting currTcb->cyclesWaited\n");
				currTcb->cyclesWaited = 0;;
				// give the thread the appropriate number of time slices
				printf("giving thread appropriate number of time slices\n");
				currTcb->timeSlices = numSlices;
				// subtract numSlices from timeSlicesLeft
				timeSlicesLeft = timeSlicesLeft - numSlices;
				// change its corresponding thread's status to THREAD_READY.
				printf("setting currTcb->status to THREAD_READY\n");
				currTcb->status = THREAD_READY;
				printf("line 531\n");
			}
			printf("continuing in MLPQ navigation\n");
			prev = currPnode;
			currPnode = currPnode->next;
			printf("at end of level in MLPQ: %d\n", i);
		}
	}

	// third part: searching all non-0 levels of the MLPQ to see if any threads
	// not at P0 have THREAD_READY status and an age greater than 5.
	// if so, bump up their priority level and set their age to 0.
	// this means we add them to the next highest level, increment their
	// priority by 1, and delink them from this level.
	for(i = 1; i < MAX_NUM_THREADS; i++) {
		pnode *curr = MLPQ[i];
		pnode *prev = MLPQ[i];
		// go through current level's queue
		while(curr != NULL) {
			tcb *currTcb = tcbList[(uint) curr->tid];
			// if the thread has THREAD_READY status:
			if(currTcb->status == THREAD_READY) {
				// if the thread's age is 5 cycles or greater,
				// "promote" it
				if(currTcb->cyclesWaited >=5) {
					// set its age to 0
					currTcb->cyclesWaited = 0;
					// decrement its priority member
					currTcb->priority -= 1;
					// set a temp ptr to the current thread
					pnode *temp = curr;
					// delink it from the current queue
					prev->next = curr->next;
					// insert it into the next highest level
					insertPnodeMLPQ(temp, currTcb->priority);
				}
				// otherwise, increase its age by 1
				else{ 
					currTcb->cyclesWaited ++;
				}
			}
			prev = curr;
			curr = curr->next;
		}
	}

	// final part: check if runQueue and MLPQ are both empty. if they
	// are, set manager_active to 0.
	if(runQueue == NULL) {
		int mlpq_empty = 1;
		// check and see if all queues in MLPQ are empty.
		for(i = 0; i < sizeof(MLPQ); i++) {
			if(MLPQ[i] != NULL) {
				break;
			}
			if(i == (sizeof(MLPQ) - 1)) {
				mlpq_empty = 0;
			}
		}
		if(mlpq_empty == 1) {
			manager_active = 0;
		}
	}
	// when runQueue has either been populated with valid, ready threads,
	// or we've indicated that the manager thread's job has finished,
	// return 0 to indicate success.
	return 0;
}



/* this function is the helper function which performs most of
the work for the manager thread's run queue. Returns 0 on failure,
1 on success. */
int runQueueHelper() {
	printf("entered runQueueHelper()!\n");
	testMsg();
	// first, check and see if the manager thread is still active after
	// the last round of maintenance
	if(manager_active == 0) {
		return 0;
	}
	if(runQueue == NULL) {
		printf("Error! Went into runQueueHelper() without a populated runQueue. There must be an issue in the MLPQ not resolved in maintenanceHelper().\n");
		return -1;
	}

	// call signal handler for SIGVTALRM, which should activate
	// each time we receive a SIGVTALRM
	sigaction(SIGVTALRM, &sa, NULL);

	// it begins with a populated runQueue. it needs to iterate through
	// each thread and perform the necessary functions depending on
	// the thread's status. the only valid status for a thread it
	// encounters is THREAD_READY. it will, however, change thread
	// statuses to THREAD_DONE, THREAD_INTERRUPTED, or THREAD_WAITING
	// at some point.
	pnode *currPnode = runQueue;
	pnode *prev = currPnode;
	while(currPnode != NULL) {
		my_pthread_t currId = currPnode->tid;
		tcb *currTcb = tcbList[(uint) currId];
		// grab number of time slices allowed for the thread
		int slicesLeft = currTcb->timeSlices;
		// change status of current thread to running
		currTcb->status = THREAD_RUNNING;
		current_status = THREAD_RUNNING;
		// setitimer for 25ms * the number of time slices allotted
		// to this thread. set timer type to VIRTUAL_TIMER.
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = (25000) * slicesLeft;
		setitimer(ITIMER_VIRTUAL, &timer, NULL);

		// swap contexts with this child thread.
		current_exited = 0;
		current_thread = currId;
		swapcontext(&Manager, &(currTcb->context));
		// if this context resumed and current_status is still THREAD_RUNNING,
		// then thread ran to completion before being interrupted.
		if(current_status == THREAD_RUNNING) {
			// turn itimer off for this thread
			timer.it_value.tv_sec = 0;
			timer.it_value.tv_usec = 0;
            if(current_exited == 0){ //implicit exit
                current_thread = tcbList[(uint) currId]->tid;
                my_pthread_exit(NULL);
            }
			currTcb->status = THREAD_DONE;
		}
		// if this context  resumed and current_status is THREAD_INTERRUPTED,
		// then the signal handler interrupted the child thread, which
		// didn't get to run to completion.
		else if(current_status == THREAD_INTERRUPTED){
			// Do nothing here, since thread's status was already set
		}
		// this branch shouldn't occur
		else {
			printf("Error! Thread %d in runQueue had non-valid status.\n", currId);
			return -1;
		}
		// go to the next node in the runQueue
		currPnode = currPnode->next;
	}
	return 0;
	printf("finished runQueueHelper()\n");
}



void VTALRMhandler(int signum) {
	// DO NOT PUT A PRINT MESSAGE IN A SIGNAL HANDLER!!!

	// We've interrupted a thread, so change the current_status
	// to THREAD_INTERRUPTED
	current_status = THREAD_INTERRUPTED;
	// Set the current context back to Manager
	current_thread = MAX_NUM_THREADS + 1;
	setcontext(&Manager);
}


int init_manager_thread() {
	printf("entered init_manager_thread()!\n");
	testMsg();
	// initialize global variables before adding Main's thread
	// to the manager
	// first, initialize array for MLPQ
//	printf("initializing MLPQ array\n");
	pnode *temp[NUM_PRIORITY_LEVELS];
	MLPQ = temp;
	int i;
//	printf("setting MLPQ queues to NULL by default\n");
	for(i = 0; i < NUM_PRIORITY_LEVELS; i++) {
		MLPQ[i] = NULL;
	}
	// next, initialize tcbList
//	printf("initializing tcbList\n");
	tcb *newTcbList[MAX_NUM_THREADS];
	tcbList = newTcbList;
	// initialize all pointers in tcbList to NULL by default.
//	printf("initializing tcbList pointers to NULL\n");
	for(i = 0; i < sizeof(tcbList); i++) {
		tcbList[i] = NULL;
	}
	// initialize current_exited to 0
	current_exited = 0;
	// Get the current context (this is the main context)
	printf("getting main context!\n");
	getcontext(&Main);
	// Point its uc_link to Manager (Manager is its "parent thread")
	printf("pointing main's uc_link to Manager\n");
	Main.uc_link = &Manager;
	// initialize tcb for main
	if(MLPQ[0] == NULL) {
		printf("MLPQ level 0 is NULL!\n");
	}
	else{
		printf("MLPQ level 0 is NOT NULL!\n");
	}
	printf("initializing tcb for main\n");
	tcb *newTcb = createTcb(THREAD_READY, 0, Main.uc_stack, Main, 0);
	if(MLPQ[0] == NULL) {
		printf("MLPQ level 0 is NULL!\n");
	}
	else{
		printf("MLPQ level 0 is NOT NULL!\n");
	}
	//now add pnode with Main thread's ID (0) to MLPQ
	printf("creating mainNode with TID 0\n");
	pnode *mainNode = createPnode(0);
	printf("inserting main pnode into MLPQ level 0!\n");
	insertPnodeMLPQ(mainNode, 0);
	printf("setting tcbList[0] to main's tcb\n");
	tcbList[0] = newTcb;
	threadsSoFar = 1;
	runQueue = NULL;
	// set manager_active to 1
	manager_active = 1;
	// initialize manager thread's context
	printf("getting Manager's context\n");
	getcontext(&Manager);
	// this is the stack that will be used by the manager context
	char manager_stack[MEM];
	// point the manager's stack pointer to the manager_stack we just set
	printf("setting Manager's stack attributes\n");
	Manager.uc_stack.ss_sp = manager_stack;
	// set the manager's stack size to MEM
	Manager.uc_stack.ss_size = sizeof(manager_stack);
	// no other context will resume after the manager leaves
	printf("setting Manager's uc_link\n");
	Manager.uc_link = NULL;
	// attach manager context to my_pthread_manager()
	printf("Making context for manager\n");
	makecontext(&Manager, (void*)&my_pthread_manager, 0);
	// allocate memory for signal alarm struct
//	printf("setting memory for signal alarm struct\n");
	memset(&sa, 0, sizeof(sa));
	// install VTALRMhandler as the signal handler for SIGVTALRM
//	printf("installing VTALRMhandler as signal handler\n");
	sa.sa_handler = &VTALRMhandler;
	printf("finished init_manager_thread()\n");
	return 0;
}


tcb *createTcb(int status, my_pthread_t tid, stack_t stack, ucontext_t context, uint timeSlizes) {
	printf("entered createTcb()!\n");
	testMsg();
	printf("input tid: %d\n", tid);
	// allocate memory for tcb instance
//	printf("mallocing memory for new tcb\n");
	tcb *ret = (tcb*) malloc(sizeof(tcb));
	// set members to inputs
//	printf("setting status\n");
	ret->status = status;
//	printf("setting tid\n");
	ret->tid = tid;
//	printf("setting stack\n");
	ret->stack = stack;
//	printf("setting context for tcb\n");
	ret->context = context;
	// set priority to 0 by default
//	printf("setting priority and rest of vars\n");
	ret->priority = 0;
	// waitingThread is -1 by default
	ret->waitingThread = MAX_NUM_THREADS + 2;
	// valuePtr is NULL by default
	ret->valuePtr = NULL;
	// cyclesWaited is 0 by default
	ret->cyclesWaited = 0;
	// return a pointer to the instance
//	printf("successfully created tcb #%d\n", tid);
	printf("finished createTcb()!\n");
	return ret;
}


pnode *createPnode(my_pthread_t tid) {
	printf("entered createPnode()!\n");
	testMsg();
	pnode *ret = (pnode*) malloc(sizeof(pnode));
	ret->tid = tid;
	ret->next = NULL;
	printf("finished createPnode()\n");
	return ret;
}

int insertPnodeMLPQ(pnode *input, uint level) {
	printf("entered insertPnodeMLPQ()!\n");
	testMsg();
	if(MLPQ == NULL) {
		printf("Error, MLPQ is NULL!\n");
		return -1;
	}
	if(input == NULL) {
		printf("Error, input is NULL!\n");
		return -1;
	}
	if(level > NUM_PRIORITY_LEVELS) {
		printf("Error, level > NUM_PRIORITY_LEVELS!\n");
		return -1;
	}
	// error-checking done, begin insertion.
	// first scenario: MLPQ[level] is NULL.
	if(MLPQ[level] == NULL) {
		printf("Inserting input node as head!\n");
		// insert input as head
		MLPQ[level] = input;
		return 0;
	}
	// second scenario: MLPQ[level] has one or more nodes.
	// go until we find the last node (temp->next == NULL)
	pnode *temp = MLPQ[level];
	while(temp->next != NULL) {
		printf("Inserting input node in middle/at end!\n");
		temp = temp->next;
	}
	printf("Delinking node from runQueue and putting at end of MLPQ.\n");
	// set temp->next to input
	temp->next = input;
	// input->next is set to NULL (in case we inserted a thread
	// from the runQueue)
	input->next = NULL;
	printf("finished insertPnodeMLPQ()\n");
	return 0;
}

/* Implements getting the number of slices for a given input level.
Should be 2^(level), so 1 slice at Level 0, 2 at Level 1, 4 at Level 2,
8 at level 3, 16 at Level 4. */
int level_slices(int level) {
	printf("entered level_slices!\n");
	// base case: level 0, give 1 slice
	if(level == 0) {
//		printf("entered base case! \n");
		printf("finished level_slices()\n");
		return 1;
	}
	// recursive case: return 2 * recursive func
	else{
//		printf("entered recusive case!\n");
		return 2*(level_slices(level - 1));
	}

}

/* Print message for testing. Used to tell current running thread,
manager_active. */
void testMsg() {
	printf("Currently in thread %d\n", current_thread);
	if(manager_active == 0) {
		printf("manager not active yet.\n");
	}
	printf("threads so far: %d\n", threadsSoFar);
}
