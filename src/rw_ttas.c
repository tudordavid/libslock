/*
 * File: rw_ttas.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Read-write test-and-test-and set implementation
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



#include "rw_ttas.h"

__thread unsigned long * rw_seeds;

int rw_trylock(rw_ttas* lock, uint32_t* limit) {
    if (CAS_U16(&lock->lock_data,0,W_MASK)==0) return 0;
    return 1;

}


void read_acquire(rw_ttas* lock, uint32_t* limit) {
    uint32_t delay;
    while (1) 
    {
        rw_data_t aux;
#if defined(OPTERON_OPTIMIZE)
        //      uint32_t t = 512;
        PREFETCHW(lock);
#endif  /* OPTERON_OPTIMIZE */
        while ((aux=lock->lock_data)>MAX_RW) 
        {
#if defined(OPTERON_OPTIMIZE)
            //	  uint32_t wt = (my_random(&(rw_seeds[0]),&(rw_seeds[1]),&(rw_seeds[2])) % t) + 1;
            //	  pause_rep(wt);
            PREFETCHW(lock);
            /* t+=16; */
            //	  t *= 4;
            //	  if (t > 102400)
            //	    {
            //	      t = 102400;
            //	    }
            PREFETCHW(lock);
#endif  /* OPTERON_OPTIMIZE */
        }
        //uint16_t aux = (uint16_t) lock->lock_data;
        if (CAS_U16(&lock->lock_data,aux,aux+1)==aux) {
            return;
        }
        else 
        {
            delay = my_random(&(rw_seeds[0]),&(rw_seeds[1]),&(rw_seeds[2]))%(*limit);
            *limit = MAX_DELAY > 2*(*limit) ? 2*(*limit) : MAX_DELAY;
            cdelay(delay);
        }
    }
}

void read_release(rw_ttas* lock) {
    DAF_U16(&(lock->lock_data));
}

void write_acquire(rw_ttas* lock, uint32_t* limit) {
    uint32_t delay;
    while (1) 
    {
#if defined(OPTERON_OPTIMIZE)
        //      uint32_t t = 512;
        PREFETCHW(lock);
#endif  /* OPTERON_OPTIMIZE */
        while (lock->lock_data!=0) 
        {
#if defined(OPTERON_OPTIMIZE)
            //	  uint32_t wt = (my_random(&(rw_seeds[0]),&(rw_seeds[1]),&(rw_seeds[2])) % t) + 1;
            //	  pause_rep(wt);
            PREFETCHW(lock);
            //	  t *= 4;
            //	  if (t > 102400)
            //	    {
            //	      t = 102400;
            //	    }
            //	  PREFETCHW(lock);
#endif  /* OPTERON_OPTIMIZE */
        }
        if (CAS_U16(&lock->lock_data,0,W_MASK)==0) {
            return;
        } 
        else {
            delay = my_random(&(rw_seeds[0]),&(rw_seeds[1]),&(rw_seeds[2]))%(*limit);
            *limit = MAX_DELAY > 2*(*limit) ? 2*(*limit) : MAX_DELAY;
            cdelay(delay);
        }

    }
}

void write_release(rw_ttas* lock) {
    COMPILER_BARRIER;
#ifdef __tile__
    MEM_BARRIER;
#endif

    lock->lock_data = 0;
}

int is_free_rw(rw_ttas* lock){
    if (lock->lock_data==0) return 1;
    return 0;
}

/*
 *  Some methods for easy lock array manipulation
 */
rw_ttas* init_rw_ttas_array_global(uint32_t num_locks) {
    rw_ttas* the_locks;
    the_locks = (rw_ttas*) malloc (num_locks * sizeof(rw_ttas));
    uint32_t i;
    for (i = 0; i < num_locks; i++) {
        the_locks[i].lock_data = 0;
    }
    MEM_BARRIER;
    return the_locks;
}

uint32_t* init_rw_ttas_array_local(uint32_t thread_num, uint32_t size){
    set_cpu(thread_num);
    rw_seeds = seed_rand();
    uint32_t* limits;
    limits = (uint32_t*)malloc(size * sizeof(uint32_t));
    uint32_t i;
    for (i = 0; i < size; i++) {
        limits[i]=1; 
    }
    MEM_BARRIER;
    return limits;
}

void end_rw_ttas_array_local(uint32_t* limits) {
    free(limits);
}

void end_rw_ttas_array_global(rw_ttas* the_locks) {
    free(the_locks);
}

int init_rw_ttas_global(rw_ttas* the_lock) {
    the_lock->lock_data=0;
    MEM_BARRIER;
    return 0;
}

int init_rw_ttas_local(uint32_t thread_num, uint32_t * limit){
    set_cpu(thread_num);
    *limit = 1;
    rw_seeds = seed_rand();
    MEM_BARRIER;
    return 0;
}

void end_rw_ttas_local() {
    //method not needed
}

void end_rw_ttas_global() {
    //method not needed
}

