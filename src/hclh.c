/*
 * File: hclh.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Hierarchical CLH lock implementation
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

#include "hclh.h"

__thread uint32_t hclh_node_mine;

uint16_t wait_for_grant_or_cluster_master(volatile qnode *q, uint8_t my_cluster) {
    qnode aux;
    while(1) 
    {
        aux.data=q->data;
        if ((aux.fields.cluster_id==my_cluster) && 
                (aux.fields.tail_when_spliced==0) && 
                (aux.fields.successor_must_wait==0)) 
            return 1;
        if (aux.fields.tail_when_spliced==1) 
            return 0;
        if (aux.fields.cluster_id!=my_cluster) 
            return 0;
        PAUSE;
    } 
}

volatile qnode * hclh_acquire(local_queue *lq, global_queue *gq, qnode *my_qnode) {
    volatile qnode* my_pred;
    do 
    {
#if defined(OPTERON_OPTIMIZE)
        PREFETCHW(lq);
#endif	/* OPTERON_OPTIMIZE */
        my_pred = *lq;
    }  while (CAS_PTR(lq, my_pred, my_qnode)!=my_pred);

    if (my_pred != NULL) 
    {
        uint16_t i_own_lock = wait_for_grant_or_cluster_master(my_pred, my_qnode->fields.cluster_id);
        if (i_own_lock) 
        {
            return my_pred;
        }
    }
    PAUSE;  PAUSE;

    volatile qnode * local_tail;
    do 
    {
#if defined(OPTERON_OPTIMIZE)
        PREFETCHW(gq);
        PREFETCHW(lq);
#endif	/* OPTERON_OPTIMIZE */
        my_pred = *gq;
        local_tail = *lq;
        PAUSE;
    } while(CAS_PTR(gq, my_pred, local_tail)!=my_pred);

    local_tail->fields.tail_when_spliced = 1;
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(my_pred);
#endif	/* OPTERON_OPTIMIZE */
    while (my_pred->fields.successor_must_wait) {
        PAUSE;
#if defined(OPTERON_OPTIMIZE)
        pause_rep(23);
        PREFETCHW(my_pred);
#endif	/* OPTERON_OPTIMIZE */
    }
    return my_pred;
}

int is_free_hclh(local_queue *lq, global_queue *gq, qnode *my_qnode) {
    if ((*lq)!=NULL) {
        qnode aux;
        aux.data=(*lq)->data;
        if ((aux.fields.cluster_id==my_qnode->fields.cluster_id) && 
                (aux.fields.tail_when_spliced==0) && 
                (aux.fields.successor_must_wait==0)) 
            return 1;
    }
    if ((*gq)->fields.successor_must_wait==0) return 1;
    return 0;
}

qnode* hclh_release(qnode *my_qnode, qnode * my_pred) {
    my_qnode->fields.successor_must_wait = 0;
    qnode* pr = my_pred;
    qnode new_node;
    new_node.data=0;
    new_node.fields.cluster_id=hclh_node_mine;
    new_node.fields.successor_must_wait = 1;
    new_node.fields.tail_when_spliced=0;

#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(pr);
#endif	/* OPTERON_OPTIMIZE */
    uint32_t old_data = pr->data;
    while (CAS_U32(&pr->data,old_data,new_node.data)!=old_data) 
    {
        old_data=pr->data; 
        PAUSE;
#if defined(OPTERON_OPTIMIZE)
        PREFETCHW(pr);
#endif	/* OPTERON_OPTIMIZE */
    }
    my_qnode=pr;
    return my_qnode;
}

/*
 *  Methods aiding with array of locks manipulation
 */

#define INIT_VAL 123

hclh_global_params* init_hclh_array_global(uint32_t num_locks) {
    hclh_global_params* the_params;
    the_params = (hclh_global_params*)malloc(num_locks * sizeof(hclh_global_params));
    uint32_t i;
    for (i=0;i<num_locks;i++) {
        //the_params[i]=(hclh_global_params*)malloc(sizeof(hclh_global_params));
        the_params[i].local_queues = (local_queue**)malloc(NUMBER_OF_SOCKETS*sizeof(local_queue*));
        the_params[i].init_done=(uint32_t*)malloc(NUMBER_OF_SOCKETS * sizeof(uint32_t));
        the_params[i].shared_queue = (global_queue*)malloc(sizeof(global_queue));
        qnode * a_node = (qnode *) malloc(sizeof(qnode));
        a_node->data=0;
        a_node->fields.cluster_id = NUMBER_OF_SOCKETS+1;
        *(the_params[i].shared_queue) = a_node;
    }
    MEM_BARRIER;
    return the_params;
}


hclh_local_params* init_hclh_array_local(uint32_t phys_core, uint32_t num_locks, hclh_global_params* the_params) {
    //assign the thread to the correct core
    set_cpu(phys_core);
    hclh_local_params* local_params;
    local_params = (hclh_local_params*)malloc(num_locks * sizeof(hclh_local_params));
    uint32_t i;
#ifdef XEON
    MEM_BARRIER;
    uint32_t real_core_num = 0;
    for (i = 0; i < (NUMBER_OF_SOCKETS * CORES_PER_SOCKET); i++) {
        if (the_cores[i]==phys_core) {
            real_core_num = i;
            break;
        }
    }
    phys_core=real_core_num;
    MEM_BARRIER;
#endif
    hclh_node_mine = phys_core/CORES_PER_SOCKET;
    for (i = 0; i < num_locks; i++) {
        //local_params[i]=(hclh_local_params*) malloc(sizeof(hclh_local_params));
        local_params[i].my_qnode = (qnode*) malloc(sizeof(qnode));
        local_params[i].my_qnode->data = 0;
        local_params[i].my_qnode->fields.cluster_id  = phys_core/CORES_PER_SOCKET;
        local_params[i].my_qnode->fields.successor_must_wait=1;
        local_params[i].my_pred = NULL;
        if (phys_core%CORES_PER_SOCKET==0) {
            the_params[i].local_queues[phys_core/CORES_PER_SOCKET] = (local_queue*)malloc(sizeof(local_queue));
            *(the_params[i].local_queues[phys_core/CORES_PER_SOCKET]) = NULL;
#ifdef __tile__
        MEM_BARRIER;
#endif
            the_params[i].init_done[phys_core/CORES_PER_SOCKET]=INIT_VAL;
        }
        while(the_params[i].init_done[phys_core/CORES_PER_SOCKET]!=INIT_VAL) {}
        local_params[i].my_queue = the_params[i].local_queues[phys_core/CORES_PER_SOCKET];
    }
    MEM_BARRIER;
    return local_params;
}

void end_hclh_array_local(hclh_local_params* local_params, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(local_params[i].my_qnode);
    }
    free(local_params);
}

void end_hclh_array_global(hclh_global_params* global_params, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(global_params[i].shared_queue);
        free(global_params[i].local_queues);
    }
    free(global_params); 
}

int init_hclh_global(hclh_global_params* the_params) {
   // hclh_global_params* the_params;
   // the_params=(hclh_global_params*)malloc(sizeof(hclh_global_params));
    the_params->local_queues = (local_queue**)malloc(NUMBER_OF_SOCKETS*sizeof(local_queue*));
    the_params->init_done=(uint32_t*)malloc(NUMBER_OF_SOCKETS * sizeof(uint32_t));
    the_params->shared_queue = (global_queue*)malloc(sizeof(global_queue));
    qnode * a_node = (qnode *) malloc(sizeof(qnode));
    a_node->data=0;
    a_node->fields.cluster_id = NUMBER_OF_SOCKETS+1;
    *(the_params->shared_queue) = a_node;
    MEM_BARRIER;
    return 0;
}


int init_hclh_local(uint32_t phys_core, hclh_global_params* the_params, hclh_local_params* local_params) {
    //assign the thread to the correct core
    set_cpu(phys_core);
#ifdef XEON
    MEM_BARRIER;
    uint32_t real_core_num = 0;
    int i;
    for (i = 0; i < (NUMBER_OF_SOCKETS * CORES_PER_SOCKET); i++) {
        if (the_cores[i]==phys_core) {
            real_core_num = i;
            break;
        }
    }
    phys_core=real_core_num;
    MEM_BARRIER;
#endif

    hclh_node_mine = phys_core/CORES_PER_SOCKET;
//    local_params=(hclh_local_params*) malloc(sizeof(hclh_local_params));
    local_params->my_qnode = (qnode*) malloc(sizeof(qnode));
    local_params->my_qnode->data = 0;
    local_params->my_qnode->fields.cluster_id  = phys_core/CORES_PER_SOCKET;
    local_params->my_qnode->fields.successor_must_wait=1;
    local_params->my_pred = NULL;
    if (phys_core%CORES_PER_SOCKET==0) {
        the_params->local_queues[phys_core/CORES_PER_SOCKET] = (local_queue*)malloc(sizeof(local_queue));
        *(the_params->local_queues[phys_core/CORES_PER_SOCKET]) = NULL;
#ifdef __tile__
        MEM_BARRIER;
#endif
        the_params->init_done[phys_core/CORES_PER_SOCKET]=INIT_VAL;
    }
    while(the_params->init_done[phys_core/CORES_PER_SOCKET]!=INIT_VAL) {}
    local_params->my_queue = the_params->local_queues[phys_core/CORES_PER_SOCKET];
    MEM_BARRIER;
    return 0;
}

void end_hclh_local(hclh_local_params local_params) {
    free(local_params.my_qnode);
}

void end_hclh_global(hclh_global_params global_params) {
    free(global_params.shared_queue);
    int i;
    for (i=0;i<sizeof(global_params.local_queues)/sizeof(global_params.local_queues[0]);i++) {
       free(global_params.local_queues[i]); 
    }
    free(global_params.local_queues);
}

