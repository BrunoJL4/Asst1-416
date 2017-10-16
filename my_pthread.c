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

/* Additional ucontext funtion info to help out */
//getcontext(context) - initializes a blank context or explicitly gets the context specified
//setcontext(context) - explicitly sets the current context to context specified
//makecontext(context, fn, #args) - assigns specified context to a function
//swapcontext(context1, context2) - assigns current context to first arg, then runs second arg context

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
unsigned int threadsSoFar;

/* contexts */
ucontext_t Manager, Main, CurrentContext;

/* info on current thread */

//indicates whether the current thread has explicitly called
//pthread_exit(). 0 if false, 1 if true. used in manager thread
//to determine whether one calls pthread_exit()'s functionality
//on a thread that didn't explicitly call it.
unsigned int current_exited;

/*ID of the currently-running thread. MAX_NUM_THREADS+1 if manager,
otherwise then some child thread. */
my_pthread_t current_thread;

/* The status of the currently-running thread (under the manager).
Will either be THREAD_RUNNING or THREAD_INTERRUPTED. */
int current_status;

/* Boolean 1 if manager thread is active, otherwise 0 as globals
are initialized to by default*/
unsigned int manager_active;

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
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	printf("entered my_pthread_create()!\n");
	//check that manager thread exists	
	//init if it does not
	if (manager_active != 1) {
		init_manager_thread();
	}
	//set information for new child thread's context
	//thread should be ready to run by default
	int status = THREAD_READY;
	my_pthread_t tid = threadsSoFar;
	// set aside a stack for the user
	char stack[MEM];
	// set the stack pointer for the user's context
	//time slices is 0 by default
	unsigned int timeSlices = 0;
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
	if (arg == NULL) {
		makecontext(&context, (void*)&function, 0);
	} else {
		makecontext(&context, (void*)&function, 1, arg);
	}
	//check if we've exceeded max number of threads
	if (threadsSoFar >= MAX_NUM_THREADS) {
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
		tcbList[(unsigned int) tid] = newTcb;
		// insert a pnode containing the ID at Level 0 of MLPQ
		pnode *node = createPnode(tid);
		insertPnodeMLPQ(node, 0);
		return tid;
	}
	// if still using new ID's, just use threadsSoFar as the index and increment it
	tcb *newTcb = createTcb(status, tid, context.uc_stack, context, timeSlices);
	// add the new tcb to the tcbList at the cell corresponding to its ID
	tcbList[threadsSoFar] = newTcb;
	// insert a pnode containing the ID at Level 0 of MLPQ
	pnode *node = createPnode(tid);
	insertPnodeMLPQ(node, 0);
	// we've added another thread, so increase this
	threadsSoFar ++;
	// swap to the manager
	current_thread = MAX_NUM_THREADS + 1;	
	swapcontext(&CurrentContext, &Manager);
	
	//returns the new thread id on success
	return tid; 
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	printf("entered my_pthread_yield()!\n");
	//set thread to yield, set current_thread to manager, swap contexts.
	//manager will yield job in stage 1 of maintenance
	tcbList[(unsigned int) current_thread]->status = THREAD_YIELDED;
	current_thread = MAX_NUM_THREADS + 1;
	swapcontext(&CurrentContext, &Manager);
	return 1;

}

/* terminate a thread and fill in the value_ptr of the
thread waiting on it, if any */
void my_pthread_exit(void *value_ptr) {
	printf("entered my_pthread_exit()!\n");
	// create unsigned int version of current thread to reduce casts
    unsigned int current_thread_int = (unsigned int) current_thread;

    // thread that the calling thread is joined to
    my_pthread_t joinedThread = tcbList[current_thread_int]->waitingThread;

    // if the thread has another thread waiting on it (joined this thread),
    // set its valuePtr member accordingly
    if((unsigned int)joinedThread != MAX_NUM_THREADS + 2) {
    	tcbList[joinedThread]->valuePtr = value_ptr;
    }
    
    // swap back to the Manager context
    current_thread = MAX_NUM_THREADS + 1;
    current_exited = 1;
    swapcontext(&CurrentContext, &Manager);
}


/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	printf("entered my_pthread_join()!\n");
     // create unsigned int version of current thread to reduce casts
    unsigned int thread_int = (unsigned int) thread;

    //what if thread doesn't exist?
    if((tcbList[thread_int]) == NULL){
        fprintf(stderr, "pthread_join(): Target thread %d does not exist!\n", thread_int);
        return 0;
    }
  
    // set target thread's waitingThread to this thread
    tcbList[thread_int]->waitingThread = current_thread;

    // set this thread's status to THREAD_WAITING
    tcbList[(unsigned int) current_thread]->status = THREAD_WAITING;

    // set the value_ptr to point to this thread's valuePtr, so that
    // the caller has access to the value. trying to access the
    // target thread's valuePtr might be undefined because it could
    // have been terminated by the manager thread before the user
    // acceses value_ptr.
    *value_ptr = tcbList[(unsigned int) current_thread]->valuePtr;

    // swap back to the manager
    swapcontext(&CurrentContext, &Manager);
        
    return 1; // success
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	printf("entered my_mutex_init()!\n");
	//Check if mutex is initialized
	//if so, return
	if (&mutex != NULL) {
		return 0;
	}
	//otherwise, initialize mutex
	mutex = malloc(sizeof(my_pthread_mutex_t));
	mutex->status = UNLOCKED;
	mutex->waitQueue = NULL;
	mutex->ownerID = -1;
	mutex->attr = attr;
	
	return 1;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_lock()!\n");
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
		tcbList[(unsigned int) current_thread]->status = THREAD_BLOCKED;
		//let the manager continue in the run queue
		current_thread = MAX_NUM_THREADS + 1;
		setcontext(&Manager);
	} 
	//continue running after the end of yielding OR did not have to yield
	//Set mutex value to locked
	mutex->status = LOCKED;
	//Set mutex owner to current thread
	mutex->ownerID = current_thread;
	
	return 1;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_unlock()!\n");
	//If mutex is NOT initialized
	//user did something bad
	if (&mutex == NULL) {
		return 0;
	//Elif mutex does not belong to us
	//we can't unlock it
	} else if (mutex->ownerID != current_thread) {
		return 0;
	}
	//otherwise unlock mutex
	mutex->status = UNLOCKED;
	//Check waiting queue, destroy mutex if there is no more use
	if (mutex->waitQueue == NULL) {
		my_pthread_mutex_destroy(mutex);
		return 1;
	}
	//alert the next available thread & remove it from queue/add back to run queue
	pnode *ptr = mutex->waitQueue;
	mutex->waitQueue = mutex->waitQueue->next;
	//make this thread ready so it can now acquire this lock
	tcbList[(unsigned int) ptr->tid]->status = THREAD_READY;
	free(ptr);
	
	return 1;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	printf("entered my_pthread_mutex_destroy()!\n");
	//If mutex is NOT initialized
	if (&mutex == NULL) {
		return 0;
	//Elif mutex is locked
	} else if (mutex->status == LOCKED) {
		return 0;
	}	
	//otherwise, free the memory used
	free(mutex);
		
	return 1;
}


/* Essential support functions go here (e.g: manager thread) */

/* Carries out the manager thread responsibilities.
Returns 0 on failure, 1 on success. */
int my_pthread_manager() {
	printf("entered my_pthread_manager()!\n");
	// check if manager is still considered "active"
	while(manager_active == 1) {
		// perform maintenance cycle
		if(!maintenanceHelper()) {
			printf("Error in maintenanceHelper!\n");
			return 0;
		}
		// perform run queue functions
		if(!runQueueHelper()) {
			printf("Error in runQueueHelper!\n");
			return 0;
		}
	}
	// We only reach this point when maintenanceHelper()
	// has set manager_active to 0. Leave the function.
	return 1;
}


/* Helper function which performs most of the work for
the manager thread's maintenance cycle. Returns 0 on failure,
1 on success.*/
int maintenanceHelper() {
	printf("entered maintenanceHelper()!\n");
	// first part: clearing the run queue, and performing
	// housekeeping depending on the thread's status
	pnode *currPnode = runQueue;
	while(currPnode != NULL) {
		my_pthread_t currId = currPnode->tid;
		tcb *currTcb = tcbList[(unsigned int)currId];
		// if a runQueue thread's status is THREAD_DONE:
		
        // Took a look at this, made minor syntax changes, long as checkAndDeallocateStack
        // and InsertPnodeMLPQ work as intended, stage 1 looks good. - Joe Gormley
		if(currTcb->status == THREAD_DONE) {
			// deallocate the thread's tcb through tcbList
			free(currTcb);
			// set tcbList[tid] to NULL
			tcbList[(unsigned int)currId] = NULL;
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
			return 0;
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
	for(i = 0; i < sizeof(MLPQ); i++) {
		// formula for priority levels v. time slices: 2^(level)
		int numSlices = round(pow(2, i)); // round() used to turn pow() to int val
		// if we don't have enough timeSlices left to distribute to any node in
		// the current level, break (prevents searching further levels)
		if(numSlices > timeSlicesLeft) {
			break;
		}
		// go through this level's queue, if at all applicable.
		pnode *currPnode = MLPQ[i];
		pnode *prev = currPnode;
		while(currPnode != NULL) {
			// don't search the current level further if not enough
			// time slices are left.
			if(numSlices > timeSlicesLeft) {
				break;
			}
			my_pthread_t currId = currPnode->tid;
			tcb *currTcb = tcbList[(unsigned int) currId];
			// if the current pnode's thread is ready to run:
			if(currTcb->status == THREAD_READY) {
				// make a temp ptr to the current pnode.
				pnode *tempCurr = currPnode;
				// first case: pnode is first node in queue
				if(currPnode == MLPQ[i]) {
					// set MLPQ[i]'s pointer to the next node
					MLPQ[i] = MLPQ[i]->next;
				}
				// second case: pnode isn't first node (e.g. is
				// in the middle or is the last node)
				else{
					prev->next = currPnode->next;
				}
				// add the tempCurr ptr to the end of the runQueue.
				pnode *temp = runQueue;
				while(temp->next != NULL) {
					temp = temp->next;
				}
				temp->next = tempCurr;
				// point its next member to NULL.
				tempCurr->next = NULL;
				// give the thread the appropriate number of time slices
				currTcb->timeSlices = numSlices;
				// subtract numSlices from timeSlicesLeft
				timeSlicesLeft = timeSlicesLeft - numSlices;
				// change its corresponding thread's status to THREAD_READY.
				currTcb->status = THREAD_READY;
			}
			prev = currPnode;
			currPnode = currPnode->next;
		}
	}

	// final part: check if runQueue and MLPQ are both empty. if they
	// are, set manager_active to 0.
	if(runQueue == NULL) {
		int mlpq_empty = 1;
		int i;
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
	// return 1 to indicate success.
	return 1;
}



/* this function is the helper function which performs most of
the work for the manager thread's run queue. Returns 0 on failure,
1 on success. */
int runQueueHelper() {
	printf("entered runQueueHelper()!\n");
	// first, check and see if the manager thread is still active after
	// the last round of maintenance
	if(manager_active == 0) {
		return 1;
	}
	if(runQueue == NULL) {
		printf("Error! Went into runQueueHelper() without a populated runQueue. There must be an issue in the MLPQ not resolved in maintenanceHelper().\n");
		return 0;
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
		tcb *currTcb = tcbList[(unsigned int) currId];
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
                current_thread = tcbList[(unsigned int) currId]->tid;
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
			return 0;
		}
		// go to the next node in the runQueue
		currPnode = currPnode->next;
	}
	return 1;
}



int VTALRMhandler(int signum) {
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
	// Get the current context (this is the main context)
	getcontext(&Main);
	// Point its uc_link to Manager (Manager is its "parent thread")
	Main.uc_link = &Manager;
	// initialize tcb for main
	tcb *newTcb = createTcb(THREAD_READY, 0, Main.uc_stack, Main, 0);
	// initialize global variables before adding Main's thread
	// to the manager
	// first, initialize array for MLPQ
	pnode *temp[NUM_PRIORITY_LEVELS];
	MLPQ = temp;
	// next, initialize tcbList
	tcb *newTcbList[MAX_NUM_THREADS];
	tcbList = newTcbList;
	// initialize current_exited to 0
	current_exited = 0;
	// initialize all pointers in tcbList to NULL by default.
	// convention will be that any non-active tcbList cell is
	// set to NULL for ease of linear search functions.
	int i;
	for(i = 0; i < sizeof(tcbList); i++) {
		tcbList[i] = NULL;
	}
	//now add pnode with Main thread's ID (0) to MLPQ
	pnode *mainNode = createPnode(0);
	MLPQ[0] = mainNode;
	tcbList[0] = newTcb;
	threadsSoFar = 1;
	runQueue = NULL;
	// set manager_active to 1
	manager_active = 1;
	// initialize manager thread's context
	getcontext(&Manager);
	// this is the stack that will be used by the manager context
	char manager_stack[MEM];
	// point the manager's stack pointer to the manager_stack we just set
	Manager.uc_stack.ss_sp = manager_stack;
	// set the manager's stack size to MEM
	Manager.uc_stack.ss_size = sizeof(manager_stack);
	// no other context will resume after the manager leaves
	Manager.uc_link = NULL;
	// attach manager context to my_pthread_manager()
	makecontext(&Manager, (void*)&my_pthread_manager, 0);
	// allocate memory for signal alarm struct
	memset(&sa, 0, sizeof(sa));
	// install VTALRMhandler as the signal handler for SIGVTALRM
	sa.sa_handler = &VTALRMhandler;
	
	return 1;
}


tcb *createTcb(int status, my_pthread_t tid, stack_t stack, ucontext_t context, unsigned int timeSlizes) {
	printf("entered createTcb()!\n");
	// allocate memory for tcb instance
	tcb *ret = (tcb*) malloc(sizeof(tcb));
	// set members to inputs
	ret->status = status;
	ret->tid = tid;
	ret->stack = stack;
	ret->context = context;
	// set priority to 0 by default
	ret->priority = 0;
	// waitingThread is -1 by default
	ret->waitingThread = MAX_NUM_THREADS + 2;
	// valuePtr is NULL by default
	ret->valuePtr = NULL;
	// return a pointer to the instance
	return ret;
}


pnode *createPnode(my_pthread_t tid) {
	printf("entered createPnode()!\n");
	pnode *ret = (pnode*) malloc(sizeof(pnode));
	ret->tid = tid;
	ret->next = NULL;
	return ret;
}

int insertPnodeMLPQ(pnode *input, unsigned int level) {
	printf("entered insertPnodeMLPQ()!\n");
	if(MLPQ == NULL) {
		return 0;
	}
	if(input == NULL) {
		return 0;
	}
	if(level > NUM_PRIORITY_LEVELS) {
		return 0;
	}
	// error-checking done, begin insertion.
	// first scenario: MLPQ[level] is NULL.
	if(MLPQ[level] == NULL) {
		// insert input as head
		MLPQ[level] = input;
		return 1;
	}
	// second scenario: MLPQ[level] has one or more nodes.
	// go until we find the last node (temp->next == NULL)
	pnode *temp = MLPQ[level];
	while(temp->next != NULL) {
		temp = temp->next;
	}
	// set temp->next to input
	temp->next = input;
	// input->next is set to NULL (in case we inserted a thread
	// from the runQueue)
	input->next = NULL;
	return 1;
}
