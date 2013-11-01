/*
 * File: alock.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      array based lock, as described in Herlihy and Shavit's "Art of Multiprocessor Programming"
 *      somewhat similar to clh, but requires more space, and needs an upper bound on the possible 
 *      number of processes
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

#ifndef _ALOCK_H_
#define _ALOCK_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifndef __sparc__
#include <numa.h>
#endif
#include <pthread.h>
#include "utils.h"
#include "atomic_ops.h"

/*
 *  this lock needs to know the maximum number of processes it can handle
 */
//TODO set this to a predefined value independent of the architecture?
#ifdef __sparc__
#define MAX_NUM_PROCESSES 64
#elif defined(__tile__)
#define MAX_NUM_PROCESSES 36
#elif defined(OPTERON)
#define MAX_NUM_PROCESSES 48
#else
#define MAX_NUM_PROCESSES 80
#endif

typedef struct flag_line {
    volatile uint16_t flag;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE-2];
#endif
} flag_t;

typedef struct lock_shared {
    volatile uint32_t tail;
    uint32_t size;
    flag_t flags[MAX_NUM_PROCESSES]; 
} lock_shared_t;

typedef struct lock {
    uint32_t my_index;
    lock_shared_t* shared_data;
} array_lock_t;


/*
 *lock array initalization and desctruction
 */
lock_shared_t* init_alock_array_global(uint32_t num_locks, uint32_t num_processes);

array_lock_t* init_alock_array_local(uint32_t thread_num, uint32_t num_locks, lock_shared_t* the_locks);

void end_alock_array_local(array_lock_t* local_locks, uint32_t size);

void end_alock_array_global(lock_shared_t* the_locks, uint32_t size);

/*
 *single lock initalization and desctruction
 */
int init_alock_global(uint32_t num_processes, lock_shared_t* the_lock);

int init_alock_local(uint32_t thread_num, lock_shared_t* the_lock, array_lock_t* my_lock);

void end_alock_local(array_lock_t local_lock);

void end_alock_global(lock_shared_t the_lock);


/*
 *  Lock manipulation functions
 */
void alock_lock(array_lock_t* lock);

void alock_unlock(array_lock_t* lock);

int alock_trylock(array_lock_t* local_lock);

int is_free_alock(lock_shared_t* the_lock);

#endif
