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

int MG_Grid_init ( InputParams *params, StateVar **g )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      count,
      i,                     // Counter
      j,                     // Counter
      ierr = 0,              // Return status
      ithread,               // Thread counter.
      ivar,                  // Counter
      num,
      nvars_with_extra_work, // 
      op,
      opslist[3],
      wvar;                  // Variable index.

   float
      extra_work_percent_real;

   // ---------------------
   // Executable Statements
   // ---------------------

   // Allocate state variables.

   for ( ivar=0; ivar<params->numvars; ivar++ ) {
      // Include ghost space.
      g[ivar] = (StateVar*)MG_CALLOC ( 1, sizeof(StateVar) );
      if ( g[ivar] == NULL ) {
         fprintf ( stderr, "Allocation of g[%d] of size %d failed \n",
                   ivar, (int)sizeof(StateVar) );
         ierr = -1;
         MG_Assert ( !ierr, "main: Allocation of arrays g[i]" );
      }
      g[ivar]->do_more_work_nvars = 0;
   }

   // Determine which variables, if any, will include extra work.

   nvars_with_extra_work = 0;

   switch ( params->extra_work_percent ) 
   {
      case 0:

         break;

      case 100: 

         nvars_with_extra_work = params->numvars;
         for ( ivar=0; ivar<params->numvars; ivar++ ) {
            g[ivar]->do_more_work_nvars = params->extra_work_nvars;
            g[ivar]->do_more_work_vars = (int*)MG_CALLOC ( params->extra_work_nvars, sizeof(int) );
            MG_Assert ( g[ivar]->do_more_work_vars != NULL,
                        "MG_Grid_init: MG_CALLOC(g[ivar]->do_more_work_vars) failed." );

            for ( i=0; i<params->extra_work_nvars; i++ ) {
               wvar = ( random () % params->numvars );
               g[ivar]->do_more_work_vars[i] = wvar;
            }
         }
         break;

      default:

         extra_work_percent_real = params->extra_work_percent / 100.0;

         nvars_with_extra_work = extra_work_percent_real * params->numvars;
         if ( nvars_with_extra_work == 0 ) {
            nvars_with_extra_work = 1; // There has to be at least one.
            if ( mgpp.mype == 0 ) {
               fprintf ( stdout, "\n\n *** Warning *** Extra work requested results in less than one variable \n" );
               fprintf ( stdout, "                     with extra work. Setting extra work to one variable. *** \n\n" );
            }
         }
         for ( ivar=0; ivar<params->numvars; ivar++ ) {
            num = random () % 100;
            if ( num <= params->extra_work_percent ) {
               nvars_with_extra_work++;
               g[ivar]->do_more_work_vars = (int*)MG_CALLOC ( params->extra_work_nvars, sizeof(int) );
               MG_Assert ( g[ivar]->do_more_work_vars != NULL, 
                           "MG_Grid_init: MG_CALLOC(g[ivar]->do_more_work_vars) failed." );
               g[ivar]->do_more_work_nvars = params->extra_work_nvars;
               for ( i=0; i<params->extra_work_nvars; i++ ) {
                  wvar = ( random () % params->numvars );
                  g[ivar]->do_more_work_vars[i] = wvar;
               }
            }
         }
   } // End switch ( params->extra_work_percent )

   params->nvars_with_extra_work = nvars_with_extra_work;

   // Allocate and initialize struct associated with each state variable.

   opslist[0] = MG_COLLECTIVE_OP_MAX;
   opslist[1] = MG_COLLECTIVE_OP_MIN;
   opslist[2] = MG_COLLECTIVE_OP_SUM;

   for ( ivar=0; ivar<params->numvars; ivar++ ) {

      g[ivar]->toggle = 0;

      g[ivar]->lmax = -1111.0;            // Maximum value across local grid.
      g[ivar]->lmin = -2222.0;            // Minimum value across local grid.
      g[ivar]->lsum = -3333.0;            // Summation across local grid.

      g[ivar]->gmax = -1111.1;            // Maximum value across global grid.
      g[ivar]->gmin = -2222.1;            // Minimum value across global grid.
      g[ivar]->gsum = -3333.1;            // Summation across global grid.

      // Max, min, and summation, by thread, across the local grid.
      g[ivar]->thr_max = (MG_REAL*)MG_CALLOC ( mgpp.num_threads, sizeof(MG_REAL) ); 
      g[ivar]->thr_min = (MG_REAL*)MG_CALLOC ( mgpp.num_threads, sizeof(MG_REAL) ); 
      g[ivar]->thr_sum = (MG_REAL*)MG_CALLOC ( mgpp.num_threads, sizeof(MG_REAL) ); 

      for ( ithread=0; ithread<mgpp.num_threads; ithread++ ) {
         g[ivar]->thr_max[ithread] = -1111.2;
         g[ivar]->thr_min[ithread] = -2222.2;
         g[ivar]->thr_sum[ithread] = -3333.2;
      }
      g[ivar]->lflux = 0.0;
      g[ivar]->gflux = 0.0;

      g[ivar]->thr_flux = (MG_REAL*)MG_CALLOC ( mgpp.num_threads, sizeof(MG_REAL) );
      for ( ithread=0; ithread<mgpp.num_threads; ithread++ ) {
         g[ivar]->thr_flux[ithread] = 0.0;   
         //printf ( "[pe %d] g[%d]->thr_flux[%d]=%4.2e \n", mgpp.mype, ivar, ithread, g[ivar]->thr_flux[ithread] );
      }
   }
   if ( params->percent_sum == 100 ) {

      for ( ivar=0; ivar<params->numvars; ivar++ )
         g[ivar]->check_answer = 1;   // every grid is summed.

      params->num_sum_grid = params->numvars;
   }
   else if ( params->percent_sum == 0 ) {

      for ( ivar=0; ivar<params->numvars; ivar++ )
         g[ivar]->check_answer = 0;   // no grids are summed.

      params->num_sum_grid = 0;

   } 
   else {

      params->num_sum_grid = 0;

      for ( ivar=0; ivar<params->numvars; ivar++ ) {
         num = rand () % 100;
         if ( num < params->percent_sum ) {
            g[ivar]->check_answer = 1;
            params->num_sum_grid++;
         } 
         else {
            g[ivar]->check_answer = 0;
         }
      }
   }
   for ( ivar=0; ivar<params->numvars; ivar++ ) {

      if ( g[ivar]->check_answer ) {
         g[ivar]->op = MG_COLLECTIVE_OP_SUM;
      } 
      else {
         op = rand ( ) % 3; // FIXME rfbarre: Ensure this is the same value across all processes.
         g[ivar]->op = opslist[op];
      }
   }

#if defined _MG_DEBUG
   srand( (unsigned)7 );
#else
//   srand( (unsigned)time( NULL ) );
#endif

   // Allocate state variable values, include ghost space.
   count = (params->nx+(2*params->ghostdepth))*(params->ny+(2*params->ghostdepth))*(params->nz+(2*params->ghostdepth));

   for ( ivar=0; ivar<params->numvars; ivar++ ) { // Initialize grid values.
      g[ivar]->values1 = (MG_REAL*)MG_CALLOC ( count, sizeof(MG_REAL) );
      if ( g[ivar]->values1 == NULL ) {
         fprintf ( stderr, "Allocation of g[%d]->values1 of count %d failed \n", ivar, count );
      }
      MG_Assert ( g[ivar]->values1 != NULL, "MG_Grid_init: MG_CALLOC ( g[ivar]->values1 )" );

      ierr = MG_Fill_grid ( *params, g[ivar] );
      MG_Assert ( !ierr, "MG_Grid_init: MG_Fill_grid" );

      g[ivar]->values2 = (MG_REAL*)MG_CALLOC ( count, sizeof(MG_REAL) );
      if ( g[ivar]->values2 == NULL ) {
         fprintf ( stderr, "Allocation of g[%d]->values2 of count %d failed \n", ivar, count );
      }
      MG_Assert ( g[ivar]->values2 != NULL, "MG_Grid_init: MG_CALLOC ( g[ivar]->values2 )" );
   }
   return ( ierr );

} // End MG_Grid_init

//  ===================================================================================

int MG_Fill_grid ( InputParams params, StateVar *g )
{
   // Fill input array with random values.

   // ------------------
   // Local Declarations
   // ------------------

   int 
      gd,          // ghostdepth shorthand.
      i, j, k,     // Counters for ghost space.
      ierr,        // Return status.
      maxval,      // Control magnitude of values.
      nx, ny, nz;  // Shorthand

   MG_REAL
      lsource;     // Sum across local grid.

   // ---------------------
   // Executable Statements
   // ---------------------

   ierr = 0;

   nx = params.nx;
   ny = params.ny;
   nz = params.nz;

   gd = params.ghostdepth;

   // Initialize all zones to 0, including ghosts.

   for ( k=0; k<nz+(2*gd); k++ ) {
      for ( j=0; j<ny+(2*gd); j++ ) {
         for ( i=0; i<nx+(2*gd); i++ ) {
            g->values1(i,j,k) = 0.0;
         }
      }
   }
   lsource = 0.0;

   maxval = MG_Max3i ( params.nx, params.ny,params.nz ) + 1;

   switch ( params.init_grid_values ) 
   {
      case MG_INIT_GRID_ZEROS:

         break;

      case MG_INIT_GRID_RANDOM:

         for ( k=gd; k<=nz+gd-1; k++ ) {
            for ( j=gd; j<=ny+gd-1; j++ ) {
               for ( i=gd; i<=nx+gd-1; i++ ) {
                  g->values1(i,j,k) = (MG_REAL)((rand() % maxval)+1); // FIXME rfbarre: Need parallel rand()
                  lsource += g->values1(i,j,k);
               }
            }
         }
         break;

      default:

         if ( mgpp.rootpe == mgpp.mype )
            fprintf ( stderr, "MG_Fill_grid: Unknown grid initialization: %d \n", params.init_grid_values );

         ierr = -1;
         MG_Assert ( !ierr, "MG_Fill_grid: Unknown grid initialization" );

         break;

   } // End switch ( params.init_grid_values ) 

   // Compute the total source.

#if defined _MG_MPI
   ierr = CALL_MPI_Allreduce ( &lsource, &(g->source_total), 1, MG_COMM_REAL,
                          MPI_SUM, MPI_COMM_MG );
   MG_Assert ( MPI_SUCCESS == ierr, "MG_Fill_grid : CALL_MPI_Allreduce (lsource)" );
#else
   g->source_total = lsource;
#endif

   return ( ierr );

} // MG_Fill_grid

// ======================================================================================
