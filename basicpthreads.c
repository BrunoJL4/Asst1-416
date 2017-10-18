#include "my_pthread_t.h"
#include <stdio.h>

/* this function is run by the second thread */
void * inc_x(){

    
    
    int num = 100;
    void *ptr = (void*) &num;
    printf("The increment you want to pass to the joined thread is %d\n", *((int*)ptr));
    
    my_pthread_exit(ptr);
    /* the function must return something - NULL will do */
    return NULL;
}



int main(){

    int x = 0, y = 0;

    void **ptr = NULL;
    
    
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
    printf("int** operation\n");
    int** ptr_1 = (int**) ptr;
    printf("int* operation\n");
    int* ptr_2 = (int*) *(ptr_1);
    printf("int operation\n");
    int ptr_3 = (int) *(ptr_2);

    printf("value in ptr: %d\n", ptr_3);


    return 0;
}
