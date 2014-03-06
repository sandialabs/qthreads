#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#include "qthread/qthread.h"

int MPIQ_Init(int * argc, char *** argv)
{
    int rc = 0;

    rc = qthread_initialize();
    if (QTHREAD_SUCCESS != rc) {
        fprintf(stderr, "Error: failed to initialize Qthreads (rc=%d)\n", rc);
        abort();
    }

    rc = MPI_Init(argc, argv);
    if (MPI_SUCCESS != rc) {
        fprintf(stderr, "Error: failed to initialize MPI (rc=%d)\n", rc);
        abort();
    }

    return rc;
}
