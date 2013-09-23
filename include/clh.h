//clh lock
#ifndef _CLH_H_
#define _CLH_H_

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

typedef struct clh_qnode {
    volatile uint8_t locked;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 1];
#endif
} clh_qnode;

typedef volatile clh_qnode *clh_qnode_ptr;
typedef clh_qnode_ptr clh_lock;

typedef struct clh_local_params {
    clh_qnode* my_qnode;
    clh_qnode* my_pred;
} clh_local_params;


typedef struct clh_global_params {
    clh_lock* the_lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif
} clh_global_params;

//lock array creation and destruction methods
clh_global_params* init_clh_array_global(uint32_t num_locks);

clh_local_params* init_clh_array_local(uint32_t thread_num, uint32_t num_locks);

void end_clh_array_local(clh_local_params* the_params, uint32_t size);

void end_clh_array_global(clh_global_params* the_locks, uint32_t size);

//single lock creation and destruction methods
clh_global_params init_clh_global();

clh_local_params init_clh_local(uint32_t thread_num);

void end_clh_local(clh_local_params the_params);

void end_clh_global(clh_global_params the_lock);

//lock
volatile clh_qnode* clh_acquire(clh_lock* the_lock, clh_qnode* my_qnode);

//unlock
clh_qnode* clh_release(clh_qnode* my_qnode, clh_qnode* my_pred);

int clh_trylock(clh_lock * L, clh_qnode_ptr I);


#endif
