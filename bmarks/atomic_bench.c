/*
* File: atomic_bench.c
* Author: Tudor David <tudor.david@epfl.ch>
*
* Description: 
*      This benchmark allows testing of various atomic instructions under dynamic conditions;
*      It allows:
*       - measuring the throughput in terms of atomic operation calls
*       - measuring the throughput in terms of successful operations
*       - measuring the atomic operation latencies under contention
*      
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



#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#ifndef __sparc__
#  include <numa.h>
#endif
#include "utils.h"
#include "atomic_ops.h"

#define XSTR(s) #s
#define ALIGNMENT

#if defined (__tile__) || defined (__sparc__)
typedef volatile uint32_t data_type;
#else
typedef volatile uint8_t data_type;
#endif

#define DEFAULT_NUM_ENTRIES 1024
#define DEFAULT_NUM_THREADS 1
#define DEFAULT_DURATION 10000
#define DEFAULT_PAUSE 100 //pause between consecutive attemps to do an atomic operation
#define DEFAULT_BENCHMARK 0

__thread uint32_t phys_id;
ticks correction;
int num_entries;
int num_threads;
int duration;
int benchmark;
int op_pause;

static volatile int stop;
__thread unsigned long * seeds;

typedef union {
    data_type data;
#ifdef ALIGNMENT
    volatile char padding[CACHE_LINE_SIZE];
#endif
} data_t;

__attribute__((aligned(CACHE_LINE_SIZE))) volatile data_t * the_data;

typedef struct barrier {
    pthread_cond_t complete;
    pthread_mutex_t mutex;
    int count;
    int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
    pthread_mutex_lock(&b->mutex);
    /* One more thread through */
    b->crossing++;
    /* If not all here, wait */
    if (b->crossing < b->count) {
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        pthread_cond_broadcast(&b->complete);
        /* Reset for next time */
        b->crossing = 0;
    }
    pthread_mutex_unlock(&b->mutex);
}

typedef struct thread_data {
    barrier_t *barrier;
    unsigned long num_operations;
    ticks total_time;
    unsigned long num_measured;
    int id;
    char padding[CACHE_LINE_SIZE];
} thread_data_t;

void *test_latency(void *data)
{
    thread_data_t *d = (thread_data_t *)data;
    phys_id = the_cores[d->id];
    set_cpu(phys_id);
    int rand_max;
#if defined(TEST_CTR)
    data_type old_data;
    data_type new_data;
#endif
    volatile uint64_t res;

    seeds = seed_rand();
    rand_max = num_entries - 1;

    unsigned long do_not_measure=0;
    int entry=0;
    ticks t1 = 0, t2 = 0;
    barrier_cross(d->barrier);

    while (stop == 0) {
        if (num_entries>1) {
            entry =(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
        }
        do_not_measure=(d->num_operations) & 0x1f; 
#ifdef TEST_CAS
        if ((d->num_operations)&1) { 
            res = CAS_U8(&(the_data[entry].data),1,0);
        } else {
            if (!do_not_measure) {
                t1=getticks();
#  ifdef __tile__
                MEM_BARRIER;
#  endif
                res = CAS_U8(&(the_data[entry].data),0,1);
#  ifdef __tile__
                MEM_BARRIER;
#  endif
                t2=getticks();

            } else {
                res = CAS_U8(&(the_data[entry].data),0,1);
            }
        }
#elif defined(TEST_SWAP)
        if ((d->num_operations)&1) {
            res = SWAP_U8(&(the_data[entry].data),0);
        } else {
            if (do_not_measure) {
                res = SWAP_U8(&(the_data[entry].data),1);
            } else {
                t1=getticks(); 
#  ifdef __tile__
                MEM_BARRIER;
#  endif
               res = SWAP_U8(&(the_data[entry].data),1);
#  ifdef __tile__
                MEM_BARRIER;
#  endif

                t2=getticks();
            }
        }
#elif defined(TEST_CTR)
        if (do_not_measure) {
            do {
                old_data=the_data[entry].data;
                new_data=old_data+1;
            } while (CAS_U8(&(the_data[entry].data),old_data,new_data)!=old_data);
        } else {
            t1=getticks();
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            do {
                old_data=the_data[entry].data;
                new_data=old_data+1;
            } while (CAS_U8(&(the_data[entry].data),old_data,new_data)!=old_data);
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            t2=getticks();
        }
#elif defined(TEST_TAS)
        if (do_not_measure) {
            res = TAS_U8(&(the_data[entry].data));
        } else {
            t1=getticks();
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            res = TAS_U8(&(the_data[entry].data));
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            t2=getticks();
        }
        if (res==0) {
            the_data[entry].data = 0;
        }
#elif defined(TEST_FAI)
        if (do_not_measure) {
            FAI_U8(&(the_data[entry].data));
        } else {
            t1=getticks();
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            FAI_U8(&(the_data[entry].data));
#  ifdef __tile__
                MEM_BARRIER;
#  endif
            t2=getticks();
        }
#else
        perror("No test primitive specified");
#endif 
        if (!do_not_measure) {
            d->num_measured++;
            d->total_time+=t2-t1-correction;
        }
        d->num_operations++;
        if (op_pause>0) {
            cpause(op_pause);
        }
    }
    
     /* avoid warning of unused var*/
    if (res == 12345654)
      {
	printf("%d", (int) res);
      }

    return NULL;
}

void *test_success(void *data)
{
    thread_data_t *d = (thread_data_t *)data;
    phys_id = the_cores[d->id];
    set_cpu(phys_id);
    int rand_max;
#if defined(TEST_CTR)
    data_type old_data;
    data_type new_data;
#endif
    uint64_t res;

    seeds = seed_rand();
    rand_max = num_entries - 1;

    barrier_cross(d->barrier);
    int entry=0;
    while (stop == 0) {
        if (num_entries>1) {
            entry =(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
        }
#ifdef TEST_CAS
        if ((d->num_operations)&1) {
            do {
                res = CAS_U8(&(the_data[entry].data),0,1);
            } while (res!=1);
        } else {
            do {
                res = CAS_U8(&(the_data[entry].data),1,0);
            } while (res!=0);
        }
#elif defined(TEST_SWAP)
#  ifdef __sparc__
        if ((d->num_operations)&1) {
            res = SWAP_U32(&(the_data[entry].data),0);
        } else {
            res = SWAP_U32(&(the_data[entry].data),1);
        }
#  else
        if ((d->num_operations)&1) {
            res = SWAP_U8(&(the_data[entry].data),0);
        } else {
            res = SWAP_U8(&(the_data[entry].data),1);
        }
#  endif
#elif defined(TEST_CTR)
        do {
            old_data=the_data[entry].data;
            new_data=old_data+1;
        } while (CAS_U8(&(the_data[entry].data),old_data,new_data)!=old_data);
#elif defined(TEST_TAS)
        do {
            res = TAS_U8(&(the_data[entry].data));
        } while (res!=0);
        MEM_BARRIER;
        the_data[entry].data = 0;
#elif defined(TEST_FAI)
        FAI_U8(&(the_data[entry].data));
#else
        perror("No test primitive specified");
#endif 
        d->num_operations++;
        if (op_pause>0) {
            cpause(op_pause);
        }
    }
    return NULL;
}


void *test_throughput(void *data)
{
    thread_data_t *d = (thread_data_t *)data;
    phys_id = the_cores[d->id];
    set_cpu(phys_id);
    int rand_max;
#if defined(TEST_CTR)
    data_type old_data;
    data_type new_data;
#endif
    volatile uint64_t res;

    seeds = seed_rand();
    rand_max = num_entries - 1;

    barrier_cross(d->barrier);
    int entry=0;
    while (stop == 0) {
        if (num_entries>1) {
            entry =(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
        }
#ifdef TEST_CAS
        if ((d->num_operations)&1) {
            res = CAS_U8(&(the_data[entry].data),1,0);
        } else {
            res = CAS_U8(&(the_data[entry].data),0,1);
        }
#elif defined(TEST_SWAP)
#  ifdef __sparc__
        if ((d->num_operations)&1) {
            res = SWAP_U32(&(the_data[entry].data),0);
        } else {
            res = SWAP_U32(&(the_data[entry].data),1);
        }

#  else
        if ((d->num_operations)&1) {
            res = SWAP_U8(&(the_data[entry].data),0);
        } else {
            res = SWAP_U8(&(the_data[entry].data),1);
        }
#  endif
#elif defined(TEST_CTR)
        do {
            old_data=the_data[entry].data;
            new_data=old_data+1;
        } while (CAS_U8(&(the_data[entry].data),old_data,new_data)!=old_data);
#elif defined(TEST_TAS)
        res = TAS_U8(&(the_data[entry].data));
        if (res==0) {
            the_data[entry].data = 0;
        }
#elif defined(TEST_FAI)
        FAI_U8(&(the_data[entry].data));
#else
        perror("No test primitive specified");
#endif 
        d->num_operations++;
        if (op_pause>0) {
            cpause(op_pause);
        }
    }

     /* avoid warning of unused var*/
    if (res == 12345654)
      {
	printf("%d", (int) res);
      }

    return NULL;
}


void catcher(int sig)
{
    static int nb = 0;
    printf("CAUGHT SIGNAL %d\n", sig);
    if (++nb >= 3)
        exit(1);
}



int main(int argc, char* const argv[])
{
    set_cpu(the_cores[0]);
#ifdef PRINT_OUTPUT
    fprintf(stderr, "The size of the data being tested: %lu\n",sizeof(data_type));
    fprintf(stderr, "Number of entries per cache line: %lu\n",CACHE_LINE_SIZE / sizeof(data_t));
#endif
    struct option long_options[] = {
        // These options don't set a flag
        {"help",                      no_argument,       NULL, 'h'},
        {"entries",                   required_argument, NULL, 'e'},
        {"duration",                  required_argument, NULL, 'd'},
        {"pause",                     required_argument, NULL, 'p'},
        {"num-threads",               required_argument, NULL, 'n'},
        {"benchmark",                      required_argument, NULL, 'b'},
        {NULL, 0, NULL, 0}
    };

    correction = getticks_correction_calc(); 
    int i, c;
    thread_data_t *data;
    pthread_t *threads;
    pthread_attr_t attr;
    barrier_t barrier;
    struct timeval start, end;
    struct timespec timeout;

    num_entries = DEFAULT_NUM_ENTRIES;
    num_threads = DEFAULT_NUM_THREADS;
    duration = DEFAULT_DURATION;
    benchmark = DEFAULT_BENCHMARK;
    op_pause = DEFAULT_PAUSE;

    sigset_t block_set;

    while(1) {
        i = 0;
        c = getopt_long(argc, argv, "he:d:p:n:b:", long_options, &i);

        if(c == -1)
            break;

        if(c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch(c) {
            case 0:
                /* Flag is automatically set */
                break;
            case 'h':
                printf("lock stress test\n"
                        "\n"
                        "Usage:\n"
                        "  atomic_bench [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -e, --entires <int>\n"
                        "        Number of entries in the test (default=" XSTR(DEFAULT_NUM_LOCKS) ")\n"
                        "  -d, --duration <int>\n"
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -p, --pause <int>\n"
                        "        Pause between consecutive atomic operations in cycles (default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -n, --num-threads <int>\n"
                        "        Number of threads (default=" XSTR(DEFAULT_NUM_THREADS) ")\n"
                        "  -b, --benchmark <int>\n"
                        "        benchmark to perform (0=throughput in atomic operation call, 1=throughput in successful atomic ops, 2=atomic op latency,  default=" XSTR(DEFAULT_BENCHMARK) ")\n"
                      );
                exit(0);
            case 'e':
                num_entries = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 'p':
                op_pause = atoi(optarg);
                break;
            case 'b':
                benchmark = atoi(optarg);
                break;
            case '?':
                printf("Use -h or --help for help\n");
                exit(0);
            default:
                exit(1);
        }
    }
    op_pause=op_pause/NOP_DURATION;
    num_entries = pow2roundup(num_entries);

    assert(duration >= 0);
    assert(num_entries >= 1);
    assert(num_threads > 0);

#ifdef PRINT_OUTPUT
    printf("Number of entries   : %d\n", num_entries);
    printf("Duration       : %d\n", duration);
    printf("Number of threads     : %d\n", num_threads);
    printf("Type sizes     : int=%d/long=%d/ptr=%d\n",
            (int)sizeof(int),
            (int)sizeof(long),
            (int)sizeof(void *));
#endif
    timeout.tv_sec = duration / 1000;
    timeout.tv_nsec = (duration % 1000) * 1000000;


    the_data = (data_t*)malloc(num_entries * sizeof(data_t));
    for (i = 0; i < num_entries; i++) {
        the_data[i].data=0;
    }

    if ((data = (thread_data_t *)malloc(num_threads * sizeof(thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    if ((threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    stop = 0;
    /* Access set from all threads */
    barrier_init(&barrier, num_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < num_threads; i++) {
        data[i].id = i;
        data[i].num_operations = 0;
        data[i].total_time=0;
        data[i].num_measured=0;
        data[i].barrier = &barrier;
    }


    void *(*test_function)(void*);

    switch(benchmark) {
        case 0:
            test_function = test_throughput;
            break;
        case 1:
            test_function = test_success;
            break;
        case 2: 
            test_function = test_latency;
            break;
        default:
            fprintf(stderr, "benchmark not correctly specified\n");
            exit(1);
    }

    for (i=0;i<num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Creating thread %d\n", i);
#endif
        if (pthread_create(&threads[i], &attr, test_function, (void *)(&data[i])) != 0) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }
    pthread_attr_destroy(&attr);

    /* Catch some signals */
    if (signal(SIGHUP, catcher) == SIG_ERR ||
            signal(SIGINT, catcher) == SIG_ERR ||
            signal(SIGTERM, catcher) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Start threads */
    barrier_cross(&barrier);

#ifdef PRINT_OUTPUT
    printf("STARTING...\n");
#endif
    gettimeofday(&start, NULL);
    if (duration > 0) {
        nanosleep(&timeout, NULL);
    } else {
        sigemptyset(&block_set);
        sigsuspend(&block_set);
    }
    stop = 1;
    gettimeofday(&end, NULL);
#ifdef PRINT_OUTPUT
    printf("STOPPING...\n");
#endif

    /* Wait for thread completion */
    for (i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

    unsigned long operations = 0;
    unsigned long total_measurements = 0;
    ticks total_ticks = 0;

    for (i = 0; i < num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Thread %d\n", i);
        printf("  #operations   : %lu\n", data[i].num_operations);
#endif
        operations += data[i].num_operations;
        if (benchmark==2) {
            total_ticks += data[i].total_time;
            total_measurements += data[i].num_measured;
        }
    }

    printf("Duration      : %d (ms)\n", duration);
    printf("#operations     : %lu (%f / s)\n", operations, operations * 1000.0 / duration);
    if (benchmark==2) {
        printf("average latency     : %lu\n", total_ticks / total_measurements);
    }
    free((data_t*) the_data);
    free(threads);
    free(data);

    return 0;
}
