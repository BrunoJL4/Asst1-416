#include "my_pthread_t.h"
#include <stdio.h>

/* this function is run by the second thread */
void inc_x(){

    
    
    int num = 100;
    void * ptr = (void*)&num;
    printf("The increment you want to pass to the joined thread is %d\n", ptr);
    
    my_pthread_exit(ptr);
    /* the function must return something - NULL will do */
    return NULL;
}



int main(){

    int x = 0, y = 0;

    void ** ptr = NULL;
    
    
    /* show the initial values of x and y */
    printf("x: %d, y: %d\n", x, y);

    /* this variable is our reference to the second thread */
    pthread_t inc_x_thread;

    /* create a second thread which executes inc_x(&x) */
    if(pthread_create(&inc_x_thread, NULL, inc_x, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    
    /* increment y to 100 in the first thread */
    while(++y < 100);

    printf("y increment finished\n");

    /* wait for the second thread to finish */
    if(pthread_join(inc_x_thread, ptr)) {
        fprintf(stderr, "Error joining thread\n");
        return 2;

    }

    

    return 0;
}
