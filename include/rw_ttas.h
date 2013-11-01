/*
 * File: rw_ttas.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Implementation of a test-and-test-and-set read-write-lock
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

#ifndef _RWTTAS_H_
#define _RWTTAS_H_

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

#define MAX_DELAY 1000

#ifdef __tile__
#define MAX_RW UINT32_MAX
#define W_MASK 0x100000000
typedef uint32_t rw_data_t;
typedef uint64_t all_data_t;
#else
#define MAX_RW UINT8_MAX
#define W_MASK 0x100
typedef uint8_t rw_data_t;
typedef uint16_t all_data_t;
#endif

typedef struct rw_ttas_data {
    volatile rw_data_t read_lock;
    volatile rw_data_t write_lock;
} rw_ttas_data;


typedef struct rw_ttas {
    union {
        rw_ttas_data rw;
        volatile all_data_t lock_data;
#ifdef ADD_PADDING
        uint8_t padding[CACHE_LINE_SIZE];
#endif
    };
} rw_ttas;

rw_ttas* init_rw_ttas_array_global(uint32_t num_locks);

uint32_t* init_rw_ttas_array_local(uint32_t thread_num, uint32_t size);

void end_rw_ttas_array_local(uint32_t* limits);

void end_rw_ttas_array_global(rw_ttas* the_locks);

int init_rw_ttas_global(rw_ttas* the_lock);

int init_rw_ttas_local(uint32_t thread_num, uint32_t* limit);

void end_rw_ttas_local();

void end_rw_ttas_global();


void read_acquire(rw_ttas* lock, uint32_t * limit);

void read_release(rw_ttas * lock);

void write_acquire(rw_ttas* lock, uint32_t * limit);

int rw_trylock(rw_ttas* lock, uint32_t* limit);
void write_release(rw_ttas * lock);

int is_free_rw(rw_ttas* lock);


#endif
