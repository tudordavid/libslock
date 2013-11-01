
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
#include "utils.h"
#include "atomic_ops.h"
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
#define DEFAULT_NUM_THREADS             1
#define DEFAULT_BALANCE_PERC            60
#define DEFAULT_WITHDRAW_PERC           20
#define DEFAULT_DEPOSIT_PERC            20
#define DEFAULT_SEED                    0
#define DEFAULT_USE_LOCKS               1
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile int stop;
static global_lock gl;

int use_locks;
__thread unsigned long* seeds;
__thread uint32_t phys_id;
__thread uint32_t cluster_id;

volatile global_data the_locks;
__attribute__((aligned(CACHE_LINE_SIZE))) volatile local_data * local_th_data;

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account 
{
  uint32_t number;
  int32_t balance;
} account_t;

typedef struct bank 
{
  account_t *accounts;
  uint32_t size;
} bank_t;



int
check(account_t *a1,  int thread_id)
{
  int amount=0;
  //lock ordering
  int n1=a1->number;

  //local_lock_read(&gl);
  if (use_locks != 0)
    {
      acquire_read(&local_th_data[thread_id][n1],&the_locks[n1]);
    }

  amount = a1->balance;

  if (use_locks != 0) 
    {
      release_read(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
  //local_unlock_read(&gl);

  return amount;
}


int
deposit(account_t *src, int amount, int thread_id)
{
  /* Allow overdrafts */
  //lock ordering
  int n1=src->number;
  //local_lock_write(&gl);
  if (use_locks!=0) 
    {
      acquire_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
  src->balance += amount;

  if (use_locks!=0)
    {
      release_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
  //local_unlock_write(&gl);

  return amount;
}

int
withdraw(account_t *src, int amount, int thread_id)
{
  /* Allow overdrafts */
  //lock ordering
  int n1=src->number;
  int successfull = 0;
  //local_lock_write(&gl);
  if (use_locks!=0) 
    {
      acquire_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    }

  if (src->balance >= amount)
    {
      src->balance -= amount;
      successfull = 1;
    }

  if (use_locks!=0)
    {
      release_write(&local_th_data[thread_id][n1],&the_locks[n1]);
    }
  //local_unlock_write(&gl);

  return successfull;
}

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier 
{
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

typedef struct thread_data 
{
  union
  {
    struct
    {
      bank_t *bank;
      barrier_t *barrier;
      unsigned long nb_balance;
      unsigned long nb_deposit;
      unsigned long nb_withdraw;
      unsigned int seed;
      int id;
      int balance_perc;
      int deposit_perc;
      int withdraw_perc;
      int nb_threads;
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} thread_data_t;

void *test(void *data)
{
  int rand_max;
  thread_data_t *d = (thread_data_t *)data;
  seeds = seed_rand();

//#ifdef __sparc__
    phys_id = the_cores[d->id];
    cluster_id = get_cluster(phys_id);
//#else
//    phys_id = d->id;
//#endif

  rand_max = d->bank->size - 1;

  /* local initialization of locks */
  local_th_data[d->id] = init_lock_array_local(phys_id, d->bank->size, the_locks);

  /* Wait on barrier */
  barrier_cross(d->barrier);

  while (stop == 0) 
    {
      uint32_t nb = (uint32_t) (my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) % 100);
      uint32_t acc = (uint32_t) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
      account_t* accp = &d->bank->accounts[acc];

      if (nb < d->deposit_perc) 
	{
	  deposit(accp, 1, d->id);
	  d->nb_deposit++;
	} 
      else if (nb < d->withdraw_perc) 
	{
	  withdraw(accp, 1, d->id);
	  d->nb_withdraw++;
	} 
      else	     /* nb < balance_perc */
	{
	  check(accp, d->id);
	  d->nb_balance++;
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


int 
main(int argc, char **argv)
{
    set_cpu(the_cores[0]);
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"accounts",                  required_argument, NULL, 'a'},
    {"duration",                  required_argument, NULL, 'd'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"check",                     required_argument, NULL, 'c'},
    {"deposit_perc",              required_argument, NULL, 'e'},
    {"servers",                   required_argument, NULL, 's'},
    {"withdraws",            required_argument, NULL, 'w'},
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
  int nb_threads = DEFAULT_NUM_THREADS;
  int duration = DEFAULT_DURATION;
  int nb_accounts = DEFAULT_NB_ACCOUNTS;
  int balance_perc = DEFAULT_BALANCE_PERC;
  int deposit_perc = DEFAULT_DEPOSIT_PERC;
  int seed = DEFAULT_SEED;
  int withdraw_perc = DEFAULT_WITHDRAW_PERC;

  sigset_t block_set;

  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "ha:c:d:n:r:e:s:w:W:j", long_options, &i);

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
	       "  -d, --duraiton <int>\n"
	       "        Duration of the test in ms (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
	       "  -n, --num-threads <int>\n"
	       "        Number of threads (default=" XSTR(DEFAULT_NUM_THREADS) ")\n"
	       "  -c, --check <int>\n"
	       "        Percentage of check balance transactions (default=" XSTR(DEFAULT_BALANCE_PERC) ")\n"
	       "  -e, --deposit_perc <int>\n"
	       "        Percentage of deposit transactions (default=" XSTR(DEFAULT_DEPOSIT_PERC) ")\n"
	       "  -w, --withdraws <int>\n"
	       "        Percentage of withdraw_perc transactions (default=" XSTR(DEFAULT_WITHDRAW_PERC) ")\n"
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
      case 'c':
	balance_perc = atoi(optarg);
	break;
      case 'e':
	deposit_perc = atoi(optarg);
	break;
      case 'w':
	withdraw_perc = atoi(optarg);
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
  assert(balance_perc >= 0 && withdraw_perc >= 0
	 && deposit_perc >= 0
	 && deposit_perc + balance_perc + withdraw_perc <= 100);

  nb_accounts = pow2roundup(nb_accounts);

  uint32_t missing = 100 - (deposit_perc + balance_perc + withdraw_perc);
  if (missing > 0)
    {
      balance_perc += missing;
    }

  printf("Nb accounts    : %d\n", nb_accounts);
  printf("Num ops        : %d\n", duration);
  printf("Nb threads     : %d\n", nb_threads);
  printf("Check balance  : %d\n", balance_perc);
  printf("Deposit        : %d\n", deposit_perc);
  printf("Withdraws      : %d\n", withdraw_perc);

  withdraw_perc += deposit_perc;
  balance_perc += withdraw_perc;

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

  stop = 0;
  /* Init locks */
  printf("Initializing locks\n");
  the_locks = init_lock_array_global(nb_accounts, nb_threads);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].id = i;
    data[i].nb_threads = nb_threads;
    data[i].nb_balance = 0;
    data[i].nb_deposit = 0;
    data[i].nb_withdraw = 0;
    data[i].balance_perc = balance_perc;
    data[i].deposit_perc = deposit_perc;
    data[i].withdraw_perc = withdraw_perc;

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

  printf("STARTING...\n");
  gettimeofday(&start, NULL);
  if (duration > 0) {
    nanosleep(&timeout, NULL);
  } else {
    sigemptyset(&block_set);
    sigsuspend(&block_set);
  }
  stop = 1;
  gettimeofday(&end, NULL);
  printf("STOPPING...\n");

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
    printf("Thread %d\n", i);
    printf("  #balance    : %lu\n", data[i].nb_balance);
    printf("  #withdraw   : %lu\n", data[i].nb_withdraw);
    printf("  #deposit    : %lu\n", data[i].nb_deposit);
    updates += data[i].nb_withdraw;
    reads += data[i].nb_balance;
    writes += data[i].nb_deposit;
  }
  /* printf("Bank total    : %d (expected: 0)\n", total(bank, 0)); */
  printf("Duration      : %d (ms)\n", duration);
  printf("#read txs     : %lu ( %f / s)\n", reads, reads * 1000.0 / duration);
  printf("#write txs    : %lu ( %f / s)\n", writes, writes * 1000.0 / duration);
  printf("#update txs   : %lu ( %f / s)\n", updates, updates * 1000.0 / duration);
  printf("#txs          : %lu ( %f / s)\n", reads + writes + updates, (reads + writes + updates) * 1000.0 / duration);
  /* Delete bank and accounts */
  free(bank->accounts);
  free(bank);

  /* Cleanup locks */
  //free_global(the_locks, nb_accounts);

  //free(threads);
  //free(data);

  return 0;
}
