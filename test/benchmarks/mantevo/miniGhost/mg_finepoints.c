#include <qthread/qthread.h>
#include <qthread/barrier.h>
#include <qthread/qloop.h>
#include <qt_atomics.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpiPart.h"
#include "mg_tp.h"

static qt_barrier_t *wait_on_me;

/* TODO(nevans) make this dynamic, add this to MPIF init */
struct fp_params {
	int tag;
	int dest;
  int count;
  int comm;
	qt_barrier_t *wait;
  int nthread;
	MPI_Request *sendReq;
	MPI_Request *recvReq;
  int participant;
  MPI_Datatype datatype;
  char *slab;
  QTHREAD_FASTLOCK_TYPE lock;
};

typedef struct fp_params fp_params;

#define MAX_TAB 512
fp_params *tab = NULL;

int ntab = 0;

struct arg_wrapper {
  int dest;
  int tag;
  int count;
  MPI_Datatype datatype;
  void *buf;
  MPI_Comm comm;
  MPI_Request *req;
  InputParams *p;
};

typedef struct arg_wrapper arg_wrapper;
/* so what are we going to give to this? what do I need?
So the partitioned send needs
Actually this is what sets up the data structure (almost).

I can make the hash table too, but do that last. 
 */

static int tabinitted;

int
tabentry(int dest, int tag)
{
  int i;
  if(!tabinitted) {
	fprintf(stderr, "table not initialized yet");
	exit(-1);
  }
  for(i = 0; i < ntab; i++)
		if(tab[i].tag == tag && tab[i].dest == dest) {
			break;
		}
  if(i == ntab && ntab != MAX_TAB) {
    tab[ntab].tag = tag;
    tab[ntab].dest = dest;
  } else {
    fprintf(stderr, "table overflow, exiting\n");
    exit(-1);
  } 
  return i;
}


int
MPIF_Init(int *argc, char ***argv, InputParams *params)
{
  qthread_initialize();
  //tab = malloc(sizeof(fp_params)*params->numblks);
  tab = malloc(sizeof(fp_params)*MAX_TAB);
  if(tab == NULL) {
    fprintf(stderr, "couldn't allocate table of size %ld numblks %ld sizeof(fp_params) %ld\n", sizeof(fp_params)*params->numblks, params->numblks, sizeof(fp_params));
  }
  tabinitted = 1;	
  //memset(tab, 0, sizeof(fp_params)*params->numblks);
  memset(tab, 0, sizeof(fp_params)*MAX_TAB);
  MPI_Init(argc,argv);
}


aligned_t
start_part(void *arg) {
  int rc;

  arg_wrapper *a = (arg_wrapper*)arg;
  fp_params *p;
  p = &tab[tabentry(a->dest, a->tag)];
  p->slab = a->buf;
  // not necessary made with the entry
  // p->dest = a->dest;
  // p->tag = a->tag;
  p->count = a->count;
  p->datatype = a->datatype;
  p->comm = a->comm;
  p->nthread = a->p->numblks;
  p->wait = qt_barrier_create(p->nthread, REGION_BARRIER);
  p->sendReq = a->req;
  /* this needs to be the slab */
  printf("%s: send_create p->slab %p p->count %d p->nthread %d p->dest %d p->tag %d p->datatype %p\n", __func__, p->slab, p->count, p->nthread, p->dest, p->tag, p->datatype);
  rc = MPI_Partitioned_Send_create( p->slab, p->count * p->nthread + 4096, p->datatype, p->nthread, p->dest, p->tag, p->comm, p->sendReq );
  assert( rc == MPI_SUCCESS );
  rc = MPI_Start_part(p->sendReq);
  assert( rc == MPI_SUCCESS );

  return rc;
}

QTHREAD_TRYLOCK_TYPE part_trylock;

int
MPIF_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm, InputParams *params)
{
	int rc;
	MPI_Request sendReq, *sendReqRet;
	arg_wrapper args = { .dest = dest, .tag = tag, .count = count, .datatype = datatype, .buf = buf, .comm = comm, .req = &sendReq, .p = params};
        args.dest = dest;
        args.datatype = datatype;
        printf("%s: dest %d args.dest %d datatype %p args.datatype %p params->numblks %d args.p->numblks %d sendReq %p args.req %p\n", __func__, dest, args.dest, datatype, args.datatype, params->numblks, args.p->numblks, &sendReq, args.req);
  int i;
	int rank;
	int tid;

  /* TODO(nevans) make a hash into the table that puts the tag in the upper bits of the rank. */
  i = tabentry(dest, tag);
  qthread_incr(&(tab[i].participant), 1);
  //qthread_fork_once(start_part, &args, (aligned_t)sendReqRet);
aligned_t once;
  qthread_fork_once(start_part, &args, &once, &part_trylock);
  rc = MPI_Partitioned_Add_to_buffer( &sendReq, buf, count, datatype );
 	assert( rc  == MPI_SUCCESS );
}

aligned_t
make_recv(void *arg)
{
  int rc;

  arg_wrapper *a = (arg_wrapper*)arg;
  fp_params *p = &tab[tabentry(a->tag, a->dest)];

  rc = MPI_Partitioned_Recv_create( a->buf, a->count * p->nthread, a->datatype, a->dest, a->tag,
                                    a->comm, a->req );
    assert( rc == MPI_SUCCESS );

    return rc;
}

aligned_t
finish_recv(void *arg)
{
  int rc;
  arg_wrapper *a = (arg_wrapper*)arg;

  rc = MPI_Wait_part(a->req, MPI_STATUS_IGNORE );
  assert( rc == MPI_SUCCESS );

  return (aligned_t)a->req;
}

/**
 * - Yield or suspend task until a specific request is satisfied, stolen from MPIQ
 */
static inline void wait_on_request(MPI_Request * request, MPI_Status * status) {
  int flag;

  do {
    MPI_Test(request, &flag, status);
    if (0 == flag) {
      qthread_yield();
    }
  } while (0 == flag);
}

QTHREAD_TRYLOCK_TYPE make_trylock;
QTHREAD_TRYLOCK_TYPE finish_trylock;
int
MPIF_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status, InputParams *params)
{
	MPI_Request recvReq, *recvReqRet;
	arg_wrapper args = { source, tag, count, datatype, buf, comm, .req = &recvReq, .p = params};
	int rank;
	int rc;
	printf("%s: source %d args.dest %d params %p args.p %p params.numblks %d args.p->numblks %d\n", __func__, source, args.dest, params, args.p, params->numblks, args.p->numblks);
	MPI_Comm_rank(comm, &rank);
  //qthread_fork_once(make_recv, &args, recvReqRet);
aligned_t once;
  qthread_fork_once(make_recv, &args, &once, &make_trylock);
  printf("%s: forked_once recvReq %p recvReqRet %p\n", __func__, &recvReq, recvReqRet);
  //qthread_fork_once(finish_recv, &args, recvReqRet);

aligned_t once2;
  qthread_fork_once(finish_recv, &args, &once2, &finish_trylock);
  wait_on_request(&recvReq, status);
  return rc;
}

int
MPIF_Barrier( MPI_Comm comm )
{
  int rank;
  int i;
  MPI_Comm_rank(comm, &rank);
  // TODO(nevans) need to make a table indexed on communicator as well dest and tag
  /*
  i = tabentry(dest, tag);
  qt_barrier_enter(tab[i].wait);
  // you want a table by ranks? sendReqs are by thread? No, they're by rank but they're shared by threads. 
  if ( rank == 0) {
    MPI_Partitioned_free( &sendReq );
  }
  */
  // free singleton.
  MPI_Barrier(comm);
}

//#define VERIFY 1

/*
int MPI_Finalize( void )
{
  arg_t arg;
  int other = (rank + 1) % 2;
  double start;
  int TAG = 0xdead;
  int rc;
}
*/
