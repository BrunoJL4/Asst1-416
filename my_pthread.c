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

/* End global variable declarations. */

/* my_pthread and mutex function implementations */

/* create a new thread */
//TODO @alex: change last two parameters to address actual params of pthread_create()...
//correctly reference function pointer, number of args, and args
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
	//TODO @alex: figure out how to properly reference function pointer, # of args, and args
	//replace context's function with the input function for the child thread
	makecontext(&context, (void*)&function, 0);
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
		return tid;
	}
	// if still using new ID's, just use threadsSoFar as the index and increment it
	tcb *newTcb = createTcb(status, tid, stack, context, timeSlices);
	//add the new tcb to the tcbList at the cell corresponding to its ID
	tcbList[threadsSoFar] = newTcb;
	//we've added another thread, so increase this
	threadsSoFar ++;
	//TODO @bruno: insert pnode with tid into Level 0 of MLPQ, at the very end...
	//you should create a function called insertNewPnode() that does this, or something. 
	//set CurrentContext to the context that called my_pthread_create()...this could be
	//Main, or it could also be a child thread that's calling my_pthread_create().
	//also swap to Manager, 
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
        the function's return value as the thread's exit status. /*
    
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

/* this function carries out the manager thread responsibilities */
//TODO @all: Implement and document this
int my_pthread_manager() {
	//Check MLPQ
	//Check Run Queue & pick new context
	//Switch context to Main
	return 0;
}

/* this function is the helper function which performs most of
the work for the manager thread's run queue. */
//TODO @all: implement and document this.
//TODO @all: decide what the parameters are, if any.
void runQueueHelper() {
	return 0;
}

//TODO @all: implement signal handler for SIGALRM/SIGVTALRM
//that comes up when the manager thread calls setitimer()
//and the running thread runs out of time slices.
//Do as described in the documentation.

/* TODO @joe, alex: Implement and document this. */
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
	
	return 0;
}

/* Returns a pointer to a new tcb instance. */
tcb *createTcb(threadStatus status, my_pthread_t tid, stack_t stack, 
	ucontext_t context)
	// allocate memory for tcb instance
	tcb *ret = (tcb*) malloc(sizeof(tcb));
	// set members to inputs
	ret->status = status;
	ret->tid = tid;
	ret->stack = stack;
	ret->context = context;
	// set priority to 0 by default
	ret->priority = 0;
	// return a pointer to the instance
	return ret;
}

/* Returns a pointer to a new pnode instance. */
pnode *createPnode(my_pthread_t tid) {
	pnode *ret = (pnode*) malloc(sizeof(pnode));
	ret->tid = tid;
	ret->next = NULL;
	return ret;
}

/* Auxiliary support functions go here. */

