/*
 * File: spinlock.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Implementation of a simple test-and-set spinlock
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




#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

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
#include "atomic_ops.h"
#include "utils.h"

typedef volatile uint32_t spinlock_index_t;
#ifdef __tile__
typedef uint32_t spinlock_lock_data_t;
#else
typedef uint8_t spinlock_lock_data_t;
#endif

typedef struct spinlock_lock_t 
{
    union 
    {
        spinlock_lock_data_t lock;
#ifdef ADD_PADDING
        uint8_t padding[CACHE_LINE_SIZE];
#else
        uint8_t padding;
#endif
    };
} spinlock_lock_t;


/*
 *  Lock manipulation methods
 */

void spinlock_lock(spinlock_lock_t* the_lock, uint32_t* limits);

int spinlock_trylock(spinlock_lock_t* the_locks, uint32_t* limits);

void spinlock_unlock(spinlock_lock_t* the_locks);

int is_free_spinlock(spinlock_lock_t * the_lock);

/*
   Some methods for easy lock array manipluation
   */

spinlock_lock_t* init_spinlock_array_global(uint32_t num_locks);

uint32_t* init_spinlock_array_local(uint32_t thread_num, uint32_t size);

void end_spinlock_array_local(uint32_t* limits);

void end_spinlock_array_global(spinlock_lock_t* the_locks);

/*
 *  Methods for single lock manipulation
 */

int init_spinlock_global(spinlock_lock_t* the_lock);

int init_spinlock_local(uint32_t thread_num, uint32_t* limit);

void end_spinlock_local();

void end_spinlock_global();

#endif


