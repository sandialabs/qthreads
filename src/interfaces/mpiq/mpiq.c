#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#include "qthread/qthread.h"
#include "qthread/qtimer.h"
#include "qt_envariables.h"
#include "qt_asserts.h"

#include "mpiq.h"

static int use_choices           = 0;
static int thread_support_choice = 0;

static int size_;
static int rank_;

/**
 * - Always call `qthread_migrate_to()` so that `QTHREAD_UNSTEALABLE` task attr. is set.
 */
static inline void funnel_task(void)
{
    if (ID_POLICY_THREAD_FUNNELED == thread_support_choice) {
        qassert(qthread_migrate_to(0), QTHREAD_SUCCESS);

        //int rc = qthread_migrate_to(0);
        //if (QTHREAD_SUCCESS == rc) {
        //    fprintf(stderr, "... migrate: success; on shep: %lu\n", qthread_readstate(CURRENT_SHEPHERD));
        //} else if (QTHREAD_NOT_ALLOWED) {
        //    fprintf(stderr, "... migrate: not allowed; still on shep: %lu\n", qthread_readstate(CURRENT_SHEPHERD)); 
        //} else {
        //    fprintf(stderr, "... migrated: failed; on shep: %lu\n", qthread_readstate(CURRENT_SHEPHERD));
        //}
    }
}

/**
 * - Yield or suspend task until a specific request is satisfied
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

/**
 * - Yield or suspend task until one from a list of requests is satisfied
 */
static inline void wait_on_any_request(int count, MPI_Request array_of_requests[], int * index, MPI_Status * status) {
    int flag;

    do {
        MPI_Testany(count, array_of_requests, index, &flag, status);
        if (0 == flag) {
            qthread_yield();
        }
    } while (0 == flag);
}

/**
 * - Yield or suspend task until all from a list of requests is satisfied
 */
static inline void wait_on_all_requests(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[]) {
    int flag;

    do {
        MPI_Testall(count, array_of_requests, &flag, array_of_statuses);
        if (0 == flag) {
            qthread_yield();
        }
    } while (0 == flag);
}

int mpiq_policy(uint64_t const policy_flags)
{
    int rc = 0;

    use_choices = 1;

    /* Set MPI thread support policy */
    if (FLAG_POLICY_THREAD_SINGLE & policy_flags) {
        thread_support_choice = ID_POLICY_THREAD_SINGLE;
        fprintf(stderr, "Info: using THREAD_SINGLE policy\n");
    } else if (FLAG_POLICY_THREAD_FUNNELED & policy_flags) {
        thread_support_choice = ID_POLICY_THREAD_FUNNELED;
        fprintf(stderr, "Info: using THREAD_FUNNELED policy\n");
    } else if (FLAG_POLICY_THREAD_SERIALIZED & policy_flags) {
        thread_support_choice = ID_POLICY_THREAD_SERIALIZED;
        fprintf(stderr, "Info: using THREAD_SERIALIZED policy\n");
    } else if (FLAG_POLICY_THREAD_MULTIPLE & policy_flags) {
        thread_support_choice = ID_POLICY_THREAD_MULTIPLE;
        fprintf(stderr, "Info: using THREAD_MULTIPLE policy\n");
    }

    return rc;
}

int MPIQ_Init(int * argc, char *** argv)
{
    int rc = 0;
    int required = MPI_THREAD_SINGLE;
    int provided;

    rc = MPIQ_Init_thread(argc, argv, required, &provided);

    return rc;
}

int MPIQ_Init_thread(int * argc, char *** argv, int required, int * provided)
{
    int rc = 0;

    /* Choose thread support level; precedence:
     * 1. environment variable
     * 2. explicit policy choice
     * 3. `required` argument
     */
    const char * required_str = qt_internal_get_env_str("MPIQ_THREAD_SUPPORT", NULL);
    if (NULL != required_str) {
        /* Use setting in env. */
        fprintf(stderr, "Info: using env. var. MPIQ_THREAD_SUPPORT=%s\n", required_str);
        if (0 == strcmp(required_str, "MPI_THREAD_SINGLE")) {
            required = MPI_THREAD_SINGLE;
        } else if (0 == strcmp(required_str, "MPI_THREAD_FUNNELED")) {
            required = MPI_THREAD_FUNNELED;
        } else if (0 == strcmp(required_str, "MPI_THREAD_SERIALIZED")) {
            required = MPI_THREAD_SERIALIZED;
        } else if (0 == strcmp(required_str, "MPI_THREAD_MULTIPLE")) {
            required = MPI_THREAD_MULTIPLE;
        } else {
            fprintf(stderr, "Error: unsupported thread level (MPIQ_THREAD_SUPORT=%s)\n", required_str);
            abort();
        }
    } else if (use_choices) {
        /* Use policy choice if nothing specified in env. */
        switch(thread_support_choice) {
        case ID_POLICY_THREAD_SINGLE:
            required = MPI_THREAD_SINGLE;
            break;
        case ID_POLICY_THREAD_FUNNELED:
            required = MPI_THREAD_FUNNELED;
            break;
        case ID_POLICY_THREAD_SERIALIZED:
            required = MPI_THREAD_SERIALIZED;
            break;
        case ID_POLICY_THREAD_MULTIPLE:
            required = MPI_THREAD_MULTIPLE;
            break;
        default:
            fprintf(stderr, "Error: unknown thread policy choice (thraed_support_choice=%d)\n", thread_support_choice);
            abort();
        }
        required = thread_support_choice;
    }

    /* Initialize MPI with required thread support level */
    rc = MPI_Init_thread(argc, argv, required, provided);
    if (MPI_SUCCESS != rc) {
        fprintf(stderr, "Error: failed to initialize MPI (rc=%d)\n", rc);
        abort();
    }

    if (*provided < required) {
        fprintf(stderr, "Warning: thread support below required level (%d < %d)\n", *provided, required);
    }

    /* Check level of the request */
    const char * hint_str = qt_internal_get_env_str("MPIQ_THREAD_REQUEST_LEVEL", NULL);
    if (NULL == hint_str) {
        /* Assume request is a hint */
    } else if (0 == strcmp(hint_str, "HINT")) {
        fprintf(stderr, "Info: using thread request level %s\n", hint_str);
    } else if (0 == strcmp(hint_str, "REQUIREMENT")) {
        /* Ignore provided level and just act like it provided the required level */
        fprintf(stderr, "Info: using thread request level %s\n", hint_str);
        *provided = required;
    } else {
        fprintf(stderr, "Warning: unsupported thread request level (MPIQ_THREAD_REQUEST_LEVEL=%s)\n", hint_str);
    }

    switch (*provided) {
    case MPI_THREAD_SINGLE:
        fprintf(stderr, "Info: using MPI_THREAD_SINGLE\n");
        thread_support_choice = ID_POLICY_THREAD_SINGLE;
        // TODO: set env. vars. and use qthread_initialize()
        qthread_init(1);
        if (1 != qthread_readstate(TOTAL_WORKERS)) {
            fprintf(stderr, "Error: Using more than one worker with MPI_THREAD_SINGLE.\n");
            abort();
        }
        break;
    case MPI_THREAD_FUNNELED:
        fprintf(stderr, "Info: using MPI_THREAD_FUNNELED\n");
        thread_support_choice = ID_POLICY_THREAD_FUNNELED;
        /* Force use of one worker per shepherd */
        setenv("QT_NUM_WORKERS_PER_SHEPHERD", "1", 1);
        qthread_initialize();
        if (qthread_readstate(TOTAL_SHEPHERDS) != qthread_readstate(TOTAL_WORKERS)) {
            fprintf(stderr, "Error: Using more than one worker per shepherd with MPI_THREAD_FUNNELED.\n");
            abort();
        }
        break;
    case MPI_THREAD_SERIALIZED:
        fprintf(stderr, "Info: using MPI_THREAD_SERIALIZED\n");
        thread_support_choice = ID_POLICY_THREAD_SERIALIZED;
        break;
        // TODO: initialize with k comm. resources.
        abort();
    case MPI_THREAD_MULTIPLE:
        fprintf(stderr, "Info: using MPI_THREAD_MULTIPLE\n");
        thread_support_choice = ID_POLICY_THREAD_MULTIPLE;
        // TODO: initialize with k comm. resources.
        abort();
        break;
    default:
        fprintf(stderr, "Error: unknown MPI thread level (provided=%d)\n", *provided);
        abort();
    }

    MPI_Comm_size(MPI_COMM_WORLD, &size_);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);

    return rc;
}

int MPIQ_Finalize(void)
{
    int rc;

    funnel_task();
    rc = MPI_Finalize();

    return rc;
}

int MPIQ_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int rc;

    funnel_task();
    rc = MPI_Comm_dup(comm, newcomm);

    return rc;
}

int MPIQ_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler)
{
    int rc;

    funnel_task();
    rc = MPI_Errhandler_set(comm, errhandler);

    return rc;
}

int MPIQ_Comm_rank(MPI_Comm comm, int *rank)
{
    int rc = 0;

    *rank = rank_;

    return rc;
}

int MPIQ_Comm_size(MPI_Comm comm, int *size)
{
    int rc = 0;

    *size = size_;

    return rc;
}

int MPIQ_Barrier(MPI_Comm comm)
{
    int rc;

    funnel_task();
    rc = MPI_Barrier(comm);

    return rc;
}

int MPIQ_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
    int rc;

    funnel_task();
    rc = MPI_Bcast(buffer, count, datatype, root, comm);

    return rc;
}

int MPIQ_Abort(MPI_Comm comm, int errorcode)
{
    int rc;

    funnel_task();
    rc = MPI_Abort(comm, errorcode);

    return rc;
}

/**
 * - Request object managed by caller
 */
int MPIQ_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
{
    int rc;

    funnel_task();
    rc = MPI_Isend(buf, count, datatype, dest, tag, comm, request);

    return rc;
}

int MPIQ_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request)
{
    int rc;

    funnel_task();
    rc = MPI_Irecv(buf, count, datatype, source, tag, comm, request);

    return rc;
}

int MPIQ_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
    int rc;

    MPI_Request request;
    MPI_Status  status;

    funnel_task();

    MPIQ_Isend(buf, count, datatype, dest, tag, comm, &request);
    wait_on_request(&request, &status);

    rc = 0; // TODO: set return code

    return rc;
}

int MPIQ_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
    int rc;

    MPI_Request request;
    MPI_Status  local_status;

    funnel_task();

    MPIQ_Irecv(buf, count, datatype, source, tag, comm, &request);
    wait_on_request(&request, &local_status);

    rc = 0; // TODO: set return code

    return rc;
}

int MPIQ_Wait(MPI_Request *request, MPI_Status *status)
{
    int rc;

    funnel_task();

    wait_on_request(request, status);
    rc = 0; // TODO: set return code

    return rc;
}

int MPIQ_Waitany(int count, MPI_Request array_of_requests[], int *index, MPI_Status *status)
{
    int rc;

    funnel_task();

    wait_on_any_request(count, array_of_requests, index, status);
    rc = 0; // TODO: set return code
    
    return rc;
}

int MPIQ_Waitall(int count, MPI_Request array_of_requests[], MPI_Status array_of_statuses[])
{
    int rc;

    funnel_task();
    
    wait_on_all_requests(count, array_of_requests, array_of_statuses);
    rc = 0; // TODO: set return code

    return rc;
}

int MPIQ_Allreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    int rc;

    funnel_task();
    rc = MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);

    return rc;
}

int MPIQ_Gather(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    int rc;

    funnel_task();
    rc = MPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);

    return rc;
}

double MPIQ_Wtime(void)
{
    return qtimer_wtime();
}
