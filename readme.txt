# Asst1-416
OS Assignment 1 (My PThread and Scheduler) 


AUTHORS: Bruno Lucarelli
		 Alex Marek
		 Joseph Gormley

LIBRARY: my_pthread_t.h
		
		 my_pthread_create()
		 my_pthread_yield()
		 my_pthread_exit()
		 my_pthread_join()
		 my_pthread_mutex_init()
		 my_pthread_mutex_lock()
		 my_pthread_mutex_unlock()
		 my_pthread_mutex_destroy()

------------------------|
DESIGN / DATA STRUCTURES|
------------------------|

BE SURE TO INCLUDED RATIONAL

'MLPQ'
	The multilevel priority queue is the 'waiting to be selected' stage of all the threads. It
	consist of an array of linked lists with each index of the array corresponding to the lists 
	priority level (priority 0 is the highest level). The scheduler fills the runQueue based on 
	the priority level of various threads. 
	
	Ex. MLPQ:	
		
	Pri.	   
	    |-----|---|   |-----|--|  |-----|---|
	 0 	|  5  |  ---->| 18  | --->|  3  |  ---> NULL
		|-----|---|   |-----|--|  |-----|---|
		|         |
	 1	|  NULL   |
		|---------|   
	 .	|    .    |
     .  |    .    |
	 .  |    .    |
	 .	|    .    |
		|------|--|   |----|---|
		|   7  |  --->| 32 |  ---> NULL   
	 n	|------|--|   |----|---|

		
	

'Runtime Queue'
	The runtime queue is a list of threads that have the status of THREAD_READY (in the tcbList) to
	inform the manager that they are prepared to be ran. During the traversing of the queue, threads 
	are either set to THREAD_DONE, THREAD_WAITING or THREAD_INTERRUPTED to indicate the threads 
	performance during its runtime. 

	Ex. runQueue:									  
														  current thead running
														     in traversal													 
		|-------------|---|  |--------------------|---|  |--------------|---|
		| THREAD_DONE |  --->| THREAD_INTERRUPTED |  --->| THREAD_READY |  ---> NULL
		|-------------|---|  |--------------------|---|  |--------------|---|
		
		
'Thread Control Block'
	As given 'jobs' are assigned to different thread, details  about each thread
	needs to be stored in such a manner to be easily referenced. These details include 
	such things as a status, stack pointer, thread id, priority, slices of time, if it's 
	joined to another thread etc. 
	
	We choose an array that holds structs that contain information for each respective thread. 
	After considering the workload on the given bench marks, we came to the conclusion 
	that a few dozen threads would suffice (stuck with powers of 2).
	
	Ex. tcbList:
	
		  0   1   2    ...............    63         
		|---|---|---|-------------------|---|        struct tcbList[0] = scheduler
		| M | 1 | 2 |  ...............  | 63|        struct tcbList[1] = thread w/ tid of 1
		|---|---|---|-------------------|---|		 struct tcbList[x] = thread w/ tid of x  
		

'Thread Recycle'
	Thread IDs are distributed all the way until the highest tid is assigned, once 
	this occurs, we recycle previously assigned thread ids providing the respective thread 
	has finished its task. We assure this happens by inserting such tids into a linked list as 
	the thread's job has been completed. This provides constant time to assign thread ids.
	
	Ex. recyclableQueue: 
		
		|----|---|    |----|---|    |----|---|
		| 19 |  ----->|  2 |  ----->| 31 |  ----> NULL 
		|----|---|    |----|---|    |----|---|


-----------------|
SCHEDULER DETAILS|
-----------------|
quanta 25m/s
Operations between data structures, how they depend on one another
How much to +/- piority levels






---------------|
TESTING RESULTS|
---------------|
Own Test 1
Own Test 2
Own Test .
Own Test .
Own Test n
externalCal.c
parallelCal.c
vectorMultiply.c
