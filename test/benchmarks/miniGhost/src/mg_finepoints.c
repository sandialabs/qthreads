#include <qthread/qthread.h>
#include <qthread/barrier.h>
#include <qthread/qloop.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpiPart.h"

static qt_barrier_t *wait_on_me;

struct {
	int tag;
	int dest;
	qt_barrier_t *wait;	
} bartab[512];

int ntab = 0;

int 
MPIF_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm)
{
	int rc;
 	qt_barrier_t *wait_on_me; 
	MPI_Request sendReq;
	for(i = 0; i < ntab; i++)
		if(bartab[i].tag == tag && bartab[i].dest == dest) {
			wait_on_me = bartab[i].wait;
			break;
		}	
 	if(i == ntab) {
		bartab[i].dest = dest;
		bartab[i].tag = tag;
		// XXX: how do I know the number of workers?
		// I think we assume that they all do the same thing.
		wait_on_me = bartab[i] = qt_barrier_create(qt_num_workers(), REGION_BARRIER);
		ntab++;
 	} 
 	if (rank == 0) {
    		rc = MPI_Partitioned_Send_create( buf, count * qthread_num_workers(), datatype, qthread_num_workers(), other, tag,
					      comm, &sendReq );
    		assert( rc == MPI_SUCCESS );
    		if (tid == 0)  {
        		rc = MPI_Start_part(&sendReq);
			assert( rc == MPI_SUCCESS );
    		}
 	}
 	rc = MPI_Partitioned_Add_to_buffer( &sendReq, buf, count, datatype );
 	assert( rc  == MPI_SUCCESS );
 	qt_barrier_enter(wait_on_me);
	if ( rank == 0) {
		MPI_Partitioned_free( &sendReq );
	}
}

int 
MPIF_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status)
{
	MPI_Request recvReq;
 	qt_barrier_enter(wait_on_me);
	if ( rank == 0 ) {
    		rc = MPI_Partitioned_Recv_create( buf, count * qthread_num_workers(), datatype, other, tag,
				      comm, &recvReq );
    		assert( rc == MPI_SUCCESS );
	}
 	qt_barrier_enter(wait_on_me);
	if ( rank == 0 ) {
		MPI_Partitioned_free( &recvReq );
	}
}

//#define VERIFY 1

int MPI_Finalize( void )
{
  arg_t arg;
  int other = (rank + 1) % 2;
  double start;
  int TAG = 0xdead;
  int rc;
  

  start = MPI_Wtime();

  srand(time(NULL));

  if ( 0 == rank ) {
    MPI_Partitioned_free( &sendReq );
  } else {
    MPI_Partitioned_free( &recvReq );
  }
  double duration = MPI_Wtime() - start;
  if ( numThreads > 1 ) {
    duration -=  sleepPlus / 1000000000.0 * numIterations;
  } else {
    duration -=  sleep1 / 1000000000.0 * numIterations;
  }
  return duration;
}
