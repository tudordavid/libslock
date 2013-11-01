/*
* File: sample_mcs.c
* Author: Tudor David <tudor.david@epfl.ch>
*
* Description: 
*      Simple sample showing how the interface of a particular lock can be used.
*
* The MIT License (MIT)
*
* Copyright (c) 2013 Tudor David
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define NUM_THREADS 4

#include "atomic_ops.h" //the memory barriers are defined there
#include "mcs.h"

/* global data */
mcs_global_params the_lock;

void *do_something(void *id)
{
    int* my_core = (int*) id; 
    /* local data */
    mcs_local_params my_data;
    /*initialize this thread's local data*/
    init_mcs_local(*my_core, &my_data);
    MEM_BARRIER;


    /*acquire the lock*/
    mcs_acquire(the_lock.the_lock,my_data);
    printf("I have the lock\n");
    /*release the lock*/
    mcs_release(the_lock.the_lock,my_data);


    MEM_BARRIER;
    /*free internal memory structures which may have been allocated for the local data*/
    end_mcs_local(my_data);

    return NULL;

}

int main(int argc, char *argv[])
{
    pthread_t threads[NUM_THREADS];
    long t;

    /*initialize the global data*/
    init_mcs_global(&the_lock); 
    int ids[]={0,1,2,3};

    MEM_BARRIER;

    for(t=0;t<NUM_THREADS;t++){
        printf("In main: creating thread %ld\n", t);
        if (pthread_create(&threads[t], NULL, *do_something, &ids[t])!=0){
            fprintf(stderr,"Error creating thread\n");
            exit(-1);
        }
    }

    for (t = 0; t < NUM_THREADS; t++) {
        if (pthread_join(threads[t], NULL) != 0) {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    /*free internal memory strucutres which may have been allocated for this lock */
    end_mcs_global(the_lock);

    pthread_exit(NULL);
}
