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
#include <numa.h>
#endif
#include "utils.h"
#include "atomic_ops.h"

#define XSTR(s) #s
#define ALIGNMENT
//#define TEST_CAS

#ifdef __tile__
typedef volatile uint32_t data_type;
#else
typedef volatile uint8_t data_type;
#endif

#define DEFAULT_NUM_ENTRIES 1024
#define DEFAULT_NUM_THREADS 1
#define DEFAULT_DURATION 10000
#define DEFAULT_PAUSE 100 //pause between consecutive attemps to do an atomic operation
#define DEFAULT_SEED 0


__thread uint32_t phys_id;
int num_entries;
int num_threads;
int duration;
int seed;
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
    unsigned int seed;
    int id;
    char padding[CACHE_LINE_SIZE];
} thread_data_t;

void *test(void *data)
{
    thread_data_t *d = (thread_data_t *)data;
    phys_id = the_cores[d->id];
    set_cpu(phys_id);
    int rand_max;
    data_type old_data;
    data_type new_data;
    uint64_t res;
//#ifdef __sparc__
//#else
//    phys_id = d->id;
//#endif

    seeds = seed_rand();
    rand_max = num_entries - 1;

    /* Init of local data if necessary */

    /* Wait on barrier */
    barrier_cross(d->barrier);
    int entry;
   entry=0; 
    while (stop == 0) {
//    if (num_entries==1) {
            entry=0;
 //       } else {
//           entry =(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
//       }
    //   entry = (int)(erand48(seed) * rand_max) + rand_min;
#ifdef TEST_CAS
//    do {
if ((d->num_operations)&1) {
    res = CAS_U8(&(the_data[entry].data),1,0);
} else {
    res = CAS_U8(&(the_data[entry].data),0,1);
}
    //MEM_BARRIER;
//    } while(res!=0);
#elif defined(TEST_SWAP)
//    do {
if ((d->num_operations)&1) {
    res = SWAP_U8(&(the_data[entry].data),0);
} else {
    res = SWAP_U8(&(the_data[entry].data),1);
}
    //MEM_BARRIER;
//    } while(res!=0);
#elif defined(TEST_CTR)
    do {
    old_data=the_data[entry].data;
    new_data=old_data+1;
    } while (CAS_U8(&(the_data[entry].data),old_data,new_data)!=old_data);
    //MEM_BARRIER;
#elif defined(TEST_TAS)
//    do {
     res = TAS_U8(&(the_data[entry].data));
    //MEM_BARRIER;
    if (res==0) {
        the_data[entry].data = 0;
    }

//    } while (res!=0);
#elif defined(TEST_FAI)
    FAI_U8(&(the_data[entry].data));
  //  MEM_BARRIER;
#else
perror("No test primitive specified");
#endif 
//#ifdef XEON
//MEM_BARRIER;
//#endif
        d->num_operations++;
        if (op_pause>0) {
            cpause(op_pause);
            //cdelay(op_pause);
        }
    }

    /* Free any local data if necessary */ 

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
        {"seed",                      required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };

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
    seed = DEFAULT_SEED;
    op_pause = DEFAULT_PAUSE;


    sigset_t block_set;

    while(1) {
        i = 0;
        c = getopt_long(argc, argv, "he:d:p:n:s", long_options, &i);

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
                        "  stress_test [options...]\n"
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
                        "  -s, --seed <int>\n"
                        "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
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
            case 's':
                seed = atoi(optarg);
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

    if (seed == 0)
        srand((int)time(NULL));
    else
        srand(seed);

    stop = 0;
    /* Access set from all threads */
    barrier_init(&barrier, num_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Creating thread %d\n", i);
#endif
        data[i].id = i;
        data[i].num_operations = 0;
        data[i].seed = rand();
        data[i].barrier = &barrier;
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
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
    for (i = 0; i < num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Thread %d\n", i);
        printf("  #operations   : %lu\n", data[i].num_operations);
#endif
        operations += data[i].num_operations;
    }

#ifdef PRINT_OUTPUT
    printf("Duration      : %d (ms)\n", duration);
#endif
    printf("#operations     : %lu (%f / s)\n", operations, operations * 1000.0 / duration);


    free((data_t*) the_data);
    free(threads);
    free(data);

 
    return 0;
}
