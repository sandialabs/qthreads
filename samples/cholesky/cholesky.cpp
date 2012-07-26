//********************************************************************************
// Copyright (c) 2007-2012 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include "cholesky_types.h"
#include "cholesky.h"
//#include "cholesky_cnc.cpp"
#include <cassert>

template<> int Tile<chosen_type>::created=0;
template<> int Tile<chosen_type>::destroyed=0;

extern void cholesky( chosen_type *A, const int n, const int b, const char * oname );

void posdef_gen( chosen_type * A, int n )
{
	/* Allocate memory for the matrix and its transpose */
	chosen_type *L;
	chosen_type *LT;
    chosen_type two = 2.0;
	chosen_type one = 1.0;
    //	srand( 1 );

	L = (chosen_type *) calloc(sizeof(chosen_type), n*n);
	assert (L);

	LT = (chosen_type *) calloc(sizeof(chosen_type), n*n);
	assert (LT);

    memset( A, 0, sizeof( chosen_type ) * n * n );
	
	/* Generate a conditioned matrix and fill it with random numbers */
    for(int j = 0; j < n; j++) {
        for(int k = 0; k <= j; k++) {
			if(k<j) {
				// The initial value has to be between [0,1].
				L[k*n+j] = ( ( (j*k) / ((chosen_type)(j+1)) / ((chosen_type)(k+2)) * two) - one ) / ((chosen_type)n);
			} else if (k == j) {
				L[k*n+j] = 1;
            }
		}
	}

	/* Compute transpose of the matrix */
	for(int i = 0; i < n; i++) {
		for(int j = 0; j < n; j++) {
			LT[j*n+i] = L[i*n+j];
		}
	}
		
    //tbb::parallel_for( 0, n, 1, [&]( int i ) {
    for (int i = 0; i < n; i++) {
            chosen_type * _a = &A[i*n];
            chosen_type * _l = &L[i*n];
            for (int k = 0; k < n; k++) {
                chosen_type * _lt = &LT[k*n];
                for (int j = 0; j < n; j++) {
                    _a[j] += _l[k] * _lt[j];
                }
            }
        };

    free (L);
    free (LT);
}

// Read the matrix from the input file
void matrix_init (chosen_type *A, int n, const char *fname)
{
    if( fname ) {
        int i;
        int j;
        FILE *fp;
        
        fp = fopen(fname, "r");
        if(fp == NULL) {
            fprintf(stderr, "\nFile does not exist\n");
            exit(0);
        }
        for (i = 0; i < n; i++) {
            for (j = 0; j <= i; j++) {
//            for (j = 0; j < n; j++) {
                //if( fscanf(fp, "%lf ", &A[i*n+j]) <= 0) {   
# ifdef USE_SINGLE_PRECISION
                if( fscanf(fp, "%f ", &A[i*n+j]) <= 0) {
# else
                if( fscanf(fp, "%lf ", &A[i*n+j]) <= 0) {   
# endif
                   fprintf(stderr,"\nMatrix size incorrect %d (%d %d)\n", n, i, j);
                   exit(0);
                }
                if( i != j ) {
                    A[j*n+i] = A[i*n+j];
                }
            }
        }
        fclose(fp);
    } else {
        posdef_gen( A, n );
    }
}

// write matrix to file
void matrix_write ( chosen_type *A, int n, const char *fname )
{
    if( fname ) {
        int i;
        int j;
        FILE *fp;
        
        fp = fopen(fname, "w");
        if(fp == NULL) {
            fprintf(stderr, "\nCould not open file %s for writing.\n", fname );
            exit(0);
        }
        for (i = 0; i < n; i++) {
            for (j = 0; j <= i; j++) {
                fprintf( fp, "%lf ", A[i*n+j] );
            }
            fprintf( fp, "\n" );
        }
        fclose(fp);
    }
}

int main (int argc, char *argv[])
{
    int n;
    int b;
    const char *fname = NULL;
    const char *oname = NULL;
    const char *mname = NULL;
    int argi;

    // Command line: cholesky n b filename [out-file]
    if (argc < 3 || argc > 7) {
        fprintf(stderr, "Incorrect number of arguments, expected N BS [-i infile] [-o outfile] [-w mfile]\n");
        return -1;
    }
    argi = 1;
    n = atol(argv[argi++]);
    b = atol(argv[argi++]);
    while( argi < argc ) {
        if( ! strcmp( argv[argi], "-o" ) ) oname = argv[++argi];
        else if( ! strcmp( argv[argi], "-i" ) ) fname = argv[++argi];
        else if( ! strcmp( argv[argi], "-w" ) ) mname = argv[++argi];
        ++argi;
    }

    if(n % b != 0) {
        fprintf(stderr, "The tile size is not compatible with the given matrix\n");
        exit(0);
    }

    chosen_type * A = new chosen_type[n*n];

    matrix_init (A, n, fname);
    if( mname ) matrix_write( A, n, mname );
    else cholesky(A, n, b, oname);     

    delete [] A;

    return 0;
}
