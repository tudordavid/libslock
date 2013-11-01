/* 
 * File: measure_contention.c
 * Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *
 * Description: measures the avg queuing per lock acquisition
 * using ticket locks
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
#include <numa.h>
#endif
#include "gl_lock.h"
#include "utils.h"
#include "lock_if.h"
#include "atomic_ops.h"

#include "ticket.h"

uint64_t c[2] = {0, 0};

#define STR(s)   #s
#define XSTR(s) STR(s)

#define PRINT_OUTPUT

//number of concurres threads
#define DEFAULT_NUM_THREADS 1
//total number of locks
#define DEFAULT_NUM_LOCKS 128
//number of lock acquisitions in this test
#define DEFAULT_NUM_ACQ 10000
//delay between consecutive acquire attempts in cycles
#define DEFAULT_ACQ_DELAY 0
//delay between lock acquire and release in cycles
#define DEFAULT_ACQ_DURATION 00
//delay between lock acquire and release in cycles
#define DEFAULT_CL_ACCESS 1
//the total duration of a test
#define DEFAULT_DURATION 10000
//if do_writes is 0, the test only reads cache lines, else it also writes them
#define DEFAULT_DO_WRITES 0
//default seed
#define DEFAULT_SEED 0

static volatile int stop;

__thread unsigned long* seeds;
__thread uint32_t phys_id;
__thread uint32_t cluster_id;
volatile global_data the_locks;
__attribute__((aligned(CACHE_LINE_SIZE))) volatile local_data* local_th_data;

typedef struct shared_data
{
  volatile char the_data[64];
} shared_data;

__attribute__((aligned(CACHE_LINE_SIZE))) volatile shared_data* protected_data;
__attribute__((aligned(CACHE_LINE_SIZE))) volatile uint32_t* protected_offsets;
int duration;
int num_locks;
int do_writes;
int num_threads;
int acq_duration;
int acq_delay;
int fair_delay;
int mutex_delay;
int cl_access;
int seed;


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

typedef struct thread_data 
{
  union
  {
    struct
    {
      barrier_t *barrier;
      unsigned long num_acquires;
      unsigned int seed;
      int id;
    };
    char padding[CACHE_LINE_SIZE];
  };
} thread_data_t;


double* avg_q_stats;

void*
test(void *data)
{
  int rand_max;
  thread_data_t *d = (thread_data_t *)data;
  phys_id = the_cores[d->id];
  cluster_id = get_cluster(phys_id);
  rand_max = num_locks - 1;

  seeds = seed_rand();

  /* local initialization of locks */
  local_th_data[d->id] = init_lock_array_local(phys_id, num_locks, the_locks);

  /* Wait on barrier */
  barrier_cross(d->barrier);

  int lock_to_acq;

  local_data local_d = local_th_data[d->id];
  while (stop == 0) 
    {
      //lock_to_acq= (int)(erand48(seed) * rand_max) + rand_min;
      if (num_locks==1) 
	{
	  lock_to_acq=0;
	} else 
	{
	  lock_to_acq=(int) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
	}
      acquire_lock(&local_d[lock_to_acq],&the_locks[lock_to_acq]);
      //#ifndef NO_DELAYS
      if (acq_duration > 0)
        {
	  cpause(acq_duration);
        }
      uint32_t i;
#ifndef NO_DELAYS
      for (i = 0; i < cl_access; i++)
	{
	  if (do_writes==1) 
	    {
	      protected_data[i + protected_offsets[lock_to_acq]].the_data[0]+=d->id;
	    } else 
	    {
	      protected_data[i + protected_offsets[lock_to_acq]].the_data[0]= d->id;
	    }
	}
#endif
      release_lock(&local_d[lock_to_acq],&the_locks[lock_to_acq]);
      if (acq_delay>0) 
	{
	  cpause(acq_delay);
	}

#if defined(USE_MUTEX_LOCKS)
      if (acq_delay>0)
        cpause(mutex_delay);
#endif
      d->num_acquires++;
    }


  ticket_print_contention_stats();
  avg_q_stats[d->id] = ticket_avg_queue();

  free_lock_array_local(local_th_data[d->id], num_locks);
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
  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,       NULL, 'h'},
      {"locks",                     required_argument, NULL, 'l'},
      {"duration",                  required_argument, NULL, 'd'},
      {"num-threads",               required_argument, NULL, 'n'},
      {"acquire",                   required_argument, NULL, 'a'},
      {"pause",                     required_argument, NULL, 'p'},
      {"do_writes",                 required_argument, NULL, 'w'},
      {"clines",                    required_argument, NULL, 'c'},
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
  duration = DEFAULT_DURATION;
  num_locks = DEFAULT_NUM_LOCKS;
  do_writes = DEFAULT_DO_WRITES;
  num_threads = DEFAULT_NUM_THREADS;
  acq_duration = DEFAULT_ACQ_DURATION;
  acq_delay = DEFAULT_ACQ_DELAY;
  cl_access = DEFAULT_CL_ACCESS;
  seed = DEFAULT_SEED;

  sigset_t block_set;

  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hl:d:n:w:a:p:c:s", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
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
		 "  -w, --do_writes <int>\n"
		 "        Whether or not the test writes cache lines (default=" XSTR(DEFAULT_DO_WRITES) ")\n"
		 "  -p, --pause <int>\n"
		 "        Number of cycles between a lock release and the next acquire (default=" XSTR(DEFAULT_ACQ_DELAY) ")\n"
		 "  -c, --clines <int>\n"
		 "        Number of cache lines written in every critical section (default=" XSTR(DEFAULT_CL_ACCESS) ")\n"
		 "  -s, --seed <int>\n"
		 "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
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
	case 'w':
	  do_writes = atoi(optarg);
	  break;
	case 'a':
#ifdef NO_DELAYS
#ifdef PRINT_OUTPUT
	  printf("*** the NO_DELAYS flag is set");
#endif
#endif
	  acq_duration = atoi(optarg);
	  break;
	case 'p':
#ifdef NO_DELAYS
#ifdef PRINT_OUTPUT
	  printf("*** the NO_DELAYS flag is set");
#endif
#endif
	  acq_delay = atoi(optarg);
	  break;
	case 'c':
	  cl_access = atoi(optarg);
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
  fair_delay=100;
  mutex_delay=(num_threads-1) * 30 / NOP_DURATION;
  fair_delay=fair_delay/NOP_DURATION;
  acq_delay=acq_delay/NOP_DURATION;
  acq_duration=acq_duration/NOP_DURATION;
  num_locks=pow2roundup(num_locks);
  assert(duration >= 0);
  assert(num_locks >= 1);
  assert(num_threads > 0);
  assert(acq_duration >= 0);
  assert(acq_delay >= 0);
  assert(cl_access >= 0);

  avg_q_stats = (double*) malloc(num_threads * sizeof(double));
  assert(avg_q_stats != NULL);

  if (cl_access > 0)
    {
      protected_data = (shared_data*) calloc(cl_access * num_locks, sizeof(shared_data));
      protected_offsets = (uint32_t*) calloc(num_locks, sizeof(shared_data));
      int j;
      for (j = 0; j < num_locks; j++) {
	protected_offsets[j]=cl_access * j;
      }
    }
#ifdef PRINT_OUTPUT
  printf("Number of locks        : %d\n", num_locks);
  printf("Duration               : %d\n", duration);
  printf("Number of threads      : %d\n", num_threads);
  printf("Lock is held for       : %d\n", acq_duration);
  printf("Delay between locks    : %d\n", acq_delay);
  printf("Cache lines accessed   : %d\n", cl_access);
  printf("Do writes              : %d\n", do_writes);
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

  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);

  local_th_data = (local_data *)malloc(num_threads*sizeof(local_data));

  stop = 0;
  /* Init locks */
  the_locks = init_lock_array_global(num_locks, num_threads);

  /* Access set from all threads */
  barrier_init(&barrier, num_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < num_threads; i++) 
    {
      data[i].id = i;
      data[i].num_acquires = 0;
      data[i].seed = rand();
      data[i].barrier = &barrier;
      if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) 
	{
	  fprintf(stderr, "Error creating thread\n");
	  exit(1);
	}
    }
  pthread_attr_destroy(&attr);

  /* Catch some signals */
  if (signal(SIGHUP, catcher) == SIG_ERR ||
      signal(SIGINT, catcher) == SIG_ERR ||
      signal(SIGTERM, catcher) == SIG_ERR) 
    {
      perror("signal");
      exit(1);
    }

  /* Start threads */
  barrier_cross(&barrier);
  gettimeofday(&start, NULL);
  if (duration > 0) 
    {
      nanosleep(&timeout, NULL);
    } else 
    {
      sigemptyset(&block_set);
      sigsuspend(&block_set);
    }
  stop = 1;
  gettimeofday(&end, NULL);
  /* Wait for thread completion */
  for (i = 0; i < num_threads; i++) 
    {
      if (pthread_join(threads[i], NULL) != 0) 
	{
	  fprintf(stderr, "Error waiting for thread completion\n");
	  exit(1);
	}
    }

  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);

  double avg_q = 0;
  unsigned long acquires = 0;
  for (i = 0; i < num_threads; i++) 
    {
      avg_q += avg_q_stats[i];
      acquires += data[i].num_acquires;
    }

  avg_q /= num_threads;

  printf("Total avg queuing: %10.4f which means (on avg):\n", avg_q);
  if (avg_q <= 1)
    {
      printf("  %4.2f%% of the requests faced queuing\n", 100*avg_q);
    }
  else
    {
      printf("  ALL faced queuing\n");
    }
#ifdef PRINT_OUTPUT
  printf("Duration      : %d (ms)\n", duration);
#endif
  printf("#acquires     : %lu ( %lu / s)\n", acquires, (unsigned long )(acquires * 1000.0 / duration));
    
  /* Cleanup locks */
  free_lock_array_global(the_locks, num_locks);

  free(threads);
  free(data);

  return 0;
}
