/*
 * File: htlock.h
 * Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *
 * Description: a numa-aware hierarchical teicket lock 
 *    The htlock contains N local ticket locks (N = number of memory
 *    nodes) and 1 global ticket lock. A thread always tries to acquire
 *    the local ticket lock first. If there isn't any (local) available,
 *    it enqueues for acquiring the global ticket lock and at the same
 *    time it "gives" NB_TICKETS_LOCAL tickets to the local ticket lock, 
 *    so that if more threads from the same socket try to acquire the lock,
 *    they will enqueue on the local lock, without even accessing the
 *    global one.      
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Vasileios Trigonakis
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

#ifndef _HTICKET_H_
#define _HTICKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifndef __sparc__
#  ifndef __tile__
#    include <numa.h>
#    include <emmintrin.h>
#  endif
#endif
#include <pthread.h>
#include <assert.h>
#include "utils.h"
#include "atomic_ops.h"

#define NB_TICKETS_LOCAL	128 /* max number of local tickets of local tickets
                                   before releasing global*/

typedef struct htlock_global
{
    volatile uint32_t nxt;
    volatile uint32_t cur;
    uint8_t padding[CACHE_LINE_SIZE - 8];
} htlock_global_t;

typedef struct htlock_local
{
    volatile int32_t nxt;
    volatile int32_t cur;
    uint8_t padding[CACHE_LINE_SIZE - 8];
} htlock_local_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) htlock
{
    htlock_global_t* global;
    htlock_local_t* local[NUMBER_OF_SOCKETS];
} htlock_t;

extern int create_htlock(htlock_t* htl);
extern void init_htlock(htlock_t* htl); /* initiliazes an htlock */
extern void init_thread_htlocks(uint32_t thread_num);
extern htlock_t* init_htlocks(uint32_t num_locks);
extern void free_htlocks(htlock_t* locks);


extern uint32_t is_free_hticket(htlock_t* htl);
extern void htlock_lock(htlock_t* l);
extern uint32_t htlock_trylock(htlock_t* l);

extern void htlock_release(htlock_t* l);
extern inline void htlock_release_try(htlock_t* l);	/* trylock rls */

    static inline void 
wait_cycles(uint64_t cycles)
{
    if (cycles < 256)
    {
        cycles /= 6;
        while (cycles--)
        {
            PAUSE;
        }
    }
    else
    {
        ticks _start_ticks = getticks();
        ticks _end_ticks = _start_ticks + cycles - 130;
        while (getticks() < _end_ticks);
    }
}

#endif	/* _HTICKET_H_ */


