
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
#include "gl_lock.h"
#include "atomic_ops.h"
#include "utils.h"
#include "lock_if.h"

#define XSTR(s) #s

//number of concurres threads
#define DEFAULT_NUM_THREADS 1
//total number of locks
#define DEFAULT_NUM_LOCKS 2
//number of lock acquisitions in this test
#define DEFAULT_NUM_ACQ 10000
//delay between consecutive acquire attempts in cycles
#define DEFAULT_ACQ_DELAY 100
//delay between lock acquire and release in cycles
#define DEFAULT_ACQ_DURATION 10
//the total duration of a test
#define DEFAULT_DURATION 10000

static volatile int stop;

__thread uint32_t phys_id;
__thread uint32_t cluster_id;

volatile uint32_t tail;
volatile uint32_t head;

volatile global_data the_locks;
__attribute__((aligned(CACHE_LINE_SIZE))) volatile local_data * local_th_data;

typedef struct shared_data{
    char the_data[64];
} shared_data;

__attribute__((aligned(CACHE_LINE_SIZE))) volatile shared_data * some_data;
int duration;
int num_locks;
int num_threads;
int acq_duration;
int acq_delay;

ticks correction;
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
    unsigned long num_acquires;
    ticks acquire_time;
    ticks release_time;
    int id;
    char padding[CACHE_LINE_SIZE];
} thread_data_t;

void *test(void *data)
{
    thread_data_t *d = (thread_data_t *)data;

    phys_id = the_cores[d->id];
    cluster_id = get_cluster(phys_id);
    /* local initialization of locks */
    local_th_data[d->id] = init_lock_array_local(phys_id, num_locks, the_locks);

    barrier_cross(d->barrier);
    ticks begin;
    ticks begin_release;

    local_data local_d = local_th_data[d->id];
    while (stop == 0) {
        uint32_t my_ticket = IAF_U32(&tail);
        while (head != my_ticket) {
            PAUSE;
        }
        COMPILER_BARRIER;
        begin = getticks();
        COMPILER_BARRIER;
        acquire_lock(&local_d[1],&the_locks[1]);
        COMPILER_BARRIER;
        ticks end = getticks() - begin - correction;
        d->acquire_time+=end;
        COMPILER_BARRIER;
        begin_release = getticks();
        release_lock(&local_d[1],&the_locks[1]);
        MEM_BARRIER;
        COMPILER_BARRIER;
        d->release_time+=getticks() - begin_release - correction;

#ifdef PRINT_OUTPUT
        fprintf(stderr, "%d %llu\n",d->id, (unsigned long long int) end);
#endif
#ifdef __tile__
        MEM_BARRIER;
#endif
        COMPILER_BARRIER;
        head++;
        d->num_acquires++;
    }
    /* Free locks */
    free_lock_array_local(local_th_data[d->id], num_locks);
    if (acq_delay>0) {
            cpause(acq_delay);
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



int main(int argc, char **argv)
{
    set_cpu(the_cores[0]);
    struct option long_options[] = {
        // These options don't set a flag
        {"help",                      no_argument,       NULL, 'h'},
        {"locks",                     required_argument, NULL, 'l'},
        {"duration",                  required_argument, NULL, 'd'},
        {"num-threads",               required_argument, NULL, 'n'},
        {"acquire",                   required_argument, NULL, 'a'},
        {"pause",                     required_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}
    };

    correction = getticks_correction_calc(); 
    some_data = (shared_data*)malloc(4 * sizeof(shared_data));
    int i, c;
    thread_data_t *data;
    pthread_t *threads;
    pthread_attr_t attr;
    barrier_t barrier;
    struct timeval start, end;
    struct timespec timeout;
    duration = DEFAULT_DURATION;
    num_locks = DEFAULT_NUM_LOCKS;
    num_threads = DEFAULT_NUM_THREADS;
    acq_duration = DEFAULT_ACQ_DURATION;
    acq_delay = DEFAULT_ACQ_DELAY;

    head=1;
    tail=0;

    sigset_t block_set;

    while(1) {
        i = 0;
        c = getopt_long(argc, argv, "hl:d:n:a:p:", long_options, &i);

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
                        "  -l, --lcoks <int>\n"
                        "        Number of locks in the test (default=" XSTR(DEFAULT_NUM_LOCKS) ")\n"
                        "  -d, --duration <int>\n"
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -n, --num-threads <int>\n"
                        "        Number of threads (default=" XSTR(DEFAULT_NUM_THREADS) ")\n"
                        "  -a, --acquire <int>\n"
                        "        Number of cycles a lock is held (default=" XSTR(DEFAULT_ACQ_DURATION) ")\n"
                        "  -p, --pause <int>\n"
                        "        Number of cycles between a lock release and the next acquire (default=" XSTR(DEFAULT_ACQ_DELAY) ")\n"
                      );
                exit(0);
            case 'l':
                num_locks = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 'a':
                acq_duration = atoi(optarg);
                break;
            case 'p':
                acq_delay = atoi(optarg);
                break;
            case '?':
                printf("Use -h or --help for help\n");
                exit(0);
            default:
                exit(1);
        }
    }

    assert(duration >= 0);
    assert(num_locks >= 2);
    assert(num_threads > 0);
    assert(acq_duration >= 0);
    assert(acq_delay >= 0);
    acq_delay=acq_delay/NOP_DURATION;

#ifdef PRINT_OUTPUT
    printf("Number of locks    : %d\n", num_locks);
    printf("Duration       : %d\n", duration);
    printf("Number of threads     : %d\n", num_threads);
    printf("Lock is held for  : %d\n", acq_duration);
    printf("Delay between locks   : %d\n", acq_delay);
    printf("Type sizes     : int=%d/long=%d/ptr=%d\n",
            (int)sizeof(int),
            (int)sizeof(long),
            (int)sizeof(void *));
#endif
    timeout.tv_sec = duration / 1000;
    timeout.tv_nsec = (duration % 1000) * 1000000;

    if ((data = (thread_data_t *)malloc(num_threads * sizeof(thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }
    if ((threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    local_th_data = (local_data *)malloc(num_threads*sizeof(local_data));

    stop = 0;
    /* Init locks */
#ifdef PRINT_OUTPUT
    printf("Initializing locks\n");
#endif
    the_locks = init_lock_array_global(num_locks, num_threads);

    /* Access set from all threads */
    barrier_init(&barrier, num_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Creating thread %d\n", i);
#endif
        data[i].id = i;
        data[i].num_acquires = 0;
        data[i].acquire_time = 0;
        data[i].release_time = 0;
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

#ifdef PRINT_OUTPUT
    fprintf(stderr, "%d %d %d %d\n",some_data[0].the_data[1],some_data[1].the_data[2],some_data[2].the_data[3],some_data[3].the_data[4]);
#endif
    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

    unsigned long acquires = 0;
    ticks total_acquire = 0;
    ticks total_release = 0;
    for (i = 0; i < num_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Thread %d\n", i);
        printf("  #acquire   : %lu\n", data[i].num_acquires);
#endif
        total_acquire += data[i].acquire_time;
        total_release += data[i].release_time;
        acquires += data[i].num_acquires;

    }

#ifdef PRINT_OUTPUT
    printf("Duration      : %d (ms)\n", duration);
    printf("Average acquire duration: %lu (cycles)\n", total_acquire/acquires);
    printf("Acerage release duration: %lu(cycles)\n", total_release/acquires);
    printf("#acquires     : %lu (%f / s)\n", acquires, acquires * 1000.0 / duration);

#endif
    printf("%d %lu %lu\n",num_threads, total_acquire/acquires,total_release/acquires);
    /* Cleanup locks */
    free_lock_array_global(the_locks, num_locks);

    free(threads);
    free(data);

    return 0;
}
