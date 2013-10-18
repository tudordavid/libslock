/* 
 * File: htlock_test.c
 * Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *
 * Description: tests the hierarchical ticket lock (both normal
 *   lock function and the try_lock)
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
#include "utils.h"
#include "htlock.h"

#define XSTR(s) #s
#define ALIGNMENT


#define DEFAULT_NUM_ENTRIES 1024
#define DEFAULT_NUM_THREADS 1
#define DEFAULT_DURATION 2000
#define DEFAULT_SEED 0
#define DEFAULT_CORE_TRY_LOCK 8

int num_entries;
int num_threads;
int duration;
int num_cores_try_lock;

__thread uint32_t phys_id;
__thread uint32_t cluster_id;
static volatile int stop;

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
    struct {
      barrier_t *barrier;
      unsigned long num_operations;
      unsigned int seed;
      int id;
      htlock_t* locks;
    };
    char padding[CACHE_LINE_SIZE];
  };
} thread_data_t;

uint32_t steps = 0;

uint64_t atomic_counter[8] = {0};

void*
test(void *data)
{
  thread_data_t *d = (thread_data_t *)data;
//#ifdef __sparc__
    phys_id = the_cores[d->id];
    cluster_id=get_cluster(phys_id);
//#else
//    phys_id = d->id;
//#endif

  init_thread_htlocks(phys_id);
  htlock_t* htls = d->locks;

  /* Init of local data if necessary */
  /* Wait on barrier */
  barrier_cross(d->barrier);

  uint8_t success = 1;

  while (stop == 0)
    {

      if (d->id < num_cores_try_lock)
	{
	  success = htlock_trylock(htls);
	}
      else
	{
	  htlock_lock(htls);
	}

      if (success)
	{
	  asm volatile("NOP");
	  atomic_counter[0]++;
	  asm volatile("NOP");
	  if (d->id < num_cores_try_lock)
	    {
	      htlock_release_try(htls);
	    }
	  else
	    {
	      htlock_release(htls);
	    }
	  d->num_operations++;
	}
    }

  if (d->id < num_cores_try_lock)
    {
      printf("[%02d] try_lock completed: %lu\n", d->id, d->num_operations);
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


int
main(int argc, char* const argv[])
{
  set_cpu(the_cores[0]);
  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,       NULL, 'h'},
      {"entries",                   required_argument, NULL, 'e'},
      {"duration",                  required_argument, NULL, 'd'},
      {"num-threads",               required_argument, NULL, 'n'},
      {"try-lock",                  required_argument, NULL, 't'},
      {NULL, 0, NULL, 0}
    };

#ifdef PRINT_OUTPUT
  printf("sizeof(htlock_global_t) = %lu\n", sizeof(htlock_global_t));
  printf("sizeof(htlock_local_t) = %lu\n", sizeof(htlock_local_t));
  printf("sizeof(htlock_t) = %lu\n", sizeof(htlock_t));
#endif
  int i, c;
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
 
  num_entries = DEFAULT_NUM_ENTRIES;
  num_threads = DEFAULT_NUM_THREADS;
  num_cores_try_lock = DEFAULT_CORE_TRY_LOCK;
  duration = DEFAULT_DURATION;
  sigset_t block_set;

  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "he:d:n:t:", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) {
      case 0:
	/* Flag is automatically set */
	break;
      case 'h':
	printf("htlock test\n"
	       "\n"
	       "Usage:\n"
	       "  htlock_test [options...]\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help\n"
	       "        Print this message\n"
	       "  -e, --entries <int>\n"
	       "        Number of entries in the test (default=" XSTR(DEFAULT_NUM_LOCKS) ")\n"
	       "  -d, --duration <int>\n"
	       "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
	       "  -n, --num-threads <int>\n"
	       "        Number of threads (default=" XSTR(DEFAULT_NUM_THREADS) ")\n"
	       "  -t, --try-lock <int>\n"
	       "        Number of cores working with try-locks (" XSTR(DEFAULT_CORE_TRY_LOCK)")\n"
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
      case 't':
	num_cores_try_lock = atoi(optarg);
	break;
      case '?':
	printf("Use -h or --help for help\n");
	exit(0);
      default:
	exit(1);
      }
    }

  assert(duration >= 0);
  assert(num_entries >= 1);
  assert(num_threads > 0);

#ifdef PRINT_OUTPUT
  printf("Number of entries   : %d\n", num_entries);
  printf("Duration       : %d\n", duration);
  printf("Number of threads     : %d\n", num_threads);
  printf("Number of trylock threads     : %d\n", num_cores_try_lock);
  printf("Type sizes     : int=%d/long=%d/ptr=%d\n",
	 (int)sizeof(int),
	 (int)sizeof(long),
	 (int)sizeof(void *));
#endif
  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

 
  /* the_data = (data_t*) malloc(num_entries * sizeof(data_t)); */
  /* for (i = 0; i < num_entries; i++)  */
  /*   { */
  /*     the_data[i].data = 0; */
  /*   } */

  if ((data = (thread_data_t *) malloc(num_threads * sizeof(thread_data_t))) == NULL) 
    {
      perror("malloc");
      exit(1);
    }

  if ((threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t))) == NULL) 
    {
      perror("malloc");
      exit(1);
    }

  srand((int)time(NULL));

  stop = 0;
  /* Access set from all threads */
  barrier_init(&barrier, num_threads + 1);
  pthread_attr_init(&attr);


  /* initialize the locks */

  htlock_t* htls = init_htlocks(1);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#ifdef PRINT_OUTPUT
  printf("Creating threads: ");
#endif
  for (i = 0; i < num_threads; i++) 
    {
#ifdef PRINT_OUTPUT
      printf("%d, ", i);
#endif
      data[i].id = i;
      data[i].num_operations = 0;
      data[i].seed = rand();
      data[i].barrier = &barrier;
      data[i].locks = htls;
      if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) 
	{
	  fprintf(stderr, "Error creating thread\n");
	  exit(1);
	}
    }
#ifdef PRINT_OUTPUT
  printf("\n");
#endif
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

#ifdef PRINT_OUTPUT
  printf("STARTING...\n");
#endif
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
#ifdef PRINT_OUTPUT
  printf("STOPPING...\n");
#endif

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
    
  unsigned long operations = 0;
  for (i = 0; i < num_threads; i++) 
    {
#ifdef PRINT_OUTPUT
      printf("Thread %d\n", i);
      printf("  #operations   : %lu\n", data[i].num_operations);
#endif
      operations += data[i].num_operations;
    }
  printf("Duration      : %d (ms)\n", duration);
  printf("#operations     : %lu (%f / s)\n", operations, operations * 1000.0 / duration);
  printf("atomic count val: %llu\n", (long long unsigned) atomic_counter[0]);



  /* free(the_data); */
  free(threads);
  free(data);

  return 0;
}
