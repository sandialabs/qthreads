#ifndef MPIQ_H
#define MPIQ_H

#include <mpi.h>

/*
 * MPIQ Custom Interface
 */

enum policy_flags_e {
    ID_POLICY_THREAD_SINGLE = 0,
    ID_POLICY_THREAD_FUNNELED,
    ID_POLICY_THREAD_SERIALIZED,
    ID_POLICY_THREAD_MULTIPLE,
    ID_POLICY_COUNT
};

#define  FLAG_POLICY_THREAD_SINGLE     (1 << ID_POLICY_THREAD_SINGLE)
#define  FLAG_POLICY_THREAD_FUNNELED   (1 << ID_POLICY_THREAD_FUNNELED)
#define  FLAG_POLICY_THREAD_SERIALIZED (1 << ID_POLICY_THREAD_SERIALIZED)
#define  FLAG_POLICY_THREAD_MULTIPLE   (1 << ID_POLICY_THREAD_MULTIPLE)

int mpiq_policy(uint64_t const policy_flags);

/*
 * MPI Wrappers
 */

int MPIQ_Init(int * argc, char *** argv);
int MPIQ_Init_thread(int * argc, char *** argv, int required, int * provided);
int MPIQ_Finalize(void);
int MPIQ_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm);
int MPIQ_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler);
int MPIQ_Comm_rank(MPI_Comm comm, int *rank);
int MPIQ_Comm_size(MPI_Comm comm, int *size);
int MPIQ_Comm_get_attr(MPI_Comm comm, int comm_keyval, void * attribute_val, int * flag);
int MPIQ_Barrier(MPI_Comm comm);
int MPIQ_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
int MPIQ_Abort(MPI_Comm comm, int errorcode);
int MPIQ_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
int MPIQ_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request);
int MPIQ_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
int MPIQ_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);
int MPIQ_Wait(MPI_Request *request, MPI_Status *status);
int MPIQ_Waitany(int count, MPI_Request array_of_requests[], int *indx, MPI_Status *status);
int MPIQ_Waitall(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[]);
int MPIQ_Allreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPIQ_Gather(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);
double MPIQ_Wtime(void);

#endif
