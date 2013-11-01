/*
 * File: mcs.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Implementation of an MCS lock
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




#ifndef _MCS_H_
#define _MCS_H_

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

typedef struct mcs_qnode {
    volatile uint8_t waiting;
    volatile struct mcs_qnode *volatile next;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    uint8_t padding[CACHE_LINE_SIZE - 16];
#endif
#endif
} mcs_qnode;

typedef volatile mcs_qnode *mcs_qnode_ptr;
typedef mcs_qnode_ptr mcs_lock; //initialized to NULL

typedef mcs_qnode* mcs_local_params;

typedef struct mcs_global_params {
    mcs_lock* the_lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif
} mcs_global_params;


/*
   Methods for easy lock array manipulation
   */

mcs_global_params* init_mcs_array_global(uint32_t num_locks);

mcs_qnode** init_mcs_array_local(uint32_t thread_num, uint32_t num_locks);

void end_mcs_array_local(mcs_qnode** the_qnodes, uint32_t size);

void end_mcs_array_global(mcs_global_params* the_locks, uint32_t size);
/*
   single lock manipulation
   */

int init_mcs_global(mcs_global_params* the_lock);

int init_mcs_local(uint32_t thread_num, mcs_qnode** the_qnode);

void end_mcs_local(mcs_qnode* the_qnodes);

void end_mcs_global(mcs_global_params the_locks);

/*
 *  Acquire and release methods
 */

void mcs_acquire(mcs_lock *the_lock, mcs_qnode_ptr I);

void mcs_release(mcs_lock *the_lock, mcs_qnode_ptr I);

int is_free_mcs(mcs_lock *L );

int mcs_trylock(mcs_lock *L, mcs_qnode_ptr I);
#endif
