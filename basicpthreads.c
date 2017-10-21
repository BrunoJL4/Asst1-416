#include "my_pthread_t.h"
#include <pthread.h>
#include <stdio.h>

#define BILLION 1000000000L

/* this function is run by the second thread */
void * inc_x(){

    
    pnode *ptr = (pnode*) malloc(sizeof(pnode));
    ptr->tid = 9001;
    ptr->next = NULL;
    printf("The increment you want to pass to the joined thread is %d\n", *((int*)ptr));
    
    pthread_exit(ptr);
    /* the function must return something - NULL will do */
    return NULL;
}



int main(){
    struct timespec start, end;
    uint64_t diff;

    clock_gettime(CLOCK_MONOTONIC, &start);

    int x = 0, y = 0;

    void *ptr_stored;
    void **ptr = &ptr_stored;
    
    
    /* show the initial values of x and y */
    printf("x: %d, y: %d\n", x, y);

    /* this variable is our reference to the second thread */
    pthread_t inc_x_thread;

    /* create a second thread which executes inc_x(&x) */
    if(pthread_create(&inc_x_thread, NULL, inc_x, NULL) != 0) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    
    /* wait for the second thread to finish */
    if(pthread_join(inc_x_thread, ptr) != 0) {
        fprintf(stderr, "Error joining thread\n");
        return 2;

    }

    
    pnode **ptr_addr = (pnode**) ptr;
    pnode *temp = (pnode*) *(ptr_addr);
    printf("temp->tid: %d\n", temp->tid);
    if(temp->next == NULL) {
        printf("temp->next is NULL!\n");
    }
    else{
        printf("temp->next is NOT NULL!\n");
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    printf("elapsed time = %llu nanoseconds\n", (long long unsigned int) diff);
    return 0;
}
