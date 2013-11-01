/*
 * File: ticket.c
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

#include "ticket.h"

/* enable measure contantion to collect statistics about the 
   average queuing per lock acquisition */
#if defined(MEASURE_CONTENTION)
__thread uint64_t ticket_queued_total = 0;
__thread uint64_t ticket_acquires = 0;
#endif

static inline uint32_t
sub_abs(const uint32_t a, const uint32_t b)
{
  if (a > b)
    {
      return a - b;
    }
  else
    {
      return b - a;
    }
}

int
ticket_trylock(ticketlock_t* lock) 
{
  uint32_t me = lock->tail;
  uint32_t me_new = me + 1;
  uint64_t cmp = ((uint64_t) me << 32) + me_new; 
  uint64_t cmp_new = ((uint64_t) me_new << 32) + me_new; 
  uint64_t* la = (uint64_t*) lock;
  if (CAS_U64(la, cmp, cmp_new) == cmp) 
    {
      return 0;
    }
  return 1;
}

void
ticket_acquire(ticketlock_t* lock) 
{
  uint32_t my_ticket = IAF_U32(&(lock->tail));


#if defined(OPTERON_OPTIMIZE)
  uint32_t wait = TICKET_BASE_WAIT;
  uint32_t distance_prev = 1;
#  if defined(MEASURE_CONTENTION)
  uint8_t once = 1;
  ticket_acquires++;
#  endif

  while (1)
    {
      PREFETCHW(lock);
      uint32_t cur = lock->head;
      if (cur == my_ticket)
        {
	  break;
        }
      uint32_t distance = sub_abs(cur, my_ticket);

#  if defined(MEASURE_CONTENTION)
      if (once)
        {
	  ticket_queued_total += distance;
	  once = 0;
        }
#  endif

      if (distance > 1)
        {
	  if (distance != distance_prev)
            {
	      distance_prev = distance;
	      wait = TICKET_BASE_WAIT;
            }

	  nop_rep(distance * wait);
	  /* wait = (wait + TICKET_BASE_WAIT) & TICKET_MAX_WAIT; */
        }
      else
        {
	  nop_rep(TICKET_WAIT_NEXT);
        }

      if (distance > 20)
        {
	  sched_yield();
	  /* pthread_yield(); */
        }
    }

#else  /* !OPTERON_OPTIMIZE */
  /* backoff proportional to the distance would make sense even without the PREFETCHW */
  /* however, I did some tests on the Niagara and it performed worse */

#  if defined(__x86_64__)
#    if defined(MEASURE_CONTENTION)
  uint8_t once = 1;
  ticket_acquires++;
#    endif

  uint32_t wait = TICKET_BASE_WAIT;
  uint32_t distance_prev = 1;

  while (1)
    {
      uint32_t cur = lock->head;
      if (cur == my_ticket)
        {
	  break;
        }
      uint32_t distance = sub_abs(cur, my_ticket);

#  if defined(MEASURE_CONTENTION)
      if (once)
        {
	  ticket_queued_total += distance;
	  once = 0;
        }
#  endif

      if (distance > 1)
        {
	  if (distance != distance_prev)
            {
	      distance_prev = distance;
	      wait = TICKET_BASE_WAIT;
            }

	  nop_rep(distance * wait);
        }
      else
        {
	  nop_rep(TICKET_WAIT_NEXT);
        }

      if (distance > 20)
        {
	  sched_yield();
        }
    }
#  else
  while (lock->head != my_ticket)
    {
      PAUSE;
    }
#  endif
#endif	/* OPTERON_OPTIMIZE */
}

void
ticket_release(ticketlock_t* lock) 
{
#ifdef __tile__
  MEM_BARRIER;
#endif
#if defined(OPTERON_OPTIMIZE)
  PREFETCHW(lock);
#endif	/* OPTERON */
  COMPILER_BARRIER;
  lock->head++;
}


int create_ticketlock(ticketlock_t* the_lock) 
{
    the_lock->head=1;
    the_lock->tail=0;
    MEM_BARRIER;
    return 0;
}


int is_free_ticket(ticketlock_t* t)
{
  if ((t->head - t->tail) == 1) 
    {
      return 1;
    }
  return 0;
}

void init_thread_ticketlocks(uint32_t thread_num) 
{
  set_cpu(thread_num);
}

ticketlock_t* 
init_ticketlocks(uint32_t num_locks) 
{
  ticketlock_t* the_locks;
  the_locks = (ticketlock_t*) malloc(num_locks * sizeof(ticketlock_t));
  uint32_t i;
  for (i = 0; i < num_locks; i++) 
    {
      the_locks[i].head=1;
      the_locks[i].tail=0;
    }
  MEM_BARRIER;
  return the_locks;
}

void
free_ticketlocks(ticketlock_t* the_locks) 
{
  free(the_locks);
}


#if defined(MEASURE_CONTENTION)
void
ticket_print_contention_stats()
{
  double avg_q = ticket_queued_total / (double) ticket_acquires;
  printf("#Acquires: %10llu / #Total queuing: %10llu / Avg. queuing: %.3f\n",
	 (long long unsigned) ticket_acquires, (long long unsigned) ticket_queued_total, avg_q);
}

double
ticket_avg_queue()
{
  double avg_q = ticket_queued_total / (double) ticket_acquires;
  return avg_q;
}

#endif	/* MEASURE_CONTENTION */
