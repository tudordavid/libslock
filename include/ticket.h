/*
 * File: ticket.h
 * Author: Tudor David <tudor.david@epfl.ch>, Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *
 * Description: 
 *      An implementation of a ticket lock with:
 *       - proportional back-off optimization
 *       - pretetchw for write optitization for the AMD Opteron
 *           Magny-Cours processors
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David, Vasileios Trigonakis
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


#ifndef _TICKET_H_
#define _TICKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#if defined(PLATFORM_NUMA)
#  include <numa.h>
#endif
#include <pthread.h>
#include "utils.h"
#include "atomic_ops.h"

/* setting of the back-off based on the length of the queue */
#define TICKET_BASE_WAIT 512
#define TICKET_MAX_WAIT  4095
#define TICKET_WAIT_NEXT 128

#define TICKET_ON_TW0_CLS 0	/* Put the head and the tail on separate 
                               cache lines (O: not, 1: do)*/
typedef struct ticketlock_t 
{
    volatile uint32_t head;
#if TICKET_ON_TW0_CLS == 1
    uint8_t padding0[CACHE_LINE_SIZE - 4];
#endif
    volatile uint32_t tail;
#ifdef ADD_PADDING
    uint8_t padding1[CACHE_LINE_SIZE - 8];
#  if TICKET_ON_TW0_CLS == 1
    uint8_t padding2[4];
#  endif
#endif
} ticketlock_t;



int ticket_trylock(ticketlock_t* lock);
void ticket_acquire(ticketlock_t* lock);
void ticket_release(ticketlock_t* lock);
int is_free_ticket(ticketlock_t* t);

int create_ticketlock(ticketlock_t* the_lock);
ticketlock_t* init_ticketlocks(uint32_t num_locks);
void init_thread_ticketlocks(uint32_t thread_num);
void free_ticketlocks(ticketlock_t* the_locks);

#if defined(MEASURE_CONTENTION)
extern void ticket_print_contention_stats(void);
double ticket_avg_queue(void);
#endif	/* MEASURE_CONTENTION */

#endif


