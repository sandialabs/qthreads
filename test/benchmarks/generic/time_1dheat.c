#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <getopt.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#ifdef __MACH__
#include <unistd.h>
#elif __linux__
#include <time.h>
#endif

// Add this in to qthreads branch, also add new api.
static __inline__ unsigned long long rdtscp(unsigned *cpuid)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(*cpuid));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

void
localsleep(int amount)
{
#ifdef __MACH__
  usleep(amount);
#elif __linux__
  struct timespec res;
  res.tv_sec = 0;
  res.tv_nsec = amount;

  clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &res, NULL);
#endif
}

// so what are we doing with this?
// that is interesting.

typedef struct {
  float *t0;
  float *t1;
  int size;
  int rank;
  int tsize;
  int alpha;
  int nwork;
  int start;
  int stop;
  int target;
  aligned_t *resp;
  aligned_t *value;
} sendargs;

static aligned_t
do_comm(void *args)
{
  static int debug;
  sendargs *a = (sendargs*)args;
  unsigned long long finish, start;
  unsigned oldcpuid, cpuid;
  float *t0 = a->t0;
  float *t1 = a->t1;
  int rank = a->rank;
  int tsize = a->tsize;
  int target = a->target;
  aligned_t *resp = a->resp;
  aligned_t *value = a->value;

  start = rdtscp(&oldcpuid);
  // so what is the argument I'm sending?
  if(rank % 2 == 0) {
    if(debug)
      printf("rank %d starting send to %d\n", rank, rank);
    MPI_Send(t0, tsize, MPI_FLOAT, target,  0, MPI_COMM_WORLD);
    if(debug)
      printf("rank %d starting recv from %d\n", rank, rank);
    MPI_Recv(t1, tsize, MPI_FLOAT, target, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  } else {
    if(debug)
      printf("rank %d starting recv from %d\n", rank, rank);
    MPI_Recv(t1, tsize, MPI_FLOAT, target, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if(debug)
      printf("rank %d starting send to %d\n", rank, rank);
    MPI_Send(t0, tsize, MPI_FLOAT, target,  0, MPI_COMM_WORLD);
  }
  finish = rdtscp(&cpuid);
  qthread_writeF(resp, value);
  return 0;
}

// so I need to fix this.
static aligned_t
diffuse(void *args) {
  sendargs *a = (sendargs*)args;
  float *t0 = a->t0;
  float *t1 = a->t1;
  int size = a->size;
  int rank = a->rank;
  //int tsize = a->tsize;
  int nwork = a->nwork;
  int alpha = a->alpha;
  int start = a->start;
  int stop = a->stop;
  int i;
  aligned_t resp, value;
  sendargs netargs = *a;
  a->resp = &resp;
  a->value = &value;

  if(start == 0 && rank != 0) {
    a->target = rank-1;
    qthread_fork_net(do_comm, &netargs, NULL);
    qthread_readFE(&resp, &value);
    // feb to grab value.
  }

  for(i = start; i < stop; i++) {
    // so we're going to need local pointers.
    t1[i] =t0[i]+alpha*t0[i+1]-2*t0[i]+t0[i-1];
  }
  if(stop == nwork && rank != size) {
    a->target = rank+1;
    qthread_fork_net(do_comm, &netargs, NULL);
    qthread_readFE(&resp, &value);
    // feb to grab value
  }
  return 0;
}

int
main(int argc, char **argv)
{
  int ntimes;
  int size;
  int noise;
  int rank;
  int i, j;
  float *t0;
  float alpha;
  int tsize;
  int nwork;
  int stride;
  static int debug;
  // need to make a bunch of problems that we can put together.
  noise = 0;
  ntimes = 10;
  tsize = 4096;
  nwork = 1000;
  alpha = 2.0;
  stride = 10;
  for(;;) {
    int c;
    static struct option long_options[] =
      {
        /* These options set a flag. */
        {"debug", no_argument, &debug, 1},
        /* These options don’t set a flag.
           We distinguish them by their indices. */
        {"noise",  required_argument, 0, 'n'},
        {"tsize",  required_argument, 0, 'z'},
        {"work",  required_argument, 0, 'w'},
        {"times", required_argument, 0, 't'},
        {"stride", required_argument, 0, 's'},
        {0, 0, 0, 0}
      };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "w:n:z:s:",
                     long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
        break;
      printf ("option %s", long_options[option_index].name);
      if (optarg)
        printf (" with arg %s", optarg);
      printf ("\n");
      break;

    case 'w':
      nwork = atoi(optarg);
      break;

    case 'n':
      noise = atoi(optarg);
      break;

    case 'z':
      tsize = atoi(optarg);
      break;

    case 's':
      stride = atoi(optarg);
      break;

    case 't':
      ntimes = atoi(optarg);
      break;

    case '?':
      /* getopt_long already printed an error message. */
      break;

    default:
      abort ();
    }
  }
  if(debug)
    printf("allocating buffer\n");
  MPI_Init(&argc, &argv);
  MPI_Comm_rank( MPI_COMM_WORLD, &rank);
  MPI_Comm_size( MPI_COMM_WORLD, &size);
  qthread_initialize();
  // need the buffer size
  if(debug)
    printf("size %d\n", size);
  // need to set

  // work sharing/dealing.
  // work splitting
  // openmp guided.
  sendargs args;
  for(i = 0; i < ntimes; i++) {
  // do I want to set up a bunch of qthreads to do the spawning?
    t0 = malloc(sizeof(float)*tsize);
    args.t0 = t0;
    args.nwork = nwork;
    args.size = size;
    args.rank = rank;
    args.tsize = tsize;

    for(j = 0; j < nwork; j++) {
      qthread_fork(diffuse, &args, NULL);
    }
  }
  /* network task */

  free(t0);
  MPI_Finalize();
  exit(0);
}
