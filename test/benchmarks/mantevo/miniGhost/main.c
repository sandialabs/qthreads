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

#define MG_EXTERN

#include "mg_tp.h"

#if defined _MG_QT_PROFILE
aligned_t num_tasks;
#endif

#if defined _MG_QT
typedef struct {
   int           ivar;
   StateVar   ** g;
   BlockInfo   * blk_iblk;
   InputParams * params;
} compute_block_args_t;

aligned_t compute_block(void * args_)
{
   compute_block_args_t * args = (compute_block_args_t *)args_;

   int            const ivar     = args->ivar;
   StateVar    ** const g        = args->g;
   BlockInfo    * const blk_iblk = args->blk_iblk;
   InputParams  * const params   = args->params;

   int ierr = 0;

   MG_Stencil ( *params, g, *blk_iblk, ivar );

#    if defined _MG_QT_PROFILE
   qthread_incr(&num_tasks, 1);
#    endif

   return ierr;
}
#endif

int main ( int argc, char* argv[] )
{
   int
     count,                        // Array length.
     i,
     iblk, ivar,                   // Counters
     itask,                        // Counter
     tstep,                        // Counter
     ierr,                         // Return value
     ispike,                       // Counter
     ithread,                      // Counter
     len,                          // Temporary var for computing thread block offsets.
     mype,
     nblks_xdir,                   // Number of blocks in x direction.
     nblks_ydir,                   // Number of blocks in y direction.
     nblks_zdir,                   // Number of blocks in z direction.
     numthreads,
     num_errors = 0,               // Number of variables in error.
     rem_x,                        // Remainder of domain in x direction.
     rem_y,                        // Remainder of domain in y direction.
     rem_z,                        // Remainder of domain in z direction.
     rootpe,
     tasks_per_tstep;              // Number of tasks to be spawned per time step.

   double 
      time_start;

   InputParams 
      params;             // Problem parameters.

   // ---------------------
   // Executable Statements
   // ---------------------

   memory_stats.numallocs = 0; // Struct instantiated in mg_perf.h
   memory_stats.count     = 0;
   memory_stats.bytes     = 0;

#if defined (_MG_QT) && !defined (_MG_MPIQ)
            qthread_initialize();
#endif

   ierr = MG_Init ( argc, argv, &params );
   MG_Assert ( !ierr, "main:MG_Init" );

   mype       = mgpp.mype;
   numthreads = mgpp.num_threads;
   rootpe     = mgpp.rootpe;

   tasks_per_tstep = params.numvars*params.numblks;

   // Print problem information to stdout.
   ierr = MG_Print_header ( params );

   // Allocate and configure sub-blocks.

   params.numblks = ( params.nx / params.blkxlen ) *
                    ( params.ny / params.blkylen ) *
                    ( params.nz / params.blkzlen );

   blk = (BlockInfo**)MG_CALLOC ( params.numblks, sizeof(BlockInfo*) );
   MG_Assert ( !ierr, "main: Allocation of **blk" );

   ierr = MG_Block_init ( &params, blk );
   MG_Assert ( !ierr, "main: MG_Block_init" );

#if defined _MG_OPENMP

   params.thread_offset_xy = (int*)MG_CALLOC ( mgpp.num_threads, sizeof(int) );
   MG_Assert ( !ierr, "main: Allocation of params.thread_offset_xy" );

   params.thread_offset_xz = (int*)MG_CALLOC ( mgpp.num_threads, sizeof(int) );
   MG_Assert ( !ierr, "main: Allocation of params.thread_offset_xz" );

   params.thread_offset_yz = (int*)MG_CALLOC ( mgpp.num_threads, sizeof(int) );
   MG_Assert ( !ierr, "main: Allocation of params.thread_offset_yz" );

   for ( i=0; i<mgpp.num_threads; i++ ) {
      len = params.blkxlen * ( params.blkylen / mgpp.num_threads ); // For use in boundary exchange routines.
      params.thread_offset_xy[i] = len * i;

      len = params.blkxlen * ( params.blkzlen / mgpp.num_threads ); // For use in boundary exchange routines.
      params.thread_offset_xz[i] = len * i;

      len = params.blkylen * ( params.blkzlen / mgpp.num_threads ); // For use in boundary exchange routines.
      params.thread_offset_yz[i] = len * i;

      //printf ( "[pe %d:%d] offsets = (%d, %d, %d) \n", mgpp.mype, mgpp.thread_id, params.thread_offset_xy[i], params.thread_offset_xz[i], params.thread_offset_yz[i] );
   }
   MG_Barrier ();

#endif

   // Allocate and initialize state variables.
   g = (StateVar**)MG_CALLOC ( params.numvars, sizeof(StateVar*) );
   if ( g == NULL ) {
      fprintf ( stderr, "Allocation of arrays of g %d failed \n",
                params.numvars*(int)sizeof(StateVar*) );
      ierr =  -1;
      MG_Assert ( !ierr, "main: Allocation of **g" );
   }

   ierr = MG_Grid_init ( &params, g );
   MG_Assert ( !ierr, "main: MG_Grid_init" );

   // Allocate and set up spikes.
   spikes = (SpikeInfo**)MG_CALLOC ( params.numspikes, sizeof(SpikeInfo*) );
   if ( spikes == NULL ) {
      fprintf ( stderr, "Allocation of spikes of size %d failed \n", params.numspikes );
      MG_Assert ( spikes != NULL, "main: Allocation of array of structs spikes" );
   }
   ierr = MG_Spike_init ( params, spikes );
   MG_Assert ( !ierr, "main: MG_Spike_init" );

   tstep = 1; // Allows for some initialization, i.e. spike insertion.

   // Synchronize for timing.
   ierr = MG_Barrier ( );
   MG_Assert ( !ierr, "main:MG_Barrier" );

#if defined _USE_PAT_API
   ierr = PAT_region_begin ( 1, "miniGhost" );
   MG_Assert ( !ierr, "main: PAT_region_begin" );
#endif

#if defined HAVE_GEMINI_COUNTERS
   ierr = gpcd_start ( );
   MG_Assert ( !ierr, "main: gpcd_start" );
#endif

   MG_Time_start(time_start);

   for ( ispike=0; ispike<params.numspikes; ispike++ ) {

      ierr = MG_Spike_insert ( params, spikes, ispike, g, tstep );
      MG_Assert ( !ierr, "main:MG_Spike_insert" );

      itask = 0;
      for ( tstep=1; tstep<=params.numtsteps; tstep++ ) { // Time step loop.

#if defined _MG_QT
            compute_block_args_t compute_block_args;
            qt_sinc_t sinc;
            qt_sinc_init(&sinc, 0, NULL, NULL, params.numvars * params.numblks);
#endif

         for ( ivar=0; ivar<params.numvars; ivar++ ) {   // Loop over variables.

            for ( ithread=0; ithread<numthreads; ithread++ ) {
               g[ivar]->thr_flux[ithread] = 0.0;
            }
            //MG_Barrier ( );

            // Spawn tasks, one per domain subblk.
            for ( iblk=0; iblk<params.numblks; iblk++ ) {

#if defined _MG_SERIAL || defined _MG_MPI
#  if defined _MG_QT // {
                //compute_block_args_t const compute_block_args = { ivar, iblk, g[ivar], blk[iblk], params};
                compute_block_args.ivar     = ivar;
                compute_block_args.g        = g;
                compute_block_args.blk_iblk = blk[iblk];
                compute_block_args.params   = &params;

                {
                    unsigned int          task_flags;
                    qthread_shepherd_id_t target_shep;

                    task_flags = QTHREAD_SPAWN_RET_SINC_VOID;
                    if (1 == blk[iblk]->info) {
                        task_flags |= QTHREAD_SPAWN_SIMPLE;
                        target_shep = NO_SHEPHERD; 
                    } else {
                        target_shep = 0;
                    }
 		    fprintf(stderr, "spawning %d\n", compute_block);
                    qthread_spawn ( compute_block, 
                            &compute_block_args, 
                            sizeof(compute_block_args_t), 
                            &sinc,
                            0,    /* no preconds */
                            NULL, /* no preconds */
                            target_shep,
                            task_flags);
                }
#  else        // MPI everywhere and MPI + OpenMP. } {
               ierr = MG_Stencil ( params, g, *blk[iblk], ivar );
               MG_Assert ( !ierr, "main:MG_Stencil" );
#  endif // }
#elif defined _MG_QT
#endif
            } // End loop over blks.

         } // end params.numvars

#if defined _MG_QT
         qt_sinc_wait ( &sinc, NULL );
         qt_sinc_fini ( &sinc );
#endif

         /* Toggle variable domains _after_ synchronizing all tasks in
          * this time step. */
         for ( ivar=0; ivar<params.numvars; ivar++ ) {   // Loop over variables.
            g[ivar]->toggle++;
            g[ivar]->toggle = g[ivar]->toggle % 2;
         }

         // Correctness check across all variables.

         num_errors = MG_Sum_grid ( params, g, tstep, 0 );
         if ( num_errors ) {
            if ( mype == rootpe ) {
               fprintf ( stderr, 
                         "\n *** Iteration not correct; %d errors reported after iteration %d.\n\n",
                         num_errors, tstep );
            }
            MG_Assert ( !num_errors, "main: Terminating execution" );
         }
         else {
            if ( mype == rootpe ) {
               if ( tstep % params.check_answer_freq == 0 ) {
                  fprintf ( stdout, " End time step %d for spike %d. \n", tstep, ispike+1 );
               }
            }
         }
         // FIXME: Inserted for testing.
         ierr = MG_Barrier ();
      } // End tsteps

   } // End spike insertion.
      
#if defined _USE_PAT_API
   ierr = PAT_region_end ( 1 );
   MG_Assert ( !ierr, "main: PAT_region_end" );
#endif

   MG_Time_accum(time_start,timings.total);

#if defined HAVE_GEMINI_COUNTERS
   ierr = gpcd_end ( );
   MG_Assert ( !ierr, "main: gpcd_end" );
#endif

   // Final correctness check.
   num_errors = MG_Sum_grid ( params, g, tstep+1, 1 );
   if ( mype == rootpe ) {
      if ( !num_errors )  {
         fprintf ( stdout, "\n\n ** Final correctness check PASSED. ** \n\n" );
      }
      else {
         fprintf ( stdout, "\n\n ** Final correctness check FAILED. (%d variables showed errors) ** \n\n", 
                   num_errors );
      }
   }
   ierr = MG_Report_performance ( params );
   MG_Assert ( !ierr, "main: MG_Report_performance" );

   // Release workspace.

   if ( g != NULL )  {
      free ( g );
   }
   if ( blk != NULL )  {
      free ( blk );
   }

   ierr = MG_Terminate ( );
   MG_Assert ( !ierr, "main: MG_Terminate" );

   exit(0);

// End main.c
}
