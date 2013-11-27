/*
 * File: lock_if.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Common interface to various locking algorithms;
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




#ifdef USE_MCS_LOCKS
#include "mcs.h"
#elif defined(USE_HCLH_LOCKS)
#include "hclh.h"
#elif defined(USE_TTAS_LOCKS)
#include "ttas.h"
#elif defined(USE_SPINLOCK_LOCKS)
#include "spinlock.h"
#elif defined(USE_ARRAY_LOCKS)
#include "alock.h"
#elif defined(USE_RW_LOCKS)
#include "rw_ttas.h"
#elif defined(USE_CLH_LOCKS)
#include "clh.h"
#elif defined(USE_TICKET_LOCKS)
#include "ticket.h"
#elif defined(USE_MUTEX_LOCKS)
#include <pthread.h>
#elif defined(USE_HTICKET_LOCKS)
#include "htlock.h"
#else
#error "No type of locks given"
#endif

//lock globals
#ifdef USE_MCS_LOCKS
typedef mcs_global_params lock_global_data;
#elif defined(USE_HCLH_LOCKS)
typedef hclh_global_params lock_global_data;
#elif defined(USE_TTAS_LOCKS)
typedef ttas_lock_t  lock_global_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef spinlock_lock_t lock_global_data;
#elif defined(USE_ARRAY_LOCKS)
typedef lock_shared_t lock_global_data;
#elif defined(USE_CLH_LOCKS)
typedef clh_global_params lock_global_data;
#elif defined(USE_RW_LOCKS)
typedef rw_ttas lock_global_data;
#elif defined(USE_TICKET_LOCKS)
typedef ticketlock_t lock_global_data;
#elif defined(USE_MUTEX_LOCKS)
typedef pthread_mutex_t lock_global_data;
#elif defined(USE_HTICKET_LOCKS)
typedef htlock_t lock_global_data;
#endif

typedef lock_global_data* global_data;


//typedefs for thread local data
#ifdef USE_MCS_LOCKS
typedef mcs_local_params lock_local_data;
#elif defined(USE_HCLH_LOCKS)
typedef hclh_local_params lock_local_data;
#elif defined(USE_TTAS_LOCKS)
typedef unsigned int lock_local_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef unsigned int lock_local_data;
#elif defined(USE_ARRAY_LOCKS)
typedef array_lock_t lock_local_data;
#elif defined(USE_CLH_LOCKS)
typedef clh_local_params lock_local_data;
#elif defined(USE_RW_LOCKS)
typedef unsigned int  lock_local_data;
#elif defined(USE_TICKET_LOCKS)
typedef void* lock_local_data;//no local data for ticket locks
#elif defined(USE_MUTEX_LOCKS)
typedef void* lock_local_data;//no local data for mutexes
#elif defined(USE_HTICKET_LOCKS)
typedef void* lock_local_data;//no local data for hticket locks
#endif

typedef lock_local_data* local_data;

/*
 *  Declarations
 */

//lock acquire operation; in case of read write lock uses the writer lock
static inline void acquire_lock(lock_local_data* local_d, lock_global_data* global_d);

//acquisition of read lock; in case of non-rw lock, just implements exlusive access
static inline void acquire_read(lock_local_data* local_d, lock_global_data* global_d);

//acquisition of write lock; in case of non-rw lock, just implements exlusive access
static inline void acquire_write(lock_local_data* local_d, lock_global_data* global_d);

//trylock
static inline int acquire_trylock(lock_local_data* local_d, lock_global_data* global_d);

//lock release operation
//cluster_id is the cluster number of the core requesting the operation;
//e.g. the socket in the case of the Opteron
static inline void release_lock(lock_local_data* local_d, lock_global_data* global_d);

static inline void release_trylock(lock_local_data* local_d, lock_global_data* global_d);

//release reader lock
static inline void release_read(lock_local_data* local_d, lock_global_data* global_d);

//release writer lock
static inline void release_write(lock_local_data* local_d, lock_global_data* global_d);

//initialization of local data for an array of locks; core_to_pin is the core on which the thread is execting, 
static inline local_data init_lock_array_local(int core_to_pin, int num_locks, global_data the_locks);

//initialization of global data for an array of locks
static inline global_data init_lock_array_global(int num_locks, int num_threads);

//removal of global data for a lock array
static inline void free_lock_array_global(global_data the_locks, int num_locks);

//removal of local data for a lock array 
static inline void free_lock_array_local(local_data local_d, int num_locks);

//initialization of local data for a lock; core_to_pin is the core on which the thread is execting, 
static inline int init_lock_local(int core_to_pin, lock_global_data* the_locks, lock_local_data *the_data);

//initialization of global data for a lock
static inline int init_lock_global(lock_global_data* the_locks);

//removal of global data for a lock
static inline void free_lock_global(lock_global_data the_lock);

//removal of local data for a lock 
static inline void free_lock_local(lock_local_data local_d);


/*
 *  Functions
 */
static inline void acquire_lock(lock_local_data* local_d, lock_global_data* global_d) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_pred= (qnode*) hclh_acquire(local_d->my_queue,global_d->shared_queue,local_d->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(global_d, local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(global_d, local_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_pred= (clh_qnode*) clh_acquire(global_d->the_lock, local_d->my_qnode);
#elif defined(USE_RW_LOCKS)
    write_acquire(global_d,local_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(global_d);
#endif
}
static inline void acquire_write(lock_local_data* local_d, lock_global_data* global_d) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_pred= (qnode*) hclh_acquire(local_d->my_queue,global_d->shared_queue,local_d->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(global_d, local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(global_d, local_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_pred= (clh_qnode*) clh_acquire(global_d->the_lock, local_d->my_qnode);
#elif defined(USE_RW_LOCKS)
    write_acquire(global_d,local_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(global_d);
#endif
}

static inline void acquire_read(lock_local_data* local_d, lock_global_data* global_d) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_pred= (qnode*) hclh_acquire(local_d->my_queue,global_d->shared_queue,local_d->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(global_d, local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(global_d, local_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_pred= (clh_qnode*) clh_acquire(global_d->the_lock, local_d->my_qnode);
#elif defined(USE_RW_LOCKS)
    read_acquire(global_d,local_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(global_d);
#endif
}


static inline void release_lock(lock_local_data *local_d, lock_global_data *global_d) {
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_qnode=hclh_release(local_d->my_qnode,local_d->my_pred);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(global_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_qnode=clh_release(local_d->my_qnode, local_d->my_pred);
#elif defined(USE_RW_LOCKS)
    write_release(global_d); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(global_d);
#endif

}

static inline void release_write(lock_local_data *local_d, lock_global_data *global_d) {
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_qnode=hclh_release(local_d->my_qnode,local_d->my_pred);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(global_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_qnode=clh_release(local_d->my_qnode, local_d->my_pred);
#elif defined(USE_RW_LOCKS)
    write_release(global_d); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(global_d);
#endif

}

static inline void release_read(lock_local_data *local_d, lock_global_data *global_d) {
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_qnode=hclh_release(local_d->my_qnode,local_d->my_pred);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(global_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_qnode=clh_release(local_d->my_qnode, local_d->my_pred);
#elif defined(USE_RW_LOCKS)
    read_release(global_d); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(global_d);
#endif

}


static inline void set_cpu(int);

static inline local_data init_lock_array_local(int core_to_pin, int num_locks, global_data the_locks){
#ifdef USE_MCS_LOCKS
    return init_mcs_array_local(core_to_pin, num_locks);
#elif defined(USE_HCLH_LOCKS)
    return init_hclh_array_local(core_to_pin, num_locks, the_locks);
#elif defined(USE_TTAS_LOCKS)
    return init_ttas_array_local(core_to_pin, num_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_array_local(core_to_pin, num_locks);
#elif defined(USE_ARRAY_LOCKS)
    return init_alock_array_local(core_to_pin, num_locks, the_locks);
#elif defined(USE_RW_LOCKS)
    return init_rw_ttas_array_local(core_to_pin, num_locks);
#elif defined(USE_CLH_LOCKS)
    return init_clh_array_local(core_to_pin, num_locks);
#elif defined(USE_TICKET_LOCKS)
    init_thread_ticketlocks(core_to_pin);
    return NULL;
#elif defined(USE_MUTEX_LOCKS)
    //assign the thread to the correct core
    set_cpu(core_to_pin);
    return NULL;
#elif defined(USE_HTICKET_LOCKS)
    init_thread_htlocks(core_to_pin);
    return NULL;
#endif
}

static inline int init_lock_local(int core_to_pin,  lock_global_data* the_lock, lock_local_data* local_data){
#ifdef USE_MCS_LOCKS
    return init_mcs_local(core_to_pin, local_data);
#elif defined(USE_HCLH_LOCKS)
    return init_hclh_local(core_to_pin, the_lock, local_data);
#elif defined(USE_TTAS_LOCKS)
    return  init_ttas_local(core_to_pin, local_data);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_local(core_to_pin, local_data);
#elif defined(USE_ARRAY_LOCKS)
    return init_alock_local(core_to_pin, the_lock, local_data);
#elif defined(USE_RW_LOCKS)
    return init_rw_ttas_local(core_to_pin, local_data);
#elif defined(USE_CLH_LOCKS)
    return init_clh_local(core_to_pin, local_data);
#elif defined(USE_TICKET_LOCKS)
    init_thread_ticketlocks(core_to_pin);
    return 0;
#elif defined(USE_MUTEX_LOCKS)
    //assign the thread to the correct core
    set_cpu(core_to_pin);
    return 0;
#elif defined(USE_HTICKET_LOCKS)
    init_thread_htlocks(core_to_pin);
    return 0;
#endif
}

static inline void free_lock_local(lock_local_data local_d){
#ifdef USE_MCS_LOCKS
    end_mcs_local(local_d);
#elif defined(USE_HCLH_LOCKS)
    end_hclh_local(local_d);
#elif defined(USE_TTAS_LOCKS)
    //    end_ttas_array_local(local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    //    end_spinlock_array_local(local_d);
#elif defined(USE_ARRAY_LOCKS)
   // end_alock_local(local_d);
#elif defined(USE_CLH_LOCKS)
    end_clh_local(local_d);
#elif defined(USE_RW_LOCKS)
    //    end_rw_ttaslocal(local_d);
#elif defined(USE_TICKET_LOCKS)
    //nothing to be done
#elif defined(USE_MUTEX_LOCKS)
    //nothing to be done
#elif defined(USE_HTICKET_LOCKS)
    //nothing to be done
#endif
}

static inline void free_lock_array_local(local_data local_d, int num_locks){
#ifdef USE_MCS_LOCKS
    end_mcs_array_local(local_d,num_locks);
#elif defined(USE_HCLH_LOCKS)
    end_hclh_array_local(local_d,num_locks);
#elif defined(USE_TTAS_LOCKS)
    end_ttas_array_local(local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    end_spinlock_array_local(local_d);
#elif defined(USE_ARRAY_LOCKS)
    end_alock_array_local(local_d,num_locks);
#elif defined(USE_CLH_LOCKS)
    end_clh_array_local(local_d, num_locks);
#elif defined(USE_RW_LOCKS)
    end_rw_ttas_array_local(local_d);
#elif defined(USE_TICKET_LOCKS)
    //nothing to be done
#elif defined(USE_MUTEX_LOCKS)
    //nothing to be done
#elif defined(USE_HTICKET_LOCKS)
    //nothing to be done
#endif
}

static inline global_data init_lock_array_global(int num_locks, int num_threads){
#ifdef USE_MCS_LOCKS
    return init_mcs_array_global(num_locks);
#elif defined(USE_HCLH_LOCKS)
    return init_hclh_array_global(num_locks);
#elif defined(USE_TTAS_LOCKS)
    return init_ttas_array_global(num_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_array_global(num_locks);
#elif defined(USE_ARRAY_LOCKS)
    return init_alock_array_global(num_locks, num_threads);
#elif defined(USE_RW_LOCKS)
    return init_rw_ttas_array_global(num_locks);
#elif defined(USE_TICKET_LOCKS)
    return init_ticketlocks(num_locks);
#elif defined(USE_CLH_LOCKS)
    return init_clh_array_global(num_locks);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_t * the_locks;
    the_locks = (pthread_mutex_t*) malloc(num_locks * sizeof(pthread_mutex_t));
    int i;
    for (i=0;i<num_locks;i++) {
        pthread_mutex_init(&the_locks[i], NULL);
    }
    return the_locks;
#elif defined(USE_HTICKET_LOCKS)
    return init_htlocks(num_locks);
#endif
}

static inline int init_lock_global(lock_global_data* the_lock){
#ifdef USE_MCS_LOCKS
    return init_mcs_global(the_lock);
#elif defined(USE_HCLH_LOCKS)
    return init_hclh_global(the_lock);
#elif defined(USE_TTAS_LOCKS)
    return init_ttas_global(the_lock);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_global(the_lock);
#elif defined(USE_ARRAY_LOCKS)
    perror("operation not suported for array locks; use init_lock_global_nt instead");
    return 1;
#elif defined(USE_RW_LOCKS)
    return init_rw_ttas_global(the_lock);
#elif defined(USE_TICKET_LOCKS)
    return create_ticketlock(the_lock);
#elif defined(USE_CLH_LOCKS)
    return init_clh_global(the_lock);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_init(the_lock, NULL);
    return 0;
#elif defined(USE_HTICKET_LOCKS)
    return create_htlock(the_lock);
#endif
}

static inline int init_lock_global_nt(int num_threads, lock_global_data* the_lock) {
    #ifdef USE_ARRAY_LOCKS
        return init_alock_global(num_threads, the_lock);
    #else 
        return init_lock_global(the_lock);
    #endif
}

static inline void free_lock_array_global(global_data the_locks, int num_locks) {
#ifdef USE_MCS_LOCKS
    end_mcs_array_global(the_locks, num_locks);
#elif defined(USE_HCLH_LOCKS)
    end_hclh_array_global(the_locks, num_locks);
#elif defined(USE_TTAS_LOCKS)
    end_ttas_array_global(the_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    end_spinlock_array_global(the_locks);
#elif defined(USE_ARRAY_LOCKS)
    end_alock_array_global(the_locks, num_locks);
#elif defined(USE_RW_LOCKS)
    end_rw_ttas_array_global(the_locks);
#elif defined(USE_CLH_LOCKS)
    end_clh_array_global(the_locks, num_locks);
#elif defined(USE_TICKET_LOCKS)
    free_ticketlocks(the_locks);
#elif defined(USE_MUTEX_LOCKS)
    int i;
    for (i=0;i<num_locks;i++) {
        pthread_mutex_destroy(&the_locks[i]);
    }
#elif defined(USE_HTICKET_LOCKS)
    free_htlocks(the_locks);
#endif
}

static inline void free_lock_global(lock_global_data the_lock) {
#ifdef USE_MCS_LOCKS
    end_mcs_global(the_lock);
#elif defined(USE_HCLH_LOCKS)
    end_hclh_global(the_lock);
#elif defined(USE_TTAS_LOCKS)
    end_ttas_global(the_lock);
#elif defined(USE_SPINLOCK_LOCKS)
    end_spinlock_global(the_lock);
#elif defined(USE_ARRAY_LOCKS)
    //end_alock_global(the_lock);
#elif defined(USE_RW_LOCKS)
    end_rw_ttas_global(the_lock);
#elif defined(USE_CLH_LOCKS)
    end_clh_global(the_lock);
#elif defined(USE_TICKET_LOCKS)
    //free_ticketlocks(the_lock);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_destroy(&the_lock);
#elif defined(USE_HTICKET_LOCKS)
    //
#endif
}


//checks whether the lock is free; if it is, acquire it;
//we use this in memcached to simulate trylocks
//return 0 on success, 1 otherwise
static inline int acquire_trylock( lock_local_data* local_d, lock_global_data* global_d) {
#ifdef USE_MCS_LOCKS
    return mcs_trylock(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    perror("trylock not supported for hclh locks");
//    lock_local_data ld = *local_d;
//    if (is_free_hclh(ld->my_queue,(*global_d)->shared_queue,ld->my_qnode)) {
//        (*local_d)->my_pred= (qnode*) hclh_acquire(ld->my_queue,(*global_d)->shared_queue,ld->my_qnode);
//        return 0;
//    }
    return 1;
#elif defined(USE_TTAS_LOCKS)
    return ttas_trylock(global_d, local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    return spinlock_trylock(global_d, local_d);
#elif defined(USE_ARRAY_LOCKS)
    return alock_trylock(local_d);
#elif defined(USE_RW_LOCKS)
    return rw_trylock(global_d,local_d);
#elif defined(USE_TICKET_LOCKS)
    return ticket_trylock(global_d);
#elif defined(USE_CLH_LOCKS)
    perror("trylock not supported for clh locks");
//    return clh_trylock(global_d->the_lock,local_d->my_qnode);
    return 1;
#elif defined(USE_MUTEX_LOCKS)
    return pthread_mutex_trylock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    if (htlock_trylock(global_d)) return 0;
    return 1;
#endif
}

static inline void release_trylock(lock_local_data* local_d, lock_global_data* global_d) {
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock,*local_d);
#elif defined(USE_HCLH_LOCKS)
    local_d->my_qnode=hclh_release(local_d->my_qnode,local_d->my_pred);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(global_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d);
#elif defined(USE_RW_LOCKS)
    write_release(global_d); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_CLH_LOCKS)
    local_d->my_qnode=clh_release(local_d->my_qnode,local_d->my_pred);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_HTICKET_LOCKS)
    htlock_release_try(global_d);
#endif
}

