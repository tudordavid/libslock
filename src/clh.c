/*
 * File: clh.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Clh lock implementation
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



#include "clh.h"

int clh_trylock(clh_lock * L, clh_qnode_ptr I) {
    return 1;
}


volatile clh_qnode* clh_acquire(clh_lock *L, clh_qnode* I ) 
{
    I->locked=1;
#ifndef  __tile__
    clh_qnode_ptr pred = (clh_qnode*) SWAP_PTR((volatile void*) (L), (void*) I);
#else
    MEM_BARRIER;
    clh_qnode_ptr pred = (clh_qnode*) SWAP_PTR( L, I);
#endif
    if (pred == NULL) 		/* lock was free */
        return NULL;
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(pred);
#endif	/* OPTERON_OPTIMIZE */
    while (pred->locked != 0) 
    {
        PAUSE;
#if defined(OPTERON_OPTIMIZE)
        pause_rep(23);
        PREFETCHW(pred);
#endif	/* OPTERON_OPTIMIZE */
    }

    return pred;
}

clh_qnode* clh_release(clh_qnode *my_qnode, clh_qnode * my_pred) {
    COMPILER_BARRIER;
#ifdef __tile__
    MEM_BARRIER;
#endif
    my_qnode->locked=0;
    return my_pred;
}

clh_global_params* init_clh_array_global(uint32_t num_locks) {
    clh_global_params* the_params;
    the_params = (clh_global_params*)malloc(num_locks * sizeof(clh_global_params));
    uint32_t i;
    for (i=0;i<num_locks;i++) {
        the_params[i].the_lock=(clh_lock*)malloc(sizeof(clh_lock));
        clh_qnode * a_node = (clh_qnode *) malloc(sizeof(clh_qnode));
        a_node->locked=0;
        *(the_params[i].the_lock) = a_node;
    }
    MEM_BARRIER;
    return the_params;
}

clh_local_params* init_clh_array_local(uint32_t thread_num, uint32_t num_locks) {
    set_cpu(thread_num);

    //init its qnodes
    uint32_t i;
    clh_local_params* local_params = (clh_local_params*)malloc(num_locks * sizeof(clh_local_params));
    for (i=0;i<num_locks;i++) {
        local_params[i].my_qnode = (clh_qnode*) malloc(sizeof(clh_qnode));
        local_params[i].my_qnode->locked=0;
        local_params[i].my_pred = NULL;
    }
    MEM_BARRIER;
    return local_params;

}

void end_clh_array_local(clh_local_params* the_params, uint32_t size){
    free(the_params);
}

void end_clh_array_global(clh_global_params* the_locks, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(the_locks[i].the_lock);
    }
    free(the_locks);
}

int init_clh_global(clh_global_params* the_params) {
    the_params->the_lock=(clh_lock*)malloc(sizeof(clh_lock));
    clh_qnode * a_node = (clh_qnode *) malloc(sizeof(clh_qnode));
    a_node->locked=0;
    *(the_params->the_lock) = a_node;
    MEM_BARRIER;
    return 0;
}

int init_clh_local(uint32_t thread_num, clh_local_params* local_params) {
    set_cpu(thread_num);

    //init its qnodes
    local_params->my_qnode = (clh_qnode*) malloc(sizeof(clh_qnode));
    local_params->my_qnode->locked=0;
    local_params->my_pred = NULL;
    MEM_BARRIER;
    return 0;

}

void end_clh_local(clh_local_params the_params){
    //empty method
}

void end_clh_global(clh_global_params the_lock) {
    free(the_lock.the_lock);
}

