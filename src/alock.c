/*
 * File: alock.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Array lock implementation
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




#include "alock.h"

int is_free_alock(lock_shared_t* the_lock) {
    if ((the_lock->flags[(the_lock->tail) % the_lock->size].flag) == (uint32_t)1) return 1;
    return 0;
}

int alock_trylock(array_lock_t* local_lock) {
    lock_shared_t *lock = local_lock->shared_data;
    uint32_t tail = lock->tail;
    if  (lock->flags[tail % lock->size].flag==1) {
        if (CAS_U32(&(lock->tail), tail, tail+1)==tail) {
            local_lock->my_index = tail % lock->size;
            return 0;
        }
    }
    return 1;
}

void alock_lock(array_lock_t* local_lock) 
{
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(local_lock);
    PREFETCHW(local_lock->shared_data);
#endif	/* OPTERON_OPTIMIZE */
    lock_shared_t *lock = local_lock->shared_data;
#ifdef __tile__
    MEM_BARRIER;
#endif
    uint32_t slot = FAI_U32(&(lock->tail)) % lock->size;
    local_lock->my_index = slot;

    volatile uint16_t* flag = &lock->flags[slot].flag;
#ifdef __tile__
    MEM_BARRIER;
#endif
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(flag);
#endif	/* OPTERON_OPTIMIZE */
    while (*flag == 0) 
    {
        PAUSE;
#if defined(OPTERON_OPTIMIZE)
        pause_rep(23);
        PREFETCHW(flag);
#endif	/* OPTERON_OPTIMIZE */
    }
}

void alock_unlock(array_lock_t* local_lock) 
{
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(local_lock);
    PREFETCHW(local_lock->shared_data);
#endif	/* OPTERON_OPTIMIZE */
    lock_shared_t *lock = local_lock->shared_data;
    uint32_t slot = local_lock->my_index;
    lock->flags[slot].flag = 0;
#ifdef __tile__
    MEM_BARRIER;
#endif
    COMPILER_BARRIER;
    lock->flags[(slot + 1)%lock->size].flag = 1;
}

/*
 *  Methods for array of locks manipulation
 */
lock_shared_t* init_alock_array_global(uint32_t num_locks, uint32_t num_processes) {
    uint32_t i;
    lock_shared_t* the_locks = (lock_shared_t*) calloc(num_locks, sizeof(lock_shared_t));
    for (i = 0; i < num_locks; i++) {
//        the_locks[i]=(lock_shared_t*)malloc(sizeof(lock_shared_t));
//        bzero((void*)the_locks[i],sizeof(lock_shared_t));
        the_locks[i].size = num_processes;
        the_locks[i].flags[0].flag=1;
        the_locks[i].tail=0;
    }
    MEM_BARRIER;
    return the_locks;
}

array_lock_t* init_alock_array_local(uint32_t thread_num, uint32_t num_locks, lock_shared_t* the_locks) {
    //assign the thread to the correct core
    set_cpu(thread_num);

    uint32_t i;
    array_lock_t* local_locks = (array_lock_t*) malloc(num_locks * sizeof(array_lock_t));
    for (i = 0; i < num_locks; i++) {
//        local_locks[i]=(array_lock_t*) malloc(sizeof(array_lock_t));
        local_locks[i].my_index=0;
        local_locks[i].shared_data = &(the_locks[i]);
    }
    MEM_BARRIER;
    return local_locks;
}

int init_alock_global(uint32_t num_processes, lock_shared_t* the_lock) {
    bzero((void*)the_lock,sizeof(lock_shared_t));
    the_lock->size = num_processes;
    the_lock->flags[0].flag=1;
    the_lock->tail=0;
    MEM_BARRIER;
    return 0;
}

int init_alock_local(uint32_t thread_num, lock_shared_t* the_lock, array_lock_t* local_lock) {
    //assign the thread to the correct core
    set_cpu(thread_num);

    local_lock->my_index=0;
    local_lock->shared_data = the_lock;
    MEM_BARRIER;
    return 0;
}

void end_alock_array_local(array_lock_t* local_locks, uint32_t size) {
    //uint32_t i;
    //for (i = 0; i < size; i++) {
    //    free(local_locks[i]);
    //}
    free(local_locks);
}

void end_alock_array_global(lock_shared_t* the_locks, uint32_t size) {
    //uint32_t i;
    //for (i = 0; i < size; i++) {
    //    free(the_locks[i]);
    //}
    free(the_locks); 
}

void end_alock_local(array_lock_t local_lock) {
    //free(local_lock);
}

void end_alock_global(lock_shared_t the_lock) {
    //free(the_lock); 
}

