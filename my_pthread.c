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

/* TODO @all: Decide whether or not these variables should just be 
located within the manager thread's function, or if they should
stay global. */

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

//ID of the currently-running thread. -1 if manager,
//if >=0 then some thread.
unsigned int current_thread;

/* Boolean 1 if manager thread is active, otherwise 0 as globals
are initialized to by default*/
unsigned int manager_active;

/* Status of currently-running thread. 
TODO @bruno: determine if this is needed. */
threadStatus currentStatus;

/* End global variable declarations. */

/* my_pthread and mutex function implementations */

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	//check that manager thread exists	
	//init if it does not
	if (manager_active != 1) {
		init_manager_thread();
	}
	//set information for new child thread's context
	//thread should be ready to run by default
	threadStatus status = THREAD_READY;
	my_pthread_t tid = threadsSoFar;
	//allocating MEM bytes for the stack
	stack_t stack = malloc(MEM);
	//time slices is 0 by default
	unsigned int timeSlices = 0;
	ucontext_t context;
	//initialize context, fill it in with current
	//context's information
	getcontext(&context);
	//assign context's members
	//any child context should link back to Manager
	//upon finishing execution/being interrupted or preempted
	context.uc_link = Manager;
	//TODO @bruno: figure out what signals to actually mask, if any
	context.uc_sigmask = 0;
	//set context's stack to our allocated stack
	context.uc_stack = stack;
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
		tcb *newTcb = createTcb(status, tid, stack, context, timeSlices);
		//change the tcb instance in tcbList[id] to this tcb
		tcbList[tid] = newTcb;
		// insert a pnode containing the ID at Level 0 of MLPQ
		pnode *node = createPnode(tid);
		insertPnodeMLPQ(node, 0);
		return tid;
	}
	// if still using new ID's, just use threadsSoFar as the index and increment it
	tcb *newTcb = createTcb(status, tid, stack, context, timeSlices);
	// add the new tcb to the tcbList at the cell corresponding to its ID
	tcbList[threadsSoFar] = newTcb;
	// insert a pnode containing the ID at Level 0 of MLPQ
	pnode *node = createPnode(tid);
	insertPnodeMLPQ(node, 0);
	// we've added another thread, so increase this
	threadsSoFar ++;
	//TODO @bruno: Figure out how this shit works	
	swapcontext(&CurrentContext, &Manager);
	
	//returns the new thread id on success
	return tid; 
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	
    //task goes to end of tasks of jobs
    //switch context to manager, manager should choose next thread to run
    
    
    return 0;
}

/* terminate a thread */
//thread that calls join is dependent on target thread, sooo....
//this thread (target thread) should be explicitly calling pthread_exit, providing it is joined
//value_ptr: value that pthread_join caller will be using to complete task
void my_pthread_exit(void *value_ptr) {
    
    //is thread joined?
    //if not
        //implicit behaviour
        /* Performing  a  return  from the start function of any thread other than
        the main thread results in an implicit call  to  pthread_exit(),  using
        the function's return value as the thread's exit status. */
    
    //if yes,
        //set thread status to THREAD_FINISHED
        //switch to manager, manager should take job off runtime during maintenence
    
}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	//Does target thread exist?
    //if(target thread been previously joined)
        //creates undefined behavior - manpages
    
    /* ACTUAL WORK */
    
    //target thread HAS TO FINISH RUNNING before caller gets anymore cpu time
    //i think above requires a context switch to target thread at this point
    
    //if(Target thread was cancelled)
        //*RETVAL = PTHREAD_CANCEL
        //return -1; => is this considered an error?
    //if(exit's retval != null)
        //value_ptr = exits retval's address;
    return 0; //success
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	//Check if mutex is initialized
		//return 0
	//Else
		//initialize mutex as free & assign mutex attribute
		//return 1
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	//Call my_pthread_mutex_init
	//If mutex is locked
		//Enter queue for lock
		//Yield
	//Set mutex value to locked
	//Set mutex owner to current thread
	return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	//If mutex is NOT initialized
		//We can't unlock an unanitialized thread ERROR
	//Elif mutex does not belong to us
		//Can't unlock a thread we didn't lock ERROR
	//Else
		//Unlock mutex
		//Check waiting queue
			//alert the next available thread & remove it from queue/add back to run queue
	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	//If mutex is NOT initialized
		//Can't destroy an uninitialized mutex
	//Elif mutex is locked
		//Can't destroy a mutex being used
	//Else
		//Set mutex back to null
		
	return 0;
}


/* Essential support functions go here (e.g: manager thread) */

//TODO @bruno: implement signal handler for SIGALRM/SIGVTALRM
//that comes up when the manager thread calls setitimer()
//and the running thread runs out of time slices.
//Do as described in the documentation.

/* Carries out the manager thread responsibilities.
Returns 0 on failure, 1 on success. */
//TODO @bruno: Implement and document this
int my_pthread_manager() {
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
	// TODO @bruno: when manager thread no longer
	// active, deallocate its resources... maybe
	// make helper function?
	return 1;
}


/* Helper function which performs most of the work for
the manager thread's maintenance cycle. Returns 0 on failure,
1 on success.*/
int maintenanceHelper() {
	// first part: clearing the run queue, and performing
	// housekeeping depending on the thread's status
	pnode *currPnode = runQueue;
	while(currPnode != NULL) {
		my_pthread_t currId = currPnode->currId;
		tcb *currTcb = tcbList[currId];
		// if a runQueue thread's status is THREAD_DONE:
		if(currTcb->status == THREAD_DONE) {
			// check for any threads that share the thread's
			// stack, deallocate the stack if none share it.
			checkAndDeallocateStack(currId);
			// deallocate the context
			free(currTcb->context);
			// then deallocate its tcb through tcbList
			free(currTcb);
			// set tcbList[tid] to NULL
			tcbList[currId] = NULL;
			// then deallocate its pnode in the run queue while
			// moving currPnode to the next node.
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			free(temp);
		}
		// if a runQueue thread's status is THREAD_INTERRUPTED:
		else if(currPnode->status == THREAD_INTERRUPTED){
			// we insert the thread back into the MLPQ but at one lower
			// priority level, also changing its priority member.
			// then change its status to READY.
			currTcb->priority ++;
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			insertPnodeMLPQ(temp, currTcb->priority);
			currTcb->status = THREAD_READY;
		}
		// if a runQueue thread's status is THREAD_WAITING:
		else if(currPnode->status == THREAD_WAITING) {
			// put the thread into the MLPQ at the same priority level,
			// so that it can resume in subsequent runs when it's
			// set to READY as the thread it's waiting on finishes
			// execution.
			pnode *temp = currPnode;
			currPnode = currPnode->next;
			insertPnodeMLPQ(temp, currTcb->priority);
		}
		// if a runQueue thread's status isn't any of the three above:
		else{
			printf("Error! Thread in runQueue found to have invalid status
				during maintenance cycle.\n");
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
	for(i = 0; i < MLPQ.length; i++) {
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
			tcb *currTcb = tcbList[currId];
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
					prev->next = curr->next;
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
				// change its corresponding thread's status to THREAD_RUNNING.
				currTcb->status = THREAD_RUNNING;
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
		for(i = 0; i < MLPQ.length; i++) {
			if(MLPQ[i] != NULL) {
				break;
			}
			if(i == (MLPQ.length - 1)) {
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
//TODO @bruno: implement and document this.
int runQueueHelper() {
	// first, check and see if the manager thread is still active after
	// the last round of maintenance
	if(manager_active == 0) {
		return 1;
	}

	// it begins with a populated runQueue. it needs to iterate through
	// each thread and perform the necessary functions depending on
	// the thread's status. the only valid status for a thread it
	// encounters is THREAD_READY. it will, however, change thread
	// statuses to THREAD_DONE, THREAD_INTERRUPTED, or THREAD_WAITING
	// at some point.

	return 1;
}


int init_manager_thread() {
	//Get the current context (this is the main context)
	getcontext(&Main);
	//Point its uc_link to Manager (Manager is its "parent thread")
	Main.uc_link = Manager;
	//initialize tcb for main
	tcb *newTcb = createTcb(THREAD_READY, 0, Main.stack, Main);
	//initialize global variables before adding Main's thread
	//to the manager
	//first, initialize array for MLPQ
	pnode *temp[NUM_PRIORITY_LEVELS];
	MLPQ = temp;
	//next, initialize tcbList
	tcb *newTcbList[MAX_NUM_THREADS];
	tcbList = newTcbList;
	// initialize all pointers in tcbList to NULL by default.
	// convention will be that any non-active tcbList cell is
	// set to NULL for ease of linear search functions.
	int i;
	for(i = 0; i < tcbList.length; i++) {
		tcbList[i] = NULL;
	}
	//now add pnode with Main thread's ID (0) to MLPQ
	pnode *mainNode = createPnode(0);
	MLPQ[0] = mainNode;
	tcbList[0] = newTcb;
	threadsSoFar = 1;
	runQueue = NULL;
	//set manager_active to 1
	manager_active = 1;
	//actually set up and make the context for manager thread
	getcontext(&Manager);
	Manager.uc_link = 0; //no other context will resume after the manager leaves
	Manager.uc_sigmask = 0; //no signals being intentionally blocked
	Manager.uc_stack = malloc(MEM); //new stack using specified stack size
	makecontext(&Manager, (void*)&my_pthread_manager, 0);
	
	return 1;
}


tcb *createTcb(threadStatus status, my_pthread_t tid, stack_t stack, 
	ucontext_t context) {
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
	ret->waitingThread = -1;
	// valuePtr is NULL by default
	ret->valuePtr = NULL;
	// return a pointer to the instance
	return ret;
}


pnode *createPnode(my_pthread_t tid) {
	pnode *ret = (pnode*) malloc(sizeof(pnode));
	ret->tid = tid;
	ret->next = NULL;
	return ret;
}

int insertPnodeMLPQ(pnode *input, unsigned int level) {
	if(MLPQ == NULL) {
		return 0;
	}
	if(input == NULL) {
		return 0;
	}
	if(level < 0 || level > NUM_PRIORITY_LEVELS) {
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

int checkAndDeallocateStack(my_pthread_t tid) {
	// check if tcbList is NULL
	if(tcbList == NULL) {
		printf("Error! tcbList is NULL for checkAndDeallocateStack.\n");
		return -1;
	}
	// check if given a tid pointing to a valid tcb
	if(tcbList[tid] == NULL) {
		printf("Error! Given tid is NULL for checkAndDeallocateStack.\n");
		return -1;
	}
	// grab stack pointer from stack_t member of thread's tcb
	void *stack_ptr = tcbList[tid]->stack.ss_sp;
	// set flag to indicate that a thread shares this stack.
	// will be set to 1 if we find one that does.
	int stackShared = 0;
	int i;
	for(i = 0; i < tcbList.length; i++) {
		// for any non-NULL tcb in the tcbList, that doesn't
		// share our input's TID
		if( (tcbList[i] != NULL) && (i != tid) ){
			// compare its stack's ss_sp member to stack_ptr.
			// if they share the same address (reference the same stack):
			if(tcbList[i]->stack.ss_sp == stack_ptr) {
				// mark stackShared as 1.
				stackShared = 1;
			}
		}
	}
	// if after running this loop, we haven't found a thread that shares
	// the stack, we deallocate the stack and return successfully.
	if(stackShared == 0) {
		// deallocate stack
		free(tcbList[tid]->stack.ss_sp)
		return 1;
	}
	// otherwise, return.
	return 1;
}
