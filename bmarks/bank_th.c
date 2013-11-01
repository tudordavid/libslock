
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <malloc.h>
#ifndef __sparc__
#include <numa.h>
#endif
#include "gl_lock.h"
#include "atomic_ops.h"
#include "utils.h"
#include "lock_if.h"

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define RO                              1
#define RW                              0

#define DEFAULT_DURATION                10000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_SEED                    0
#define DEFAULT_USE_LOCKS               1
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

__attribute__((aligned(CACHE_LINE_SIZE))) volatile int64_t * stop;
static global_lock gl;

int use_locks;
__thread unsigned long * seeds;
__thread uint32_t phys_id;
__thread uint32_t cluster_id;
volatile global_data the_locks;
__attribute__((aligned(CACHE_LINE_SIZE))) volatile local_data * local_th_data;

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account {
    int32_t number;
    int32_t balance;
#ifdef PAD_ACCOUNTS
    uint8_t padding[CACHE_LINE_SIZE-8];
#endif
} account_t;

typedef struct bank {
    __attribute__((aligned(CACHE_LINE_SIZE))) volatile account_t *accounts;
    int size;
} bank_t;



int read_accounts(volatile account_t *a1, volatile account_t *a2,  int thread_id)
{
    int amount=0;
    //lock ordering
    int n1=a1->number;
    int n2=a2->number;
    if (a1->number < a2->number) {
        n1=a2->number;
        n2=a1->number;
    }

    //local_lock_read(&gl);
    if (use_locks!=0){
    acquire_read(&local_th_data[thread_id][n1],&the_locks[n1]);
    acquire_read(&local_th_data[thread_id][n2],&the_locks[n2]);
    }
    amount+=a1->balance;
    amount+=a2->balance;
    if (use_locks!=0) {
    release_read(&local_th_data[thread_id][n2],&the_locks[n2]);
    release_read(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
    //local_unlock_read(&gl);

    return amount;
}


int transfer(volatile account_t *src, volatile account_t *dst, int amount, int thread_id)
{
    /* Allow overdrafts */
    //lock ordering
    int n1=src->number;
    int n2=dst->number;
    if (src->number < dst->number) {
        n1=dst->number;
        n2=src->number;
    }
    //local_lock_write(&gl);
    if (use_locks!=0) {
    acquire_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    acquire_write(&local_th_data[thread_id][n2],&the_locks[n2]);
    }
    src->balance-=amount;
    dst->balance+=amount;
    if (use_locks!=0) {
    release_write(&local_th_data[thread_id][n2],&the_locks[n2]);
    release_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
    //local_unlock_write(&gl);

    return amount;
}

int total(bank_t *bank, int transactional)
{
    int i, total;
    if (use_locks!=0) {
        global_acquire_read(&gl);
    }
    total = 0;
    for (i = 0; i < bank->size; i++) {
        total += bank->accounts[i].balance;
    }
    if (use_locks!=0) {
        global_unlock_read(&gl);
    }
    return total;
}

void reset(bank_t *bank)
{
    int i;
    if (use_locks!=0) {
        global_acquire_write(&gl);
    }
    for (i = 0; i < bank->size; i++) {
        bank->accounts[i].balance = 0;
    }
    if (use_locks!=0) {
        global_unlock_write(&gl);
    }
}

/* ################################################################### *
 * BARRIER
 * ################################################################### */

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

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  union
  {
    struct
    {
      bank_t *bank;
      barrier_t *barrier;
      unsigned long nb_transfer;
      unsigned long nb_read_all;
      unsigned long nb_write_all;
      unsigned int seed;
      int id;
      int read_all;
      int read_threads;
      int write_all;
      int write_threads;
      int disjoint;
      int nb_threads;
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} thread_data_t;

void *test(void *data)
{
    int src, dst, nb;
    int rand_max, rand_min;
    thread_data_t *d = (thread_data_t *)data;
    seeds = seed_rand();

//#ifdef __sparc__
    phys_id = the_cores[d->id];
    cluster_id = get_cluster(phys_id);
//#else
//    phys_id = d->id;
//#endif
    /* Prepare for disjoint access */
    if (d->disjoint) {
        rand_max = d->bank->size / d->nb_threads;
        rand_min = rand_max * d->id;
        if (rand_max <= 2) {
            fprintf(stderr, "can't have disjoint account accesses");
            return NULL;
        }
    } else {
        rand_max = d->bank->size - 1;
        rand_min = 0;
    }

    //lock local data
    //#ifdef USE_MCS_LOCKS
    //    mcs_qnode** the_qnodes;
    //#elif defined(USE_HCLH_LOCKS)
    //    hclh_local_params** local_params;
    //#elif defined(USE_TTAS_LOCKS)
    //    unsigned int * the limits;
    //#elif defined(USE_ARRAY_LOCKS)
    //    lock_t** local_locks;
    //#endif

    /* local initialization of locks */
    local_th_data[d->id] = init_lock_array_local(phys_id, d->bank->size, the_locks);

    /* Wait on barrier */
    barrier_cross(d->barrier);

    int n1, n2;
    int read_thresh = (d->read_all * 128) / 100;
    int read_write_thresh = ((d->read_all + d->write_all) * 128) / 100;

    while (stop[0] == 0) {
        if (d->id < d->read_threads) {
            /* Read all */
            total(d->bank, 1);
            d->nb_read_all++;
        } else if (d->id < d->read_threads + d->write_threads) {
            /* Write all */
            reset(d->bank);
            d->nb_write_all++;
        } else {
            //nb = (int)(erand48(seed) * 100);
            nb=(int) (my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & 0x7f);
            if (nb < read_thresh) {
                // /* Read all */
                //total(d->bank, 1);
                d->nb_read_all++;

                //actually, just read a couple of accounts
                n1=(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
                n2=(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[3])) & rand_max;

                // n1 = (int)(erand48(seed) * rand_max) + rand_min;
                // n2 = (int)(erand48(seed) * rand_max) + rand_min;
                if (n1 == n2)
                    n2 = ((n1 + 1) % rand_max) + rand_min;
                read_accounts(&d->bank->accounts[n1], &d->bank->accounts[n2], d->id);

            } else if (nb < read_write_thresh) {
                /* Write all */
                reset(d->bank);
                d->nb_write_all++;
            } else {
                /* Choose random accounts */
                src=(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
                dst=(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;

                //  src = (int)(erand48(seed) * rand_max) + rand_min;
                //  dst = (int)(erand48(seed) * rand_max) + rand_min;
                if (dst == src)
                    dst = ((src + 1) % rand_max) + rand_min;
                transfer(&d->bank->accounts[src], &d->bank->accounts[dst], 1, d->id);
                d->nb_transfer++;
            }
        }
    }
    /* Free locks */
    //free_local(local_th_data[d->id], d->bank->size);
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
        {"accounts",                  required_argument, NULL, 'a'},
        {"contention-manager",        required_argument, NULL, 'c'},
        {"duration",                  required_argument, NULL, 'd'},
        {"num-threads",               required_argument, NULL, 'n'},
        {"read-all-rate",             required_argument, NULL, 'r'},
        {"read-threads",              required_argument, NULL, 'R'},
        {"seed",                      required_argument, NULL, 's'},
        {"use-locks",                 required_argument, NULL, 'l'},
        {"write-all-rate",            required_argument, NULL, 'w'},
        {"write-threads",             required_argument, NULL, 'W'},
        {"disjoint",                  no_argument,       NULL, 'j'},
        {NULL, 0, NULL, 0}
    };

    bank_t *bank;
    int i, c;
    unsigned long reads, writes, updates;
    thread_data_t *data;
    pthread_t *threads;
    pthread_attr_t attr;
    barrier_t barrier;
    struct timeval start, end;
    struct timespec timeout;
    int duration = DEFAULT_DURATION;
    int nb_accounts = DEFAULT_NB_ACCOUNTS;
    int nb_threads = DEFAULT_NB_THREADS;
    int read_all = DEFAULT_READ_ALL;
    use_locks = DEFAULT_USE_LOCKS;
    int read_threads = DEFAULT_READ_THREADS;
    int seed = DEFAULT_SEED;
    int write_all = DEFAULT_WRITE_ALL;
    int write_threads = DEFAULT_WRITE_THREADS;
    int disjoint = DEFAULT_DISJOINT;


    sigset_t block_set;

    while(1) {
        i = 0;
        c = getopt_long(argc, argv, "ha:c:d:n:r:R:s:l:w:W:j", long_options, &i);

        if(c == -1)
            break;

        if(c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch(c) {
            case 0:
                /* Flag is automatically set */
                break;
            case 'h':
                printf("bank -- lock stress test\n"
                        "\n"
                        "Usage:\n"
                        "  bank [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -a, --accounts <int>\n"
                        "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
                        "  -d, --duration <int>\n"
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -n, --num-threads <int>\n"
                        "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
                        "  -l, --use-locks <int>\n"
                        "        Use locks or not (default=" XSTR(DEFAULT_USE_LOCKS) ")\n"
                        "  -r, --read-all-rate <int>\n"
                        "        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
                        "  -R, --read-threads <int>\n"
                        "        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
                        "  -s, --seed <int>\n"
                        "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
                        "  -w, --write-all-rate <int>\n"
                        "        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
                        "  -W, --write-threads <int>\n"
                        "        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
                        );
                exit(0);
            case 'a':
                nb_accounts = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg);
                break;
            case 'n':
                nb_threads = atoi(optarg);
                break;
            case 'r':
                read_all = atoi(optarg);
                break;
            case 'l':
                use_locks = atoi(optarg);
                break;
            case 'R':
                read_threads = atoi(optarg);
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'w':
                write_all = atoi(optarg);
                break;
            case 'W':
                write_threads = atoi(optarg);
                break;
            case 'j':
                disjoint = 1;
                break;
            case '?':
                printf("Use -h or --help for help\n");
                exit(0);
            default:
                exit(1);
        }
    }

    assert(duration >= 0);
    assert(nb_accounts >= 2);
    assert(nb_threads > 0);
    assert(read_all >= 0 && write_all >= 0 && read_all + write_all <= 100);
    assert(read_threads + write_threads <= nb_threads);

    nb_accounts = pow2roundup(nb_accounts);
//#ifdef PRINT_OUTPUT
    printf("Nb accounts    : %d\n", nb_accounts);
    printf("Duration       : %d\n", duration);
    printf("Nb threads     : %d\n", nb_threads);
    printf("Read-all rate  : %d\n", read_all);
    printf("Read threads   : %d\n", read_threads);
    printf("Seed           : %d\n", seed);
    printf("Use locks      : %d\n", use_locks);
    printf("Write-all rate : %d\n", write_all);
    printf("Write threads  : %d\n", write_threads);
    printf("Type sizes     : int=%d/long=%d/ptr=%d\n",
            (int)sizeof(int),
            (int)sizeof(long),
            (int)sizeof(void *));
//#endif
    timeout.tv_sec = duration / 1000;
    timeout.tv_nsec = (duration % 1000) * 1000000;

    if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }
    if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    if (seed == 0)
        srand((int)time(NULL));
    else
        srand(seed);

    bank = (bank_t *)malloc(sizeof(bank_t));
    bank->accounts = (account_t *)malloc(nb_accounts * sizeof(account_t));
    bank->size = nb_accounts;
    for (i = 0; i < bank->size; i++) {
        bank->accounts[i].number = i;
        bank->accounts[i].balance = 0;
    }

    gl.lock_data = 0;

    local_th_data = (local_data *)malloc(nb_threads*sizeof(local_data));

    stop = (int64_t*)memalign(CACHE_LINE_SIZE, 64);
    stop[0] = 0;
    /* Init locks */
#ifdef PRINT_OUTPUT
    printf("Initializing locks\n");
#endif
    the_locks = init_lock_array_global(nb_accounts, nb_threads);

    /* Access set from all threads */
    barrier_init(&barrier, nb_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < nb_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Creating thread %d\n", i);
#endif
        data[i].id = i;
        data[i].read_all = read_all;
        data[i].read_threads = read_threads;
        data[i].write_all = write_all;
        data[i].write_threads = write_threads;
        data[i].disjoint = disjoint;
        data[i].nb_threads = nb_threads;
        data[i].nb_transfer = 0;
        data[i].nb_read_all = 0;
        data[i].nb_write_all = 0;
        data[i].seed = rand();
        data[i].bank = bank;
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
    stop[0] = 1;
    gettimeofday(&end, NULL);
#ifdef PRINT_OUTPUT
    printf("STOPPING...\n");
#endif    

    /* Wait for thread completion */
    for (i = 0; i < nb_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
    reads = 0;
    writes = 0;
    updates = 0;
    for (i = 0; i < nb_threads; i++) {
#ifdef PRINT_OUTPUT
        printf("Thread %d\n", i);
        printf("  #transfer   : %lu\n", data[i].nb_transfer);
        printf("  #read-all   : %lu\n", data[i].nb_read_all);
        printf("  #write-all  : %lu\n", data[i].nb_write_all);
#endif
        updates += data[i].nb_transfer;
        reads += data[i].nb_read_all;
        writes += data[i].nb_write_all;
    }
//#ifdef PRINT_OUTPUT
    printf("Bank total    : %d (expected: 0)\n", total(bank, 0));
    printf("Duration      : %d (ms)\n", duration);
    printf("#read txs     : %lu ( %f / s)\n", reads, reads * 1000.0 / duration);
    printf("#write txs    : %lu ( %f / s)\n", writes, writes * 1000.0 / duration);
    printf("#update txs   : %lu ( %f / s)\n", updates, updates * 1000.0 / duration);
//#endif
    printf("#txs          : %lu ( %lu / s)\n", reads + writes + updates,(unsigned long)((reads + writes + updates) * 1000.0 / duration));
    /* Delete bank and accounts */
    free((void*) bank->accounts);
    free(bank);

    /* Cleanup locks */
    //free_global(the_locks, nb_accounts);

    //free(threads);
    //free(data);

    return 0;
}
