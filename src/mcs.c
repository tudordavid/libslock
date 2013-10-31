/*
 * File: mcs.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      MCS lock implementation
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




#include "mcs.h"

int mcs_trylock(mcs_lock *L, mcs_qnode_ptr I) {
    I->next=NULL;
#ifndef  __tile__
    if (CAS_PTR(L, NULL, I)==NULL) return 0;
    return 1;
#else
    MEM_BARRIER;
    if (CAS_PTR( L, NULL, I)==NULL) return 0;
    return 1;
#endif

}

void mcs_acquire(mcs_lock *L, mcs_qnode_ptr I) 
{
    I->next = NULL;
#ifndef  __tile__
    mcs_qnode_ptr pred = (mcs_qnode*) SWAP_PTR((volatile void*) L, (void*) I);
#else
    MEM_BARRIER;
    mcs_qnode_ptr pred = (mcs_qnode*) SWAP_PTR( L, I);
#endif
    if (pred == NULL) 		/* lock was free */
        return;
    I->waiting = 1; // word on which to spin
    MEM_BARRIER;
    pred->next = I; // make pred point to me

#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(I);
#endif	/* OPTERON_OPTIMIZE */
    while (I->waiting != 0) 
    {
        PAUSE;
#if defined(OPTERON_OPTIMIZE)
        pause_rep(23);
        PREFETCHW(I);
#endif	/* OPTERON_OPTIMIZE */
    }

}

void mcs_release(mcs_lock *L, mcs_qnode_ptr I) 
{
#ifdef __tile__
    MEM_BARRIER;
#endif

    mcs_qnode_ptr succ;
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(I);
#endif	/* OPTERON_OPTIMIZE */
    if (!(succ = I->next)) /* I seem to have no succ. */
    { 
        /* try to fix global pointer */
        if (CAS_PTR(L, I, NULL) == I) 
            return;
        do {
            succ = I->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
}

int is_free_mcs(mcs_lock *L ){
    if ((*L) == NULL) return 1;
    return 0;
}

/*
   Methods for easy lock array manipulation
   */

mcs_global_params* init_mcs_array_global(uint32_t num_locks) {
    uint32_t i;
    mcs_global_params* the_locks = (mcs_global_params*)malloc(num_locks * sizeof(mcs_global_params));
    for (i=0;i<num_locks;i++) {
        the_locks[i].the_lock=(mcs_lock*)malloc(sizeof(mcs_lock));
        *(the_locks[i].the_lock)=0;
    }
    MEM_BARRIER;
    return the_locks;
}


mcs_qnode** init_mcs_array_local(uint32_t thread_num, uint32_t num_locks) {
    set_cpu(thread_num);

    //init its qnodes
    uint32_t i;
    mcs_qnode** the_qnodes = (mcs_qnode**)malloc(num_locks * sizeof(mcs_qnode*));
    for (i=0;i<num_locks;i++) {
        the_qnodes[i]=(mcs_qnode*)malloc(sizeof(mcs_qnode));
    }
    MEM_BARRIER;
    return the_qnodes;

}

void end_mcs_array_local(mcs_qnode** the_qnodes, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(the_qnodes[i]);
    }
    free(the_qnodes);
}

void end_mcs_array_global(mcs_global_params* the_locks, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(the_locks[i].the_lock);
    }
    free(the_locks); 
}

int init_mcs_global(mcs_global_params* the_lock) {
    the_lock->the_lock=(mcs_lock*)malloc(sizeof(mcs_lock));
    *(the_lock->the_lock)=0;
    MEM_BARRIER;
    return 0;
}


int init_mcs_local(uint32_t thread_num, mcs_qnode** the_qnode) {
    set_cpu(thread_num);

    (*the_qnode)=(mcs_qnode*)malloc(sizeof(mcs_qnode));

    MEM_BARRIER;
    return 0;

}

void end_mcs_local(mcs_qnode* the_qnodes) {
    free(the_qnodes);
}

void end_mcs_global(mcs_global_params the_locks) {
    free(the_locks.the_lock);
}

