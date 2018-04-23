// ************************************************************************
//
//          miniGhost: stencil computations with boundary exchange.
//                 Copyright (2013) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Richard F. Barrett (rfbarre@sandia.gov) or
//                    Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************

#include "mg_tp.h"

int MG_Sum_grid ( InputParams params, StateVar **g, int which, int final_check_flag )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      gd,                  // Shorthand for params.ghostdepth
      ierr = 0,            // Return status
      i, j, k,             // Counters
      ithread, ivar;       // Counters

   MG_REAL
      *grid_vals;

   MG_REAL
      *in,
      lsum,
      *out;                // For combined reduction.

   double
      time_start;

   // ---------------------
   // Executable Statements
   // ---------------------

   // Accumulate local flux, which has been accumulating on a thread by thread basis
   // with calls to MG_Flux_accumulate from MG_Boundary_conditions with calls to MG_Stencil.
   // This may need to be re-located if more BCs are configured. But since it accumulates
   // thread data, it must occur outside of threaded regions.

   MG_Time_start(time_start);

   for ( ivar=0; ivar<params.numvars; ivar++ ) {
      if ( !final_check_flag ) {
         // Include flux.
         for ( ithread=0; ithread<mgpp.num_threads; ithread++ ) {
            g[ivar]->lflux += g[ivar]->thr_flux[ithread];
         }
      } // else final check, so flux not re-included.
   }
   MG_Time_accum(time_start,timings.bc_accum);

   MG_Time_start(time_start);

   // Compute local sum across grid for each variable.

   if ( which%params.check_answer_freq == 0 || final_check_flag == 1 ) {

      gd = params.ghostdepth;

      for ( ivar=0; ivar<params.numvars; ivar++ ) {

         //if ( g[ivar]->check_answer == 1 || final_check_flag == 1 ) { FIXME rfbarre: why this extra conditional? If we're here we should want the check.
            if ( which%2 ) {
               grid_vals = g[ivar]->values2;
            }
            else {
               grid_vals = g[ivar]->values1;
            }
            // Local grid summed.
            lsum = 0.0;
#if defined _MG_OPENMP
#pragma omp parallel for reduction (+:lsum)
#endif
            for ( k=gd; k<=params.nz + gd-1; k++ ) {
               for ( j=gd; j<=params.ny + gd-1; j++ ) {
                  for ( i=gd; i<=params.nx + gd-1; i++ ) {
                     lsum += grid_vals(i,j,k);
                  }
               }
            }
            g[ivar]->lsum = lsum;
         //}
      }
#if defined _MG_MPI

      // Compute global sum and flux across grid for each variable.

      in = (MG_REAL*)calloc ( 2*params.numvars, sizeof ( MG_REAL ) );
      MG_Assert ( in != NULL, "MG_Sum_grid:calloc(in)" );
 
      out = (MG_REAL*)calloc ( 2*params.numvars, sizeof ( MG_REAL ) );
      MG_Assert ( out != NULL, "MG_Sum_grid:calloc(out)" );

      for ( ivar=0; ivar<params.numvars; ivar++ ) {
         in[2*ivar]   = g[ivar]->lsum;
         in[2*ivar+1] = g[ivar]->lflux;
      }
      if ( !final_check_flag ) {
         MG_Time_start(time_start);
      }
      ierr = CALL_MPI_Allreduce ( in, out, 2*params.numvars, MG_COMM_REAL, MPI_SUM, MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Sum_grid: CALL_MPI_Allreduce(MPI_SUM)" );

      if ( !final_check_flag ) {
         MG_Time_accum(time_start,timings.sum_grid_comm);
      }

      for ( ivar=0; ivar<params.numvars; ivar++ ) {
         g[ivar]->gsum  = out[2*ivar];
         g[ivar]->gflux = out[2*ivar+1];
      }
      free ( in );
      free ( out );
#else
      for ( ivar=0; ivar<params.numvars; ivar++ ) {
         g[ivar]->gsum  = g[ivar]->lsum;
         g[ivar]->gflux = g[ivar]->lflux;
      }
#endif

      // Check the answer: global sum + global flux should equal the initial values for each variable.

      ierr = 0;
      for ( ivar=0; ivar<params.numvars; ivar++ ) {
         params.iter_error[ivar] = abs ( g[ivar]->source_total - ( g[ivar]->gsum + g[ivar]->gflux ) ) / g[ivar]->source_total;
#if defined _MG_DEBUG
         fprintf ( stdout, "  ** Error for variable %d is %8.8e. Tolerance is %e. Sum should be %8.8e\n\n",
                         ivar+1, params.iter_error[ivar], params.error_tol, g[ivar]->source_total );
#endif
         if ( params.iter_error[ivar] > params.error_tol ) {
            if ( mgpp.mype == mgpp.rootpe ) {
               fprintf ( stderr, "  ** Error for variable %d is %8.8e, exceeding tolerance of %e. \n\n",
                         ivar+1, params.iter_error[ivar], params.error_tol );
               ierr++;
            }
         } 
      }
   }
   MG_Time_accum(time_start,timings.sum_grid);

   return ( ierr );

} // Check answer

// =======================================================================================
