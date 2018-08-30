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
  MPI_Datatype *datatype;
  char *slab;
  QTHREAD_FASTLOCK_TYPE lock;
};

typedef struct fp_params fp_params;

fp_params *tab;

int ntab = 0;

struct arg_wrapper {
  int dest;
  int tag;
  int count;
  MPI_Datatype *datatype;
  char *buf;
  int comm;
  MPI_Request *sendReq;
  InputParams *p;
};

typedef struct arg_wrapper arg_wrapper;
/* so what are we going to give to this? what do I need?
So the partitioned send needs
Actually this is what sets up the data structure (almost).

I can make the hash table too, but do that last. 
 */


int
tabentry(int dest, int tag)
{
  int i;

  for(i = 0; i < ntab; i++)
		if(tab[i].tag == tag && tab[i].dest == dest) {
			break;
		}
  return i > ntab ? i : -1;
}

aligned_t
start_part(void *arg) {
  int rc;

  arg_wrapper *a = (arg_wrapper*)arg;
  fp_params *p;
  /* XXX(nevans) bad mojo for not testing error condition, but a segfault is fine for now */
  p = &tab[tabentry(a->dest, a->tag)];
  p->dest = a->dest;
  p->tag = a->tag;
  p->count = a->count;
  p->datatype = a->datatype;
  p->comm = a->comm;
  p->nthread = a->p->numblks;
  p->wait = qt_barrier_create(p->nthread, REGION_BARRIER);
  p->sendReq = a->sendReq;
  /* this needs to be the slab */
  rc = MPI_Partitioned_Send_create( p->slab, p->count * p->nthread, p->datatype, p->nthread, p->dest, p->tag, p->comm, p->sendReq );
  assert( rc == MPI_SUCCESS );
  rc = MPI_Start_part(p->sendReq);
  assert( rc == MPI_SUCCESS );
  return (aligned_t)&p->sendReq;
}


int
MPIF_Init(InputParams *params, int *argc, char ***argv)
{
  qthread_initialize();
  tab = malloc(sizeof(fp_params)*params->numblks);
  MPI_Init(argc,argv);
}

int
MPIF_Send(InputParams *params, const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm)
{
	int rc;
 	qt_barrier_t *wait_on_me;
	MPI_Request sendReq, *sendReqRet;
  aligned_t once;
  int i;
	int rank;
	int tid;

  /* TODO(nevans) make a hash into the table that puts the tag in the upper bits of the rank. */
  i = tabentry(dest, tag);
  qthread_incr(&(tab[i].participant), 1);
  qthread_fork_once(start_part, &sendReq, (aligned_t)sendReqRet);

  rc = MPI_Partitioned_Add_to_buffer( sendReqRet, buf, count, datatype );
 	assert( rc  == MPI_SUCCESS );
}

aligned_t
make_recv(void *arg)
{
	MPI_Request recvReq;
  int rc;
  arg_wrapper *a = (arg_wrapper*)arg;
  fp_params *p = &tab[tabentry(a->tag, a->dest)];

  rc = MPI_Partitioned_Recv_create( a->buf, a->count * p->nthread, a->datatype, a->dest, a->tag,
                                    a->comm, &recvReq );
    assert( rc == MPI_SUCCESS );
    return (aligned_t)recvReq;
}

aligned_t
finish_recv(void *arg)
{
  int rc;
	MPI_Request recvReq;

  rc = MPI_Wait_part(&recvReq, MPI_STATUS_IGNORE );
  assert( rc == MPI_SUCCESS );

  return (aligned_t)recvReq;
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


int
MPIF_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status)
{
	MPI_Request recvReq, *recvReqRet;
	int rank;
	int rc;
	int other = (rank + 1) % 2;


	MPI_Comm_rank(comm, &rank);
  qthread_fork_once(make_recv, recvReq, recvReqRet);

  qthread_fork_once(finish_recv, recvReq, recvReqRet);
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
