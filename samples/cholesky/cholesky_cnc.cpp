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
#include <cassert>
#include <cstdio>
#include <cmath>

#include "cholesky_types.h"
#include "cholesky.h"

#include <qthread/qtimer.h>

// Compute indices (which are tag values) for tiled Cholesky factorization algorithm.
int k_compute::execute(const int & t, cholesky_context & c ) const
{
    int p = c.p;
    
    for(int k = 0; k < p; k++) {
        c.control_S1.put(k);
    }
    return CnC::CNC_Success;
}

aligned_t** k_compute::get_dependences(const int & t, cholesky_context & c, int & no ) const
{
    return NULL;
}

int kj_compute::execute(const int & t, cholesky_context & c ) const
{    
    int p = c.p;
    const int k = t;
 
    for(int j = k+1; j < p; j++) {  
        c.control_S2.put(pair(k,j));
    }
    return CnC::CNC_Success;
}

aligned_t** kj_compute::get_dependences(const int & t, cholesky_context & c, int & no ) const
{
    return NULL;
}

int kji_compute::execute(const pair & t, cholesky_context & c ) const
{
    const int k = t.first;
    const int j = t.second;

    for(int i = k+1; i <= j; i++) {
        c.control_S3.put(triple(k,j,i));
    }
    return CnC::CNC_Success;
}

aligned_t** kji_compute::get_dependences(const pair & t, cholesky_context & c, int & no ) const
{
    return NULL;
}

// Perform unblocked Cholesky factorization on the input block (item indexed by tag values).
// Output is a lower triangular matrix.
int S1_compute::execute(const int & t, cholesky_context & c ) const
{
# ifdef DEBUG_COUNTERS
    ((int*)pthread_getspecific(c.key))[0]++;
# endif
    tile_const_ptr_type A_block;
    tile_ptr_type       L_block;

    int b = c.b;
    const int k = t;
    c.Lkji.get(triple(k,k,k), A_block); // Get the input tile.
	
    // Allocate memory for the output tile.
    L_block = make_shared_tile(b); //std::make_shared< tile_type >( b );
    // FIXME this need to be a triangular tile only
    // for(int i = 0; i < b; i++) {
    //     L_block[i] = (double *) malloc((i+1) * sizeof(double));
    //     if(L_block[i] == NULL) {
    //         fprintf(stderr, "Malloc failed -> %d\n", i);
    //         exit(0);
    //     }
    // }

    for(int k_b = 0; k_b < b; k_b++) {
        if((*A_block)( k_b, k_b ) <= 0) {
            fprintf(stderr, "Not a symmetric positive definite (SPD) matrix(A[%d][%d]=%lf)\n", k_b, k_b, (*A_block)( k_b, k_b ));
            exit(0);
        } 
        (*L_block)( k_b, k_b ) = sqrt((*A_block)( k_b, k_b ));
        for(int j_b = k_b+1; j_b < b; j_b++) {
            (*L_block)( j_b, k_b ) = (*A_block)( j_b, k_b )/(*L_block)( k_b, k_b );
        }
        for(int j_bb = k_b+1; j_bb < b; j_bb++) {
            for(int i_b = j_bb; i_b < b; i_b++) {
                const_cast< tile_type & >(*A_block)( i_b, j_bb ) = (*A_block)( i_b, j_bb ) - ((*L_block)( i_b, k_b ) * (*L_block)( j_bb, k_b ));
            }
        }
    }

    c.Lkji.put(triple(k+1, k, k), L_block, c.p-k);   // Write the output tile at the next time step.
	
    return CnC::CNC_Success;
}

aligned_t** S1_compute::get_dependences(const int & t, cholesky_context & c, int & no ) const
{
    no = 1;
    aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
    c.Lkji.wait_on(triple(t,t,t), &read[0]);
    return read;
}

// Perform triangular system solve on the input tile. 
// Input to this step are the input tile and the output tile of the previous step.
int S2_compute::execute(const pair & t, cholesky_context & c ) const
{
# ifdef DEBUG_COUNTERS
    ((int*)pthread_getspecific(c.key))[1]++;
# endif
    tile_const_ptr_type A_block;
    tile_const_ptr_type Li_block;
    tile_ptr_type       Lo_block;
    
    int b = c.b;
    const int k = t.first;
    const int j = t.second;

    assert( j != k );
    c.Lkji.get(triple(k,j,k), A_block); // Get the input tile.
    c.Lkji.get(triple(k+1, k, k), Li_block);    // Get the 2nd input tile (Output of previous step).
	
    // Allocate memory for the output tile.
    Lo_block = make_shared_tile(b); //std::make_shared< tile_type >( b );
    
    for(int k_b = 0; k_b < b; k_b++) {
        for(int i_b = 0; i_b < b; i_b++) {
            (*Lo_block)( i_b, k_b ) = (*A_block)( i_b, k_b )/(*Li_block)( k_b, k_b );
        }
        for( int j_b = k_b+1; j_b < b; j_b++) {
            for( int i_b = 0; i_b < b; i_b++) {
                const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) - ((*Li_block)( j_b, k_b ) * (*Lo_block)( i_b, k_b ));
            }
        }   
    }
    
    c.Lkji.put(triple(k+1, j, k),Lo_block, c.p-k); // Write the output tile at the next time step.
	
    return CnC::CNC_Success;
}

aligned_t** S2_compute::get_dependences(const pair & t, cholesky_context & c, int & no ) const
{
    const int k = t.first;
    const int j = t.second;
    no = 2;
    aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
    c.Lkji.wait_on(triple(k,j,k), &read[0]);
    c.Lkji.wait_on(triple(k+1,k,k), &read[1]);
    return read;
}


// Performs symmetric rank-k update of the submatrix.
// Input to this step is the given submatrix and the output of the previous step.
int S3_compute::execute(const triple & t, cholesky_context & c ) const
{
# ifdef DEBUG_COUNTERS
    ((int*)pthread_getspecific(c.key))[2]++;
# endif
    tile_const_ptr_type A_block;
    tile_const_ptr_type L1_block;
    tile_const_ptr_type L2_block;
    
    chosen_type temp;
    
    int b = c.b;
    const int k = t[0];
    const int j = t[1];
    const int i = t[2];

    assert( j != k && i != k );
    c.Lkji.get(triple(k, j, i), A_block); // Get the input tile.

# ifndef USE_CHEATING 
    tile_const_ptr_type Lo_block = make_shared_tile(b);
# endif

    if(i==j){   // Diagonal tile.
	    c.Lkji.get(triple(k+1,j,k), L2_block); // In case of a diagonal tile, i=j, hence both the tiles are the same.
	}
    else{   // Non-diagonal tile.
        c.Lkji.get(triple(k+1,i,k), L2_block); // Get the first tile.
        c.Lkji.get(triple(k+1,j,k), L1_block); // Get the second tile.
    }
    
    for(int j_b = 0; j_b < b; j_b++) {
        for(int k_b = 0; k_b < b; k_b++) {
            temp = -1 * (*L2_block)( j_b, k_b );
            if(i!=j){
                for(int i_b = 0; i_b < b; i_b++) {
# ifndef USE_CHEATING 
                    const_cast< tile_type & >(*Lo_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L1_block)( i_b, k_b ));
# else
                    const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L1_block)( i_b, k_b ));
# endif
                }
            }
            else {
                for(int i_b = j_b; i_b < b; i_b++) {
# ifndef USE_CHEATING 
                    const_cast< tile_type & >(*Lo_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L2_block)( i_b, k_b ));
# else
                    const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L2_block)( i_b, k_b ));
# endif
                }
            }
        }
    }
# ifndef USE_CHEATING
    c.Lkji.put(triple(k+1,j,i),Lo_block, 1+((k==j)?1:0));  // Write the output at the next time step.
# else //Cannot have get counts in this case, last argument will be ignored
    c.Lkji.put(triple(k+1,j,i),A_block, 1+((k==j)?1:0));  // Write the output at the next time step.
# endif
    return CnC::CNC_Success;
}

aligned_t** S3_compute::get_dependences(const triple & t, cholesky_context & c, int & no ) const
{
    const int k = t[0];
    const int j = t[1];
    const int i = t[2];
    no = 3;
    aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
    c.Lkji.wait_on(triple(k,j,i), &read[0]);
    c.Lkji.wait_on(triple(k+1,i,k), &read[1]);
    c.Lkji.wait_on(triple(k+1,j,k), &read[2]);
    return read;
}

void cholesky( chosen_type * A, const int n, const int b, const char * oname )
{
# ifndef DISABLE_GET_COUNTS
    # ifdef USE_CHEATING
	# ifdef __INTEL_COMPILER
            assert (0 && "Cannot run cholesky with getcounts and cheating when using icc!");
        # endif
    # endif 
# else
    # ifndef USE_CHEATING 
        printf("Warning! With no get_counts and no cheating this application can waste a lot of memory (depending on the input size)!\n");
    #endif
# endif
    int p;
# ifdef DISABLE_GET_COUNTS 
    int k;
    FILE *fout;
#endif
    p = n/b;
    
    qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);
    
    // Create an instance of the context class which defines the graph
    cholesky_context c( b, p, n);
    printf("Ids list is:\n");
    printf("s1_compute=%d\n", c.sc_s1_compute.id);
    printf("s2_compute=%d\n", c.sc_s2_compute.id);
    printf("s3_compute=%d\n", c.sc_s3_compute.id);
    printf("k_compute=%d\n", c.sc_k_compute.id);
    printf("kj_compute=%d\n", c.sc_kj_compute.id);
    printf("kji_compute=%d\n", c.sc_kji_compute.id);
    
    printf("Lkji_compute=%d\n", c.Lkji.id);
    
	printf("sontrol_S1=%d\n", c.control_S1.id);
	printf("sontrol_S2=%d\n", c.control_S2.id);
	printf("sontrol_S3=%d\n", c.control_S3.id);
    
    c.singleton.put(1);
    for(int i = 0; i < p; i++) {
        for(int j = 0; j <= i; j++) {
            // Allocate memory for the tiles.
            tile_ptr_type temp = make_shared_tile(b); //std::make_shared< tile_type >( b );
            // Split the matrix into tiles and write it into the item space at time 0.
            // The tiles are indexed by tile indices (which are tag values).
            for(int A_i = i*b,T_i = 0; T_i < b; A_i++,T_i++) {
                for(int A_j = j*b,T_j = 0; T_j < b; A_j++,T_j++) {
                    (*temp)( T_i, T_j ) = A[A_i*n+A_j];
                }
            }
            c.Lkji.put(triple(0,i,j),temp, 1);
        }
    }
    
    // Wait for all steps to finish
    printf("Prerun was %d(addr %p)\n", (*c.prerun), c.prerun);
    
    
    //c.singleton.flush();
    //c.control_S1.flush();
    //c.control_S2.flush();
    //c.control_S3.flush();

    printf("Ending prerun\n");
    c.wait();  
    
    qtimer_stop(timer);
    total_time = qtimer_secs(timer);
    
    printf("Time(s): %.3f\n", total_time);
    qtimer_destroy(timer);

    //qt_dictionary_printbuckets(c.Lkji.m_itemCollection);
    //printf("The time taken for parallel execution a matrix of size %d x %d : %g sec\n", n, n, (t3-t2).seconds());
 
# ifdef DISABLE_GET_COUNTS //Output is in accord with reference file only when not using getcounts
    if(oname) {
        fout = fopen(oname, "w");
        if (fout !=NULL)
        {
			for (int i = 0; i < p; i++) {
				for(int i_b = 0; i_b < b; i_b++) {
					k = 1;
					for (int j = 0; j <= i; j++) {
						tile_const_ptr_type _tmp;
						
						c.Lkji.get(triple(k, i, j), _tmp);
						if(i != j) {
							for(int j_b = 0; j_b < b; j_b++) {
								fprintf(fout, "%lf ", (*_tmp)( i_b, j_b )); 
							}
						}
						else {
							for(int j_b = 0; j_b <= i_b; j_b++) {
								fprintf(fout, "%lf ", (*_tmp)( i_b, j_b )); 
							}
						}
						k++;
					}
				fprintf(fout, "\n" ); 
				}
			}
			fclose(fout);
		}
		else
		{
		    perror("Creating output file");
		    fprintf(stderr, "Could not open file '%s'\n", oname);
		}
    } else 
# endif
    {
        // make sure the result has been computed
        c.Lkji.size();
        for (int i = 0; i < p; i++) {
            for (int j = 0; j <= i; j++) {
                tile_const_ptr_type _tmp;
                c.Lkji.get(triple(j+1, i, j), _tmp);
            }
        }
        //TODO: write in A and output in file using these gets (fewer and accuarate wrt to getcounts)
    }
	printf("Tile created: %d, tiles destroyed: %d at end of program\n", Tile<chosen_type>::created, Tile<chosen_type>::destroyed);
}
