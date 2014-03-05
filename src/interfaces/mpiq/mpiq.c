#include <mpi.h>

int MPIQ_Init(int * argc, char *** argv)
{
    int rc = 0;

    rc = MPI_Init(argc, argv);

    return rc;
}
