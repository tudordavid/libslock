/* 
 * File: ticket.h
 * Authors: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 *          Tudor David <tudor.david@epfl.ch>
 *
 * Description: an implementation of ticket lock with:
 *   - proportional back-off optimization
 *   - prefetchw for write optimization for the AMD Opteron
 *     magny-cours processors
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

ticketlock_t create_ticketlock();
ticketlock_t* init_ticketlocks(uint32_t num_locks);
void init_thread_ticketlocks(uint32_t thread_num);
void free_ticketlocks(ticketlock_t* the_locks);
#endif


