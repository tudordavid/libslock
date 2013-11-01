/*
 * File: hclh.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Implementation of a hierarchical CLH lock
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

#ifndef _HCLH_H_
#define _HCLH_H_

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

typedef struct node_fields {
    volatile uint8_t successor_must_wait;
    volatile uint8_t tail_when_spliced;
    volatile uint8_t cluster_id;
} node_fields;

typedef struct qnode {
    union {
        volatile uint32_t data;
        node_fields fields;
#ifdef ADD_PADDING
        volatile uint8_t padding[CACHE_LINE_SIZE];
#endif
    };
} qnode;

typedef volatile qnode *qnode_ptr;
typedef qnode_ptr local_queue;
typedef qnode_ptr global_queue;

//global parameters needed to oerate with a lock
typedef struct hclh_global_params {
    global_queue* shared_queue;
    local_queue** local_queues;
    volatile uint32_t* init_done;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    volatile uint8_t padding[CACHE_LINE_SIZE-20];
#endif
#endif

} hclh_global_params;

//thread local parameters
typedef struct hclh_local_params {
    qnode* my_qnode;
    qnode* my_pred;
    local_queue* my_queue;
} hclh_local_params;



/*
 *  Methods aiding with array of locks manipulation
 */

hclh_global_params* init_hclh_array_global(uint32_t num_locks);


hclh_local_params* init_hclh_array_local(uint32_t thread_num, uint32_t num_locks, hclh_global_params* the_params);


void end_hclh_array_local(hclh_local_params* local_params, uint32_t size);


void end_hclh_array_global(hclh_global_params* global_params, uint32_t size);

/*
 *single lock initialization and desctruction
 */
int init_hclh_global(hclh_global_params* the_lock);


int init_hclh_local(uint32_t thread_num, hclh_global_params* the_params, hclh_local_params* local_d);


void end_hclh_local(hclh_local_params local_params);


void end_hclh_global(hclh_global_params global_params);

/*
 *  Lock manipulation methods
 */

volatile qnode * hclh_acquire(local_queue *lq, global_queue *gq, qnode *my_qnode);

qnode * hclh_release(qnode *my_qnode, qnode * my_pred);


int is_free_hclh(local_queue *lq, global_queue *gq, qnode *my_qnode);

#endif
