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

   // Statistics parameters

enum {
   MIN = 0,
   MAX,
   SUM,
   AVG,
   STDDEV,

   NUM_STATS
};

int MG_Init_perf ( InputParams params )
{
   // ---------------
   // Local Variables
   // ---------------

   int
      i,             // Counter.
      ierr = 0,      // Return status.
      num_threads;   // Shorthand for mgpp.num_threads.

   // ---------------------
   // Executable Statements
   // ---------------------

   num_threads = mgpp.num_threads;

   timings.total         = 0.0;
   timings.bc_accum      = 0.0;
   timings.sum_grid      = 0.0;
   timings.sum_grid_comm = 0.0;

   timings.stencil = (double*)MG_DCALLOC_INIT ( num_threads );
   MG_Assert ( timings.stencil != NULL, "MG_Init_perf: Failed to allocate space for timings.stencil" );

   timings.bc_comp = (double*)MG_DCALLOC_INIT ( num_threads );
   MG_Assert ( timings.bc_comp != NULL, "MG_Init_perf: Failed to allocate space for timings.bc_comp" );

   timings.bex = (double*)MG_DCALLOC_INIT ( num_threads );
   MG_Assert ( timings.bex != NULL, "MG_Init_perf: Failed to allocate space for timings.bex" );

#if defined _MG_TIMING_L1 // {
   timings.pack_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_north != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_north" );

   timings.pack_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_south != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_south" );

   timings.pack_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_east != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_east" );

   timings.pack_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_west != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_west" );

   timings.pack_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_front != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_front" );

   timings.pack_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.pack_back != NULL, "MG_Init_perf: Failed to allocate space for timings.pack_back" );

   if ( params.comm_strategy == MG_COMM_STRATEGY_SIR || params.comm_strategy == MG_COMM_STRATEGY_ISIR ) {

      // Accumulated into a single value for non-blocking receives, since these don't take much time.
      timings.recv = (double*)MG_DCALLOC_INIT ( num_threads );
      MG_Assert ( timings.recv != NULL, "MG_Init_perf: Failed to allocate space for timings.recv" );

      // Non-blocking recv completion.
      timings.wait_recv_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_north != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_north" );

      timings.wait_recv_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_south != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_south" );

      timings.wait_recv_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_east != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_east" );

      timings.wait_recv_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_west != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_west" );

      timings.wait_recv_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_front != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_front" );

      timings.wait_recv_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_recv_back != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_recv_back" );
   }
   else {
      // Blocking recvs.
      timings.recv_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_north != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_north" );

      timings.recv_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_south != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_south" );

      timings.recv_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_east != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_east" );

      timings.recv_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_west != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_west" );

      timings.recv_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_front != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_front" );

      timings.recv_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.recv_back != NULL, "MG_Init_perf: Failed to allocate space for timings.recv_back" );
   }
   if ( params.comm_strategy == MG_COMM_STRATEGY_SR || params.comm_strategy == MG_COMM_STRATEGY_SIR ) {

      // Blocking sends.
      timings.send_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_north != NULL, "MG_Init_perf: Failed to allocate space for timings.send_north" );

      timings.send_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_south != NULL, "MG_Init_perf: Failed to allocate space for timings.send_south" );
   
      timings.send_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_east != NULL, "MG_Init_perf: Failed to allocate space for timings.send_east" );

      timings.send_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_west != NULL, "MG_Init_perf: Failed to allocate space for timings.send_west" );
   
      timings.send_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_front != NULL, "MG_Init_perf: Failed to allocate space for timings.send_front" );

      timings.send_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send_back != NULL, "MG_Init_perf: Failed to allocate space for timings.send_back" );
   }
   else {

      timings.send = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.send != NULL, "MG_Init_perf: Failed to allocate space for timings.send" );

      // Non-blocking send completion.

      timings.wait_send = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send" );

      timings.wait_send_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_north != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_north" );

      timings.wait_send_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_south != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_south" );

      timings.wait_send_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_east != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_east" );

      timings.wait_send_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_west != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_west" );

      timings.wait_send_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_front != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_front" );

      timings.wait_send_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
      MG_Assert ( timings.wait_send_back != NULL, "MG_Init_perf: Failed to allocate space for timings.wait_send_back" );
   }

   // Unpacking
   timings.unpack_north = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_north != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_north" );

   timings.unpack_south = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_south != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_south" );

   timings.unpack_east = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_east != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_east" );

   timings.unpack_west = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_west != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_west" );

   timings.unpack_front = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_front != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_front" );

   timings.unpack_back = (double*)MG_DCALLOC_INIT ( num_threads*MAX_NUM_NEIGHBORS );
   MG_Assert ( timings.unpack_back != NULL, "MG_Init_perf: Failed to allocate space for timings.unpack_back" );
#endif // _MG_TIMING_L1 }

   return ( ierr );
}

//  ===================================================================================

int MG_Report_performance ( InputParams params )
{

   // ---------------
   // Local Variables
   // ---------------

   char
      comm_strategy_s[25],   // MPI Send / recv blocking or nonblocking combinations.
      //filename_yaml[100],    // File target for yaml formatted results.
      stencil_s[15];         // For constructing file name.

   int 
      ierr = 0,              // Return status.
      ivar,                  // Counter.
      len=50,
      mype,
      numpes,
      rootpe;

#if defined _MG_OPENMP // {
   int
      num_threads;
#endif // _MG_OPENMP }

   char 
      hostname[len];

   time_t
      current_time;

   char
      *c_time_string;

#if defined _MG_INT4
   int
#elif defined _MG_INT8
   long
#endif
      nfp_stencil_only_tstep_pe,
      nfp_stencil_extra_work_tstep_pe,

      num_flops_stencil_tstep_pe,
      num_flops_bc_tstep_pe,
      num_flops_sum_grid_tstep_pe;

   MG_REAL
      iter_err_min,      // Iteration error minimum across all variables.
      iter_err_max,      // Iteration error maximum across all variables.
      iter_err_sum,      // Iteration error sum across all variables.
      iter_err_avg,      // Iteration error average across all variables.
      mem_allocated;

   double
      gflops_pe,
      *times_all,
      time_bc_total,
      *times_bc_comp,    // Computation of boundary conditions
      *times_bc_accum,   // Accumulation for determining total flux.
      *times_stencil, 
      time_other,
      *times_sum_grid;

#if defined _MG_TIMING_L0 || _MG_TIMING_L1 // {
   double
      gflops_pe_bc,
      gflops_pe_sum_grid,
      gflops_pe_stencil,
      time_comm,
      time_comp;
#endif // } 

#if defined _MG_MPI // {
#if defined _MG_TIMING_L0 // {
   double
      *times_bex,
      *times_sum_grid_comm;

#endif // _MG_TIMING_L0 }
#if defined _MG_TIMING_L1 // {

   double

      *times_bex,

      *times_sum_grid_comm,

      *times_pack_north,
      *times_pack_south,
      *times_pack_east,
      *times_pack_west,
      *times_pack_front,
      *times_pack_back,

      *times_send,

      *times_send_north,
      *times_send_south,
      *times_send_east,
      *times_send_west,
      *times_send_front,
      *times_send_back,

      *times_recv,

      *times_recv_north,
      *times_recv_south,
      *times_recv_east,
      *times_recv_west,
      *times_recv_front,
      *times_recv_back,

      *times_unpack_north,
      *times_unpack_south,
      *times_unpack_east,
      *times_unpack_west,
      *times_unpack_front,
      *times_unpack_back,

      *times_wait_send,

      *times_wait_send_north,
      *times_wait_send_south,
      *times_wait_send_east,
      *times_wait_send_west,
      *times_wait_send_front,
      *times_wait_send_back,

      *times_wait_recv_north,
      *times_wait_recv_south,
      *times_wait_recv_east,
      *times_wait_recv_west,
      *times_wait_recv_front,
      *times_wait_recv_back,

      time_pack,
      time_send,
      time_recv,
      time_wait_recv,
      time_wait_send,
      time_unpack;

#endif // _MG_TIMING_L1 }

#endif // _MG_MPI }

   // ---------------------
   // Executable Statements
   // ---------------------

   mype        = mgpp.mype;
#if defined _MG_OPENMP // {
   num_threads = mgpp.num_threads;
#endif // }
   numpes      = mgpp.numpes;
   rootpe      = mgpp.rootpe;

   times_all = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_all != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_all )" );

   times_stencil = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_stencil != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_stencil )" );

   times_bc_comp = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_bc_comp != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_bc_comp )" );

   times_bc_accum = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_bc_accum != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_bc_accum )" );

   times_sum_grid = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_sum_grid != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_sum_grid )" );

   // Collect and compile timings.

   MG_Profile_stats ( &timings.total, times_all, 0 ); // Total time

   MG_Profile_stats ( timings.stencil, times_stencil, 1 ); // Stencil timings

   MG_Profile_stats ( timings.bc_comp, times_bc_comp, 1 ); // Boundary conditions work per task.

   MG_Profile_stats ( &timings.bc_accum, times_bc_accum, 0 ); // Boundary conditions flux
                                                             // accumulated.

   time_bc_total = times_bc_comp[AVG] + times_bc_accum[AVG];

   MG_Profile_stats ( &timings.sum_grid, times_sum_grid, 0 ); // Check answer

#if defined _MG_MPI // {

   // Boundary exchange
   times_bex = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_bex != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_bex )" );

   MG_Profile_stats ( timings.bex, times_bex, 1 );

   // Sum grid
   times_sum_grid_comm = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_sum_grid_comm != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_sum_grid_comm )" );

   MG_Profile_stats ( &timings.sum_grid_comm, times_sum_grid_comm, 0 ); // Sum grid Allreduce

#if defined _MG_TIMING_L1 // {

   // Packing:
   times_pack_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_north )" );

   times_pack_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_south )" );

   times_pack_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_east )" );

   times_pack_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_west )" );

   times_pack_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_front )" );

   times_pack_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_pack_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_pack_back )" );

   // Sending:

   switch ( params.comm_strategy ) {

      case MG_COMM_STRATEGY_SR:
      case MG_COMM_STRATEGY_SIR:

         times_send_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_north )" );

         times_send_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_south )" );

         times_send_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_east )" );

         times_send_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_west )" );

         times_send_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_front )" );

         times_send_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send_back )" );
   
         break;

      case MG_COMM_STRATEGY_ISR:

         times_send = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send )" );

         times_wait_send = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send )" );

         break;

      case MG_COMM_STRATEGY_ISIR:

         times_send = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_send != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_send )" );

         times_wait_send_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_north )" );
   
         times_wait_send_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_south )" );
   
         times_wait_send_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_east )" );
   
         times_wait_send_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_west )" );
   
         times_wait_send_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_front )" );
   
         times_wait_send_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_send_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_send_back )" );

         break;
  
   } // End switch ( params.comm_strategy )

   // Receiving:

   switch ( params.comm_strategy ) {

      case MG_COMM_STRATEGY_SR:
      case MG_COMM_STRATEGY_ISR:

         times_recv_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_north )" );

         times_recv_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_south )" );

         times_recv_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_east )" );

         times_recv_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_west )" );

         times_recv_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_front )" );

         times_recv_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv_back )" );

         break;

      case MG_COMM_STRATEGY_SIR:
      case MG_COMM_STRATEGY_ISIR:

         times_recv = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_recv != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_recv )" );

         times_wait_recv_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_north )" );

         times_wait_recv_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_south )" );
   
         times_wait_recv_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_east )" );
   
         times_wait_recv_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_west )" );
   
         times_wait_recv_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_front )" );
      
         times_wait_recv_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
         MG_Assert ( times_wait_recv_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_wait_recv_back )" );

         break;

   } // End switch ( params.comm_strategy )

   times_unpack_north = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_north != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_north )" );

   times_unpack_south = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_south != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_south )" );

   times_unpack_east = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_east != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_east )" );

   times_unpack_west = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_west != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_west )" );

   times_unpack_front = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_front != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_front )" );

   times_unpack_back = (double*)MG_DCALLOC_INIT ( NUM_STATS );
   MG_Assert ( times_unpack_back != NULL, "MG_Report_performance: MG_DCALLOC_INIT ( times_unpack_back )" );

   MG_Profile_stats ( timings.pack_north, times_pack_north, 1 );
   MG_Profile_stats ( timings.pack_south, times_pack_south, 1 );
   MG_Profile_stats ( timings.pack_east,  times_pack_east,  1 );
   MG_Profile_stats ( timings.pack_west,  times_pack_west,  1 );
   MG_Profile_stats ( timings.pack_back,  times_pack_back,  1 );
   MG_Profile_stats ( timings.pack_front, times_pack_front, 1 );

   switch ( params.comm_strategy ) {

      case MG_COMM_STRATEGY_SR:

         MG_Profile_stats ( timings.send_north, times_send_north, 1 );
         MG_Profile_stats ( timings.send_south, times_send_south, 1 );
         MG_Profile_stats ( timings.send_east,  times_send_east,  1 );
         MG_Profile_stats ( timings.send_west,  times_send_west,  1 );
         MG_Profile_stats ( timings.send_back,  times_send_back,  1 );
         MG_Profile_stats ( timings.send_front, times_send_front, 1 );

         MG_Profile_stats ( timings.recv_north, times_recv_north, 1 );
         MG_Profile_stats ( timings.recv_south, times_recv_south, 1 );
         MG_Profile_stats ( timings.recv_east,  times_recv_east,  1 );
         MG_Profile_stats ( timings.recv_west,  times_recv_west,  1 );
         MG_Profile_stats ( timings.recv_back,  times_recv_back,  1 );
         MG_Profile_stats ( timings.recv_front, times_recv_front, 1 );

         break;

      case MG_COMM_STRATEGY_ISR:

         MG_Profile_stats ( timings.send, times_send, 1 );

         MG_Profile_stats ( timings.wait_send, times_wait_send, 1 );
   
         MG_Profile_stats ( timings.recv_north, times_recv_north, 1 );
         MG_Profile_stats ( timings.recv_south, times_recv_south, 1 );
         MG_Profile_stats ( timings.recv_east,  times_recv_east,  1 );
         MG_Profile_stats ( timings.recv_west,  times_recv_west,  1 );
         MG_Profile_stats ( timings.recv_back,  times_recv_back,  1 );
         MG_Profile_stats ( timings.recv_front, times_recv_front, 1 );

         break;

      case MG_COMM_STRATEGY_SIR:

         MG_Profile_stats ( timings.send_north, times_send_north, 1 );
         MG_Profile_stats ( timings.send_south, times_send_south, 1 );
         MG_Profile_stats ( timings.send_east,  times_send_east,  1 );
         MG_Profile_stats ( timings.send_west,  times_send_west,  1 );
         MG_Profile_stats ( timings.send_back,  times_send_back,  1 );
         MG_Profile_stats ( timings.send_front, times_send_front, 1 );

         MG_Profile_stats ( timings.recv, times_recv, 1 );

         MG_Profile_stats ( timings.wait_recv_north, times_wait_recv_north, 1 );
         MG_Profile_stats ( timings.wait_recv_south, times_wait_recv_south, 1 );
         MG_Profile_stats ( timings.wait_recv_east,  times_wait_recv_east,  1 );
         MG_Profile_stats ( timings.wait_recv_west,  times_wait_recv_west,  1 );
         MG_Profile_stats ( timings.wait_recv_back,  times_wait_recv_back,  1 );
         MG_Profile_stats ( timings.wait_recv_front, times_wait_recv_front, 1 );

         break;

      case MG_COMM_STRATEGY_ISIR:

         MG_Profile_stats ( timings.send, times_send, 1 );

         MG_Profile_stats ( timings.wait_send_north, times_wait_send_north, 1 );
         MG_Profile_stats ( timings.wait_send_south, times_wait_send_south, 1 );
         MG_Profile_stats ( timings.wait_send_east,  times_wait_send_east,  1 );
         MG_Profile_stats ( timings.wait_send_west,  times_wait_send_west,  1 );
         MG_Profile_stats ( timings.wait_send_back,  times_wait_send_back,  1 );
         MG_Profile_stats ( timings.wait_send_front, times_wait_send_front, 1 );

         MG_Profile_stats ( timings.recv, times_recv, 1 );

         MG_Profile_stats ( timings.wait_recv_north, times_wait_recv_north, 1 );
         MG_Profile_stats ( timings.wait_recv_south, times_wait_recv_south, 1 );
         MG_Profile_stats ( timings.wait_recv_east,  times_wait_recv_east,  1 );
         MG_Profile_stats ( timings.wait_recv_west,  times_wait_recv_west,  1 );
         MG_Profile_stats ( timings.wait_recv_back,  times_wait_recv_back,  1 );
         MG_Profile_stats ( timings.wait_recv_front, times_wait_recv_front, 1 );

         break; 

   } // End switch ( params.comm_strategy )

   MG_Profile_stats ( timings.unpack_north, times_unpack_north, 1 );
   MG_Profile_stats ( timings.unpack_south, times_unpack_south, 1 );
   MG_Profile_stats ( timings.unpack_east,  times_unpack_east,  1 ); 
   MG_Profile_stats ( timings.unpack_west,  times_unpack_west,  1 ); 
   MG_Profile_stats ( timings.unpack_back,  times_unpack_back,  1 ); 
   MG_Profile_stats ( timings.unpack_front, times_unpack_front, 1 ); 

#endif // _MG_TIMING_L1 // }
#endif // _MG_MPI }

   if ( mype != rootpe ) {
      return ( ierr );
   }
   gethostname ( hostname, len );

   // Obtain current time as seconds elapsed since the Epoch.
   current_time = time(NULL);

   // Convert to local time format.
   c_time_string = ctime(&current_time);

   mem_allocated = (MG_REAL)( ((params.nx+(2*params.ghostdepth))*
                               (params.ny+(2*params.ghostdepth))*
                               (params.nz+(2*params.ghostdepth))) * sizeof(MG_REAL) ) * 
                               ( 2 * params.numvars ) / GIGA;
 
   fprintf ( stdout, "\n\n" );
   fprintf ( stdout, " ========================================================================= \n" );
#if defined _MG_SERIAL // {
   fprintf ( stdout, " Mantevo miniGhost, task parallel (serial) version. \n" );
   fprintf ( stdout, "\n" );
#elif defined _MG_MPI && _MG_OPENMP
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI+OpenMP) version \n" );
   fprintf ( stdout, "\n" );
   if ( num_threads == 1 ) {
      fprintf ( stdout, " 1 OpenMP thread on each of %d MPI processes \n", numpes );
   }
   else {
      fprintf ( stdout, " %d OpenMP threads on each of %d MPI processes \n", num_threads, numpes );
   }
#elif defined _MG_MPI && _MG_OPENMP
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI+OpenMP) version \n" );
   fprintf ( stdout, "\n" );
   if ( num_threads == 1 ) {
      fprintf ( stdout, " 1 OpenMP thread on each of %d MPI processes \n", numpes );
   }
   else {
      fprintf ( stdout, " %d OpenMP threads on each of %d MPI processes \n", num_threads, numpes );
   }
#elif defined _MG_MPI && _MG_QT
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI+qthread) version \n" );
   size_t num_qtsheps   = qthread_readstate(TOTAL_SHEPHERDS);
   size_t num_qtworkers = qthread_readstate(TOTAL_WORKERS);
   size_t qtstack_size  = qthread_readstate(STACK_SIZE);
   fprintf ( stdout, "\n" );
   if ( numpes == 1 ) {
      fprintf ( stdout, " %d MPI process \n", numpes );
   }
   else {
      fprintf ( stdout, " %d MPI processes \n", numpes );
   }
   fprintf ( stdout, " %d qthreads shepherds \n", (int)num_qtsheps );
   fprintf ( stdout, " %d qthreads workers   \n", (int)num_qtworkers );
   fprintf ( stdout, " qthreads  stack size %d \n", (int)qtstack_size );
#elif defined _MG_MPI
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI) version. \n" );
   fprintf ( stdout, "\n" );
   if ( numpes == 1 ) {
      fprintf ( stdout, " 1 MPI process " );
   }
   else {
      fprintf ( stdout, " %d MPI processes ", numpes );
   }
#elif defined _MG_OPENMP
   fprintf ( stdout, " miniGhost results, task parallel (OpenMP) version. \n" );
   fprintf ( stdout, "\n" );
   if ( num_threads == 1 ) {
      fprintf ( stdout, " 1 OpenMP thread" );
   }
   else {
      fprintf ( stdout, " %d OpenMP threads", num_threads );
   }
#else
   // FIXME: need to clean up logic to match the actual configuration (DTS)
   MG_Report_peformance: Communication protocol not defined. 
#endif // }
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Executed on machine %s \n date/time stamp %s\n", hostname, c_time_string );

   switch ( params.stencil ) {
      case ( MG_STENCIL_2D5PT  ) :
         fprintf ( stdout, " Applied 5 point stencil in 2 dimensions. \n" );
         nfp_stencil_only_tstep_pe = params.nx*params.ny*params.nz*5;
         strcpy ( stencil_s, "2d5ptStencil" );
         break;
      case ( MG_STENCIL_2D9PT  ) :
         fprintf ( stdout, " Applied 9 point stencil in 2 dimensions. \n" );
         nfp_stencil_only_tstep_pe = params.nx*params.ny*params.nz*9;
         strcpy ( stencil_s, "2d9ptStencil" );
         break;
      case ( MG_STENCIL_3D7PT  ) :
         fprintf ( stdout, " Applied 7 point stencil in 3 dimensions. \n" );
         nfp_stencil_only_tstep_pe = params.nx*params.ny*params.nz*7;
         strcpy ( stencil_s, "3d7ptStencil" );
         break;
      case ( MG_STENCIL_3D27PT ) :
         fprintf ( stdout, " Applied 27 point stencil in 3 dimensions. \n" );
         nfp_stencil_only_tstep_pe = params.nx*params.ny*params.nz*27;
         strcpy ( stencil_s, "3d27ptStencil" );
         break;
      default:
         break;
   } // End switch ( params.stencil )

   // Compute num flops per pe for applying boundary conditions

   int num_ops_per_pt;
   fprintf ( stdout, "\n" );
   switch ( params.boundary_condition ) { 
      case ( MG_BC_DIRICHLET ) :
         fprintf ( stdout, "    Dirichlet boundary conditions (MG_BC_DIRICHLET) \n" );
         switch ( params.stencil ) {
            case ( MG_STENCIL_2D5PT ) :
               num_ops_per_pt = 2;
               num_flops_bc_tstep_pe = ( ( 2*params.nx ) + ( 2*params.ny ) ) * params.nz * num_ops_per_pt;
               break;
            case ( MG_STENCIL_2D9PT ) :
               num_ops_per_pt = 4;
               num_flops_bc_tstep_pe = ( ( 2 * ( params.nx+2 ) ) + ( 2 * ( params.ny+2 ) ) ) * params.nz * num_ops_per_pt; 
               break;
            case ( MG_STENCIL_3D7PT ) :
               num_ops_per_pt = 2;
               num_flops_bc_tstep_pe = ( ( ( 2*params.nx ) + ( 2*params.ny ) ) * params.nz * num_ops_per_pt ) +
                                         ( ( 2 * ( params.nx * params.ny ) ) * num_ops_per_pt ); 
               break;
            case ( MG_STENCIL_3D27PT ) :
               num_ops_per_pt = 10;
               num_flops_bc_tstep_pe = ( ( ( 2 * ( params.nx+2 ) ) + ( 2 * ( params.ny+2 ) ) ) * params.nz * num_ops_per_pt ) +
                                         ( ( 2 * ( params.nx * params.ny ) ) * num_ops_per_pt );
               break;
            default:
               break;
         } // End switch ( params.stencil )
         break;
      case ( MG_BC_NEUMANN ):
         ierr = -1;
         MG_Assert ( !ierr,
                     "MG_Report_performanceMG_Boundary_conditions:Neumann boundary conditions (MG_BC_NEUMANN) not yet implemented." );
         break;
      case ( MG_BC_REFLECTIVE ):
         ierr = -1;
         MG_Assert ( !ierr,
                     "MG_Report_performance:Neumann boundary conditions (MG_BC_REFLECTIVE) not yet implemented." );
         break;
      case ( MG_BC_NONE ):
         fprintf ( stdout, "       No boundary conditions applied (MG_BC_NONE) \n" );
         num_flops_bc_tstep_pe = 0;
         break;
      default:
         fprintf ( stderr,
                   "Unknown boundary conditions: %d \n", params.boundary_condition );
         ierr = -1;
         MG_Assert ( !ierr, "MG_Report_performance: unknown boundary conditions" );
   }
   fprintf ( stdout, "\n" );
#if defined _MG_MPI // {
   switch ( params.comm_strategy ) {
      case ( MG_COMM_STRATEGY_SR ) :
         fprintf ( stdout, " Blocking send / blocking recv. (MG_COMM_STRATEGY_SR) \n" );

         strcpy ( comm_strategy_s, "SR" );

         break;
      case ( MG_COMM_STRATEGY_ISR ) :
         fprintf ( stdout, " Nonblocking send / blocking recv. (MG_COMM_STRATEGY_ISR) \n" );

         strcpy ( comm_strategy_s, "ISR" );

         break;
      case ( MG_COMM_STRATEGY_SIR ) :
         fprintf ( stdout, " Blocking send / nonblocking recv. (MG_COMM_STRATEGY_SIR) \n" );

         strcpy ( comm_strategy_s, "SIR" );

         break;
      case ( MG_COMM_STRATEGY_ISIR ) :
         fprintf ( stdout, " Nonblocking send / nonblocking recv. (MG_COMM_STRATEGY_ISIR) \n" );

         strcpy ( comm_strategy_s, "ISIR" );

         break;
      default:
         break;
   } // End switch ( params.comm_strategy )
#if defined _DONT_WORRY_ABOUT_TAGS
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Tags are SHARED for all messages between pairs of comm partners,\n i.e. compiled with -D_DONT_WORRY_ABOUT_TAGS.\n" );
#endif
#elif defined _MG_OPENMP
   strcpy ( comm_strategy_s, "openmp" );
#elif defined _MG_SERIAL
   strcpy ( comm_strategy_s, "serial" );
#endif // }

   nfp_stencil_extra_work_tstep_pe = nfp_stencil_only_tstep_pe * 
                                   ( params.nvars_with_extra_work * params.extra_work_nvars );

   nfp_stencil_only_tstep_pe      *= params.numvars;

   num_flops_stencil_tstep_pe      = nfp_stencil_only_tstep_pe + nfp_stencil_extra_work_tstep_pe;

   num_flops_bc_tstep_pe          *= params.numvars;

   if ( params.check_answer_freq > params.numtsteps ) {
      num_flops_sum_grid_tstep_pe = 0;
   }
   else {
      num_flops_sum_grid_tstep_pe = params.nx * params.ny * params.nz;
      num_flops_sum_grid_tstep_pe *= params.numvars;
      num_flops_sum_grid_tstep_pe /= params.check_answer_freq;
   }
   
#if defined _MG_MPI // {
   int global_nx = params.nx*params.npx;
   int global_ny = params.ny*params.npy;
   int global_nz = params.nz*params.npz;

   fprintf ( stdout, "\n Global grid dimension = %d x %d x %d \n", global_nx, global_ny, global_nz );

   fprintf ( stdout, "\n Local grid dimension  = %d x %d x %d \n", params.nx, params.ny, params.nz );
   fprintf ( stdout, "\n" );
#else // Serial.
   fprintf ( stdout, "\n Grid dimension = %d x %d x %d \n", params.nx, params.ny, params.nz );
   fprintf ( stdout, "\n" );
#endif // _MG_MPI }

   fprintf ( stdout, " Block dimension = %d x %d x %d \n", params.blkxlen, params.blkylen, params.blkzlen );
   fprintf ( stdout, "\n" );
   switch ( params.blkorder ) {

      case ( MG_BLOCK_ORDER_CART ) : // This is the default ordering.
         fprintf ( stdout, " Regular Cartesian block decomposition strategy \n" );
         break;

      case ( MG_BLOCK_ORDER_MTA )  : // 
         fprintf ( stdout, " MTA-style block decomposition strategy \n" );
         break;

      case ( MG_BLOCK_ORDER_RANDOM )  : // 
         fprintf ( stdout, " Random block decomposition strategy \n" );
         break;

      case ( MG_BLOCK_ORDER_COMM_FIRST_RAND )  : // 
         fprintf ( stdout, " Comm-first random block decomposition strategy \n" );
         break;

      case ( MG_BLOCK_ORDER_TDAG ) : // Task DAG.
         fprintf ( stdout, " Task DAG block decomposition strategy \n" );
         break;

      case ( MG_BLOCK_ORDER_WTDAG ) : // Weighted task DAG.
         fprintf ( stdout, " Weighted task DAG block decomposition strategy \n" );
         break;
   }
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Number of variables = %d \n", params.numvars );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "    Extra work: %d variables operated on %d additional variables. \n",
                       params.nvars_with_extra_work, params.extra_work_nvars );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Number of time steps = %d \n", params.numtsteps );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Number of spikes inserted = %d \n", params.numspikes );
   fprintf ( stdout, "\n" );

   iter_err_min = params.iter_error[0];
   iter_err_max = params.iter_error[0];
   iter_err_sum = params.iter_error[0];

   for ( ivar=0; ivar<params.numvars; ivar++ ) {
      iter_err_min = MG_Min ( iter_err_min, params.iter_error[ivar] );
      iter_err_max = MG_Max ( iter_err_max, params.iter_error[ivar] );
      iter_err_sum += params.iter_error[ivar];
   }
   iter_err_avg = iter_err_sum / params.numvars;

   fprintf ( stdout, " Accuracy within requested tolerance of %4.2e. \n", params.error_tol );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "    average %4.2e (min, max = %4.2e, %4.2e) \n", 
             iter_err_avg, iter_err_min, iter_err_max );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, " Number of stencil floating point operations per time step per variable: %2.3e \n", 
             (float)num_flops_stencil_tstep_pe / (float)params.numvars );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Total time: %2.3e \n", times_all[MAX] );
   fprintf ( stdout, "\n" );

#if defined _MG_MPI // {
#if defined _MG_TIMING_L0 // {
   fprintf ( stdout, " (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L1). \n" );
   fprintf ( stdout, "\n" );
#elif !defined _MG_TIMING_L1 
   fprintf ( stdout, " (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L0) \n        and more with _MG_TIMING_L1.) \n" );
   fprintf ( stdout, "\n" );
#endif // }
#elif !defined _MG_TIMING_L0
   fprintf ( stdout, " (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L0). \n" );
   fprintf ( stdout, "\n" );
#endif // _MG_MPI }

#if defined _MG_TIMING_L0 || _MG_TIMING_L1 // {
   if ( params.check_answer_freq > params.numtsteps ) {
      time_comp  = times_stencil[AVG] + time_bc_total; // We collect the grid sum time, but it will show up as "other".
   }
   else {
      time_comp  = times_stencil[AVG] + time_bc_total + times_sum_grid[AVG];
   }
   fprintf ( stdout, "    Computation time: %2.3e (%2.1f%%) \n", 
             time_comp, time_comp / times_all[MAX] * 100.0 );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, "       Stencil time:   %2.3e (%2.1f%%) \n", 
             times_stencil[AVG], times_stencil[AVG] / time_comp * 100.0 );

   time_bc_total = times_bc_comp[AVG] + times_bc_accum[AVG];

   fprintf ( stdout, "       BC time:        %2.3e (%2.1f%%) \n", 
             time_bc_total, time_bc_total / time_comp * 100.0 );
#if defined _MG_TIMING_L1 // {
   fprintf ( stdout, "          BC task time:  %2.3e (%2.1f%%) \n",
             times_bc_comp[AVG], times_bc_comp[AVG] / time_bc_total * 100.0 );
   fprintf ( stdout, "          BC accum time: %2.3e (%2.1f%%) \n",
             times_bc_accum[AVG], times_bc_accum[AVG] / time_bc_total * 100.0 );
#endif // }
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( stdout, "       Sum grid time:  %2.3e (%2.1f%%) \n",
                times_sum_grid[AVG], times_sum_grid[AVG] / time_comp * 100.0 );
   }
#endif // }

#if defined _MG_MPI // {
   if ( params.check_answer_freq <= params.numtsteps ) {
      time_comm = times_bex[AVG] + times_sum_grid_comm[AVG];
   }
   else {
      time_comm = times_bex[AVG];
   }
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "    Communication time: %2.3e (%2.1f%%) \n",
             time_comm, time_comm / times_all[MAX] * 100.0 );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "       Halo exchange time:            %2.3e (%2.1f%%)\n",
             times_bex[AVG], times_bex[AVG] / time_comm * 100.0 );
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( stdout, "       Sum grid (MPI_Allreduce) time: %2.3e (%2.1f%%)\n",
                times_sum_grid_comm[AVG], times_sum_grid_comm[AVG] / time_comm * 100.0 );
   }
#if defined _MG_TIMING_L1 // {

   time_pack = times_pack_north[AVG] + times_pack_south[AVG] +
               times_pack_east[AVG]  + times_pack_west[AVG]  +
               times_pack_front[AVG] + times_pack_back[AVG];

   fprintf ( stdout, "\n" );
   fprintf ( stdout, "          Pack time: %2.3e (%2.1f%%)\n",
             time_pack, time_pack / times_bex[AVG] * 100.0 );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, "             pNORTH:   %2.3e (%2.1f%%)\n", 
             times_pack_north[AVG], times_pack_north[AVG] / time_pack * 100.0 );
   fprintf ( stdout, "             pSOUTH:   %2.3e (%2.1f%%)\n",  
             times_pack_south[AVG], times_pack_south[AVG] / time_pack * 100.0 );
   fprintf ( stdout, "             pEAST:    %2.3e (%2.1f%%)\n",
             times_pack_east[AVG], times_pack_east[AVG]   / time_pack * 100.0 );
   fprintf ( stdout, "             pWEST:    %2.3e (%2.1f%%)\n",
             times_pack_west[AVG], times_pack_west[AVG]   / time_pack * 100.0 );
   fprintf ( stdout, "             pFRONT:   %2.3e (%2.1f%%)\n",
             times_pack_front[AVG], times_pack_front[AVG] / time_pack * 100.0 );
   fprintf ( stdout, "             pBACK:    %2.3e (%2.1f%%)\n",
             times_pack_back[AVG], times_pack_back[AVG]   / time_pack * 100.0 );

   switch ( params.comm_strategy ) {

      case MG_COMM_STRATEGY_SR:

         time_send = times_send_north[AVG] + times_send_south[AVG] +
                     times_send_east[AVG]  + times_send_west[AVG]  +
                     times_send_front[AVG] + times_send_back[AVG];

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send time: %2.3e (%2.1f%%)\n",
                   time_send, time_send / times_bex[AVG] * 100.0 );

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "             sNORTH:   %2.3e (%2.1f%%)\n",
                   times_send_north[AVG], times_send_north[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sSOUTH:   %2.3e (%2.1f%%)\n",
                   times_send_south[AVG], times_send_south[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sEAST:    %2.3e (%2.1f%%)\n",
                   times_send_east[AVG], times_send_east[AVG]   / time_send * 100.0 );
         fprintf ( stdout, "             sWEST:    %2.3e (%2.1f%%)\n",
                   times_send_west[AVG], times_send_west[AVG]   / time_send * 100.0 );
         fprintf ( stdout, "             sFRONT:   %2.3e (%2.1f%%)\n",
                   times_send_front[AVG], times_send_front[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sBACK:    %2.3e (%2.1f%%)\n",
                   times_send_back[AVG], times_send_back[AVG]   / time_send * 100.0 );

         time_recv = times_recv_north[AVG] + times_recv_south[AVG] +
                     times_recv_east[AVG]  + times_recv_west[AVG]  +
                     times_recv_front[AVG] + times_recv_back[AVG];

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Recv time: %2.3e (%2.1f%%)\n",
                   time_recv, time_recv / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );

         fprintf ( stdout, "             rNORTH:   %2.3e (%2.1f%%)\n",
                   times_recv_north[AVG], times_recv_north[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rSOUTH:   %2.3e (%2.1f%%)\n",
                   times_recv_south[AVG], times_recv_south[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rEAST:    %2.3e (%2.1f%%)\n",
                   times_recv_east[AVG], times_recv_east[AVG]   / time_recv * 100.0 );
         fprintf ( stdout, "             rWEST:    %2.3e (%2.1f%%)\n",
                   times_recv_west[AVG], times_recv_west[AVG]   / time_recv * 100.0 );
         fprintf ( stdout, "             rFRONT:   %2.3e (%2.1f%%)\n",
                   times_recv_front[AVG], times_recv_front[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rBACK:    %2.3e (%2.1f%%)\n",
                   times_recv_back[AVG], times_recv_back[AVG]   / time_recv * 100.0 );

         break;

      case MG_COMM_STRATEGY_ISR:

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send time: %2.3e (%2.1f%%)\n",
                   times_send[AVG], times_send[AVG] / times_bex[AVG] * 100.0 );

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send wait time: %2.3e (%2.1f%%)\n",
                   times_wait_send[AVG], times_wait_send[AVG] / times_bex[AVG] * 100.0 );

         time_recv = times_recv_north[AVG] + times_recv_south[AVG] +
                     times_recv_east[AVG]  + times_recv_west[AVG]  +
                     times_recv_front[AVG] + times_recv_back[AVG];

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Recv time: %2.3e (%2.1f%%)\n",
                   time_recv, time_recv / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );

         fprintf ( stdout, "             rNORTH:   %2.3e (%2.1f%%)\n",
                   times_recv_north[AVG], times_recv_north[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rSOUTH:   %2.3e (%2.1f%%)\n",
                   times_recv_south[AVG], times_recv_south[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rEAST:    %2.3e (%2.1f%%)\n",
                   times_recv_east[AVG], times_recv_east[AVG]   / time_recv * 100.0 );
         fprintf ( stdout, "             rWEST:    %2.3e (%2.1f%%)\n",
                   times_recv_west[AVG], times_recv_west[AVG]   / time_recv * 100.0 );
         fprintf ( stdout, "             rFRONT:   %2.3e (%2.1f%%)\n",
                   times_recv_front[AVG], times_recv_front[AVG] / time_recv * 100.0 );
         fprintf ( stdout, "             rBACK:    %2.3e (%2.1f%%)\n",
                   times_recv_back[AVG], times_recv_back[AVG]   / time_recv * 100.0 );

         break;

      case MG_COMM_STRATEGY_SIR:

         time_send = times_send_north[AVG] + times_send_south[AVG] +
                     times_send_east[AVG]  + times_send_west[AVG]  +
                     times_send_front[AVG] + times_send_back[AVG];

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send time: %2.3e (%2.1f%%)\n",
                   time_send, time_send / times_bex[AVG] * 100.0 );

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "             sNORTH:   %2.3e (%2.1f%%)\n",
                   times_send_north[AVG], times_send_north[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sSOUTH:   %2.3e (%2.1f%%)\n",
                   times_send_south[AVG], times_send_south[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sEAST:    %2.3e (%2.1f%%)\n",
                   times_send_east[AVG], times_send_east[AVG]   / time_send * 100.0 );
         fprintf ( stdout, "             sWEST:    %2.3e (%2.1f%%)\n",
                   times_send_west[AVG], times_send_west[AVG]   / time_send * 100.0 );
         fprintf ( stdout, "             sFRONT:   %2.3e (%2.1f%%)\n",
                   times_send_front[AVG], times_send_front[AVG] / time_send * 100.0 );
         fprintf ( stdout, "             sBACK:    %2.3e (%2.1f%%)\n",
                   times_send_back[AVG], times_send_back[AVG]   / time_send * 100.0 );

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Recv time: %2.3e (%2.1f%%)\n",
                   times_recv[AVG], times_recv[AVG] / times_bex[AVG] * 100.0 );

         time_wait_recv = times_wait_recv_north[AVG] + times_wait_recv_south[AVG] +
                          times_wait_recv_east[AVG]  + times_wait_recv_west[AVG]  +
                          times_wait_recv_front[AVG] + times_wait_recv_back[AVG];
      
         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Receive wait time: %2.3e (%2.1f%%)\n",
                   time_wait_recv, time_wait_recv / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );
         fprintf ( stdout, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_north[AVG], times_wait_recv_north[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_south[AVG], times_wait_recv_south[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_east[AVG], times_wait_recv_east[AVG]   / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_west[AVG], times_wait_recv_west[AVG]   / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_front[AVG], times_wait_recv_front[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wBACK:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_back[AVG], times_wait_recv_back[AVG]   / time_wait_recv * 100.0 );
         fprintf ( stdout, "\n" );

         break;

      case MG_COMM_STRATEGY_ISIR:

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send time: %2.3e (%2.1f%%)\n",
                   times_send[AVG], times_send[AVG] / times_bex[AVG] * 100.0 );

         time_wait_send = times_wait_send_north[AVG] + times_wait_send_south[AVG] +
                          times_wait_send_east[AVG]  + times_wait_send_west[AVG]  +
                          times_wait_send_front[AVG] + times_wait_send_back[AVG];

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Send wait time: %2.3e (%2.1f%%)\n",
                   time_wait_send, time_wait_send / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );
         fprintf ( stdout, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_send_north[AVG], times_wait_send_north[AVG] / time_wait_send * 100.0 );
         fprintf ( stdout, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_send_south[AVG], times_wait_send_south[AVG] / time_wait_send * 100.0 );
         fprintf ( stdout, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_send_east[AVG], times_wait_send_east[AVG]   / time_wait_send * 100.0 );
         fprintf ( stdout, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_send_west[AVG], times_wait_send_west[AVG]   / time_wait_send * 100.0 );
         fprintf ( stdout, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_send_front[AVG], times_wait_send_front[AVG] / time_wait_send * 100.0 );
         fprintf ( stdout, "             wBACK:    %2.3e (%2.1f%%)\n",
                   times_wait_send_back[AVG], times_wait_send_back[AVG]   / time_wait_send * 100.0 );

         fprintf ( stdout, "\n" );
         fprintf ( stdout, "          Recv time: %2.3e (%2.1f%%)\n",
                   times_recv[AVG], times_recv[AVG] / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );

         time_wait_recv = times_wait_recv_north[AVG] + times_wait_recv_south[AVG] +
                          times_wait_recv_east[AVG]  + times_wait_recv_west[AVG]  +
                          times_wait_recv_front[AVG] + times_wait_recv_back[AVG];
      
         fprintf ( stdout, "          Receive wait time: %2.3e (%2.1f%%)\n",
                   time_wait_recv, time_wait_recv / times_bex[AVG] * 100.0 );
         fprintf ( stdout, "\n" );
         fprintf ( stdout, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_north[AVG], times_wait_recv_north[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_south[AVG], times_wait_recv_south[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_east[AVG], times_wait_recv_east[AVG]   / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_west[AVG], times_wait_recv_west[AVG]   / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_front[AVG], times_wait_recv_front[AVG] / time_wait_recv * 100.0 );
         fprintf ( stdout, "             wBACK:    %2.3e (%2.1f%%)\n",
                times_wait_recv_back[AVG], times_wait_recv_back[AVG]   / time_wait_recv * 100.0 );

         break;

   } // End switch ( params.comm_strategy )

   time_unpack = times_unpack_north[AVG] + times_unpack_south[AVG] +
                 times_unpack_east[AVG]  + times_unpack_west[AVG]  +
                 times_unpack_front[AVG] + times_unpack_back[AVG];

   fprintf ( stdout, "\n" );
   fprintf ( stdout, "          Unpack time: %2.3e (%2.1f%%)\n",
             time_unpack, time_unpack / times_bex[AVG] * 100.0 );
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "             uNORTH:   %2.3e (%2.1f%%)\n",
             times_unpack_north[AVG], times_unpack_north[AVG] / time_unpack * 100.0 );
   fprintf ( stdout, "             uSOUTH:   %2.3e (%2.1f%%)\n",
             times_unpack_south[AVG], times_unpack_south[AVG] / time_unpack * 100.0 );
   fprintf ( stdout, "             uEAST:    %2.3e (%2.1f%%)\n",
             times_unpack_east[AVG], times_unpack_east[AVG]   / time_unpack * 100.0 );
   fprintf ( stdout, "             uWEST:    %2.3e (%2.1f%%)\n",
             times_unpack_west[AVG], times_unpack_west[AVG]   / time_unpack * 100.0 );
   fprintf ( stdout, "             uFRONT:   %2.3e (%2.1f%%)\n",
             times_unpack_front[AVG], times_unpack_front[AVG] / time_unpack * 100.0 );
   fprintf ( stdout, "             uBACK:    %2.3e (%2.1f%%)\n",
             times_unpack_back[AVG], times_unpack_back[AVG]   / time_unpack * 100.0 );

#endif // _MG_TIMING_L1 }
#if defined _MG_MPI // {
   time_other = times_all[MAX] - time_comp - time_comm;
#elif defined _MG_SERIAL
   time_other = times_all[MAX] - time_comp;
#endif // }
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "    Other time: %2.3e (%2.1f%%) \n", time_other, time_other / times_all[MAX] * 100.0 );
#endif // _MG_MPI }

   gflops_pe = (MG_REAL)(num_flops_stencil_tstep_pe+num_flops_bc_tstep_pe+num_flops_sum_grid_tstep_pe) / times_all[MAX] / GIGA;

   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Computational performance GFLOPS: \n" );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, "    Total (GFLOPS):  %2.2e \n", gflops_pe * numpes );
#if defined _MG_MPI // {
   fprintf ( stdout, "    Per pe (GFLOPS): %2.2e \n", gflops_pe );
#endif // }
#if defined _MG_TIMING_L0 || _MG_TIMING_L1 // {
   fprintf ( stdout, "\n" );
   gflops_pe_stencil      = (MG_REAL)(num_flops_stencil_tstep_pe) / times_stencil[AVG] / GIGA;
   gflops_pe_bc           = (MG_REAL)(num_flops_bc_tstep_pe) / time_bc_total / GIGA;
#if defined _MG_MPI
   gflops_pe_sum_grid = (MG_REAL)(num_flops_sum_grid_tstep_pe) / ( times_sum_grid[AVG] - times_sum_grid_comm[AVG] ) / GIGA;
#else
   gflops_pe_sum_grid = (MG_REAL)(num_flops_sum_grid_tstep_pe) / times_sum_grid[AVG] / GIGA;
#endif
   fprintf ( stdout, "       Stencil GFLOPS, per pe (excluding comm):             %2.2e \n", gflops_pe_stencil );

   fprintf ( stdout, "       Boundary conditions GFLOPS, per pe (excluding comm): %2.2e \n", gflops_pe_bc );
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( stdout, "       Sum grid GFLOPS, per pe (excluding comm):            %2.2e \n", gflops_pe_sum_grid );

   }
   fprintf ( stdout, "\n       Number of stencil fp ops per iteration per pe: %4.2e \n", (float)num_flops_stencil_tstep_pe );
   if ( params.extra_work_nvars != 0 ) {
      fprintf ( stdout, "                                           Real work: %4.2e \n", (float)nfp_stencil_only_tstep_pe );
      fprintf ( stdout, "                               Artificial extra work: %4.2e \n", (float)nfp_stencil_extra_work_tstep_pe );
   }
   else {
      fprintf ( stdout, "          (No extra work.) \n" );
   }

#endif // }
#if defined _MG_OPENMP // {
   fprintf ( stdout, "\n" );
   fprintf ( stdout, "    Per thread GFLOPS: %2.2e \n", gflops_pe / (MG_REAL)omp_get_max_threads() );
#endif // }
   fprintf ( stdout, "\n" );

   fprintf ( stdout, " Memory allocated (GBytes): \n" );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, "    Total (GBytes):  %2.2e \n", mem_allocated * numpes );
#if defined _MG_MPI // {
   fprintf ( stdout, "    Per pe (GBytes): %2.2e \n", mem_allocated );
#endif // }

   fprintf ( stdout, "\n" );
   fprintf ( stdout, " ========================================================================= \n" );

   // Write to YAML format.

/* Stopped using this, but leaving it in place in case someone else wants it.
#if defined _MG_MPI && _MG_OPENMP // {
   sprintf( filename_yaml, "results.%s.MPI_OMP.%s.%dpes.%dthr.blk%dx%dx%d.dim%dx%dx%d.%dvars.yaml",
            stencil_s, comm_strategy_s, numpes, omp_get_max_threads(), 
            params.blkxlen, params.blkylen, params.blkzlen, 
            params.nx, params.ny, params.nz, params.numvars );
#elif defined _MG_MPI && _MG_OPENMP
   sprintf( filename_yaml, "results.%s.MPI_OMP.%s.%dpes.%dthr.blk%dx%dx%d.dim%dx%dx%d.%dvars.yaml",
            stencil_s, comm_strategy_s, numpes, omp_get_max_threads(), 
            params.blkxlen, params.blkylen, params.blkzlen, 
            params.nx, params.ny, params.nz, params.numvars );
#elif defined _MG_MPI
   sprintf( filename_yaml, "results.%s.MPI.%s.%dpes.blk%dx%dx%d.dim%dx%dx%d.%dvars.yaml",
            stencil_s, comm_strategy_s, numpes, 
            params.blkxlen, params.blkylen, params.blkzlen, 
            params.nx, params.ny, params.nz, params.numvars );
#elif defined _MG_OPENMP
   sprintf( filename_yaml, "results.%s.OMP.%dthr.blk%dx%dx%d.dim%dx%dx%d.%dvars.yaml",
            stencil_s, omp_get_max_threads(), 
            params.blkxlen, params.blkylen, params.blkzlen, 
            params.nx, params.ny, params.nz, params.numvars );
#elif defined _MG_SERIAL
   sprintf( filename_yaml, "results.serial.%s.blk%dx%dx%d.dim%dx%dx%d.%dvars.yaml",
            stencil_s, 
            params.blkxlen, params.blkylen, params.blkzlen, 
            params.nx, params.ny, params.nz, params.numvars );
#endif // }

   FILE *fp = fopen ( filename_yaml, "w+" );

   MG_Env ( fp );

   fprintf ( fp, "Mini-Application Name: miniGhost \n" );
   fprintf ( fp, "Mini-Application Version: 1.1 \n" );

#if defined _MG_SERIAL // {
   fprintf ( fp, "  task parallel (Serial) version. \n" );
#elif defined _MG_MPI && _MG_OPENMP
   fprintf ( fp, "  task parallel (MPI+OpenMP) version \n" );
   if ( num_threads == 1 ) {
      fprintf ( fp, " 1 OpenMP thread on each of %d MPI processes ", numpes );
   }
   else {
      fprintf ( fp, "  %d OpenMP threads on each of %d MPI processes ", num_threads, numpes );
   }
#elif defined _MG_MPIQ && defined _MG_MPI
   fprintf ( fp, " Mantevo miniGhost, task parallel (MPI+Qthreads) version, %d MPI processes\n", numpes );
   fprintf ( fp, "\n" );
#elif defined _MG_MPI && _MG_OPENMP
   fprintf ( fp, "  task parallel (MPI+OpenMP) version \n" );
   if ( num_threads == 1 ) {
      fprintf ( fp, " 1 OpenMP thread on each of %d MPI processes ", numpes );
   }
   else {
      fprintf ( fp, "  %d OpenMP threads on each of %d MPI processes ", num_threads, numpes );
   }
#elif defined _MG_MPI && _MG_QT
   fprintf ( fp, " Mantevo miniGhost, task parallel (MPI+qthread) version \n" );
   fprintf ( fp, "\n" );
   if ( numpes == 1 ) {
      fprintf ( fp, " %d MPI process \n", numpes );
   }
   else {
      fprintf ( fp, " %d MPI processes \n", numpes );
   }
   fprintf ( fp, " %d qthreads shepherds \n", (int)num_qtsheps );
   fprintf ( fp, " %d qthreads workers   \n", (int)num_qtworkers );
   fprintf ( fp, " qthreads  stack size %d \n", (int)qtstack_size );
#elif defined _MG_MPI
   fprintf ( fp, "  task parallel (MPI) version, " );
   if ( numpes == 1 ) {
      fprintf ( fp, "1 MPI process\n" );
   }
   else {
      fprintf ( fp, "%d MPI processes\n", numpes );
   }
#elif defined _MG_OPENMP
   fprintf ( fp, "  task parallel (OpenMP) version, " );
   if ( num_threads == 1 ) {
      fprintf ( fp, "1 OpenMP thread." );
   }
   else {
      fprintf ( fp, "%d OpenMP threads.", num_threads );
   }
#else
  // FIXME: need to clean up logic to match the actual configuration (DTS)
  //MG_Report_peformance: Communication protocol not defined. 
#endif // }

   fprintf ( fp, "Global Run Parameters: \n" );
   fprintf ( fp, "  stencil: ");
   switch ( params.stencil ) {
      case ( MG_STENCIL_2D5PT  ) :
         fprintf ( fp, "5pt2d \n" );
         break;
      case ( MG_STENCIL_2D9PT  ) :
         fprintf ( fp, "9pt2d \n" );
         break;
      case ( MG_STENCIL_3D7PT  ) :
         fprintf ( fp, "7pt3d \n" );
         break;
      case ( MG_STENCIL_3D27PT ) :
         fprintf ( fp, "27pt3d \n" );
         break;
      default:
         break;
   } // End switch ( params.stencil )

   fprintf ( fp, "  dimensions: \n" );
   fprintf ( fp, "    nx: %d \n", params.nx*params.npx );
   fprintf ( fp, "    ny: %d \n", params.ny*params.npy );
   fprintf ( fp, "    nz: %d \n", params.nz*params.npz );
#if defined _MG_MPI // {
   fprintf ( fp, "      lnx: %d \n", params.nx );
   fprintf ( fp, "      lny: %d \n", params.ny );
   fprintf ( fp, "      lnz: %d \n", params.nz );
#endif // _MG_MPI }


   fprintf ( fp, "  Block decomp: " );
   switch ( params.blkorder ) {

      case ( MG_BLOCK_ORDER_CART ) : // This is the default ordering.
         fprintf ( fp, "Cartesian\n" );
         break;

      case ( MG_BLOCK_ORDER_MTA )  : // 
         fprintf ( fp, "MTA_style\n" );
         break;

      case ( MG_BLOCK_ORDER_RANDOM )  : // 
         fprintf ( fp, "Random\n" );
         break;

      case ( MG_BLOCK_ORDER_COMM_FIRST_RAND )  : // 
         fprintf ( fp, "Comm_first_random\n" );
         break;

      case ( MG_BLOCK_ORDER_TDAG ) : // Task DAG.
         fprintf ( fp, "Task_DAG\n" );
         break;

      case ( MG_BLOCK_ORDER_WTDAG ) : // Weighted task DAG.
         fprintf ( fp, "Weighted_task_DAG\n" );
         break;
   }
   fprintf ( fp, "    bnx: %d \n", params.blkxlen );
   fprintf ( fp, "    bny: %d \n", params.blkylen );
   fprintf ( fp, "    bnz: %d \n", params.blkzlen );


   fprintf ( fp, "  numTsteps: %d \n", params.numtsteps );
   fprintf ( fp, "  numVars: %d \n", params.numvars );

   fprintf ( fp, "  ErrTol: %4.2e \n", params.error_tol );

   fprintf ( fp, "  FLOPS/tstep/var: %2.3e \n", 
             (float)num_flops_stencil_tstep_pe / (float)params.numvars );

   fprintf ( fp, "Performance:\n" );
#if defined _MG_MPI // {
#if defined _MG_TIMING_L0 // {
   fprintf ( fp, "    (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L1). \n" );
#elif !defined _MG_TIMING_L1 
   fprintf ( fp, "    (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L0) \n        and more with _MG_TIMING_L1.) \n" );
#endif // _MG_TIMING_L0 // }
#elif !defined _MG_TIMING_L0
   fprintf ( fp, "    (Note: finer granularity of timing available if compiled with -D_MG_TIMING_L0). \n" );
#endif // }

   fprintf ( fp, "  Total time: %2.3e \n", times_all[MAX] );

#if defined _MG_TIMING_L0 || _MG_TIMING_L1 // {
   fprintf ( fp, "    Comp time: %2.3e \n", time_comp );
   fprintf ( fp, "      Comp time %% total: %2.1f \n", time_comp/times_all[MAX] * 100.0 );
   fprintf ( fp, "      Stencil time: %2.3e \n", times_stencil[AVG] );
   fprintf ( fp, "        Stencil time %% comp: %2.1f \n", times_stencil[AVG]/time_comp * 100.0 );
   fprintf ( fp, "      BC time: %2.3e \n", time_bc_total );
   fprintf ( fp, "        BC time %% comp: %2.1f \n", time_bc_total / time_comp*100.0 );
#if defined _MG_TIMING_L1 // {
   fprintf ( fp, "          BC task time:  %2.3e (%2.1f%%) \n",
             times_bc_comp[AVG], times_bc_comp[AVG] / time_bc_total * 100.0 );
   fprintf ( fp, "          BC accum time: %2.3e (%2.1f%%) \n",
             times_bc_accum[AVG], times_bc_accum[AVG] / time_bc_total * 100.0 );
#endif // }
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( fp, "       Sum grid time:  %2.3e (%2.1f%%) \n",
                times_sum_grid[AVG], times_sum_grid[AVG] / time_comp * 100.0 );
   }
#endif // }

#if defined _MG_MPI // {
#if defined _MG_TIMING_L0 // {
   fprintf ( fp, "    Halo exchange time:            %2.3e\n", times_bex[AVG] );
   fprintf ( fp, "      Halo exchange time %% total: %2.1f \n", times_bex[AVG]/times_all[MAX] * 100.0 );
   fprintf ( fp, "\n" );
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( fp, "    Sum grid (MPI_Allreduce) time: %2.3e (%2.1f%%)\n",
                times_sum_grid_comm[AVG], times_sum_grid_comm[AVG] / times_all[MAX] * 100.0 );
   }
#endif // _MG_TIMING_L0 // }
#if defined _MG_TIMING_L1 // {
   fprintf ( fp, "    Comm time: %2.3e \n", time_comm );
   fprintf ( fp, "      Comm time %% total: %2.1f \n", time_comm/times_all[MAX] * 100.0 );

   fprintf ( fp, "      Halo exchange time:  %2.3e\n", times_bex[AVG] );
   fprintf ( fp, "        Halo exchange time %% total: %2.1f \n", times_bex[AVG]/times_all[MAX] * 100.0 );

   fprintf ( fp, "\n" );
   fprintf ( fp, "          Pack time: %2.3e (%2.1f%%)\n",
             time_pack, time_pack / times_bex[AVG] * 100.0 );
   fprintf ( fp, "\n" );

   fprintf ( fp, "             pNORTH:   %2.3e (%2.1f%%)\n",
             times_pack_north[AVG], times_pack_north[AVG] / time_pack * 100.0 );
   fprintf ( fp, "             pSOUTH:   %2.3e (%2.1f%%)\n",
             times_pack_south[AVG], times_pack_south[AVG] / time_pack * 100.0 );
   fprintf ( fp, "             pEAST:    %2.3e (%2.1f%%)\n",
             times_pack_east[AVG], times_pack_east[AVG]   / time_pack * 100.0 );
   fprintf ( fp, "             pWEST:    %2.3e (%2.1f%%)\n",
             times_pack_west[AVG], times_pack_west[AVG]   / time_pack * 100.0 );
   fprintf ( fp, "             pFRONT:   %2.3e (%2.1f%%)\n",
             times_pack_front[AVG], times_pack_front[AVG] / time_pack * 100.0 );
   fprintf ( fp, "             pBACK:    %2.3e (%2.1f%%)\n",
             times_pack_back[AVG], times_pack_back[AVG]   / time_pack * 100.0 );

   switch ( params.comm_strategy ) {

      case MG_COMM_STRATEGY_SR:

         time_send = times_send_north[AVG] + times_send_south[AVG] +
                     times_send_east[AVG]  + times_send_west[AVG]  +
                     times_send_front[AVG] + times_send_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send time: %2.3e (%2.1f%%)\n",
                   time_send, time_send / times_bex[AVG] * 100.0 );

         fprintf ( fp, "\n" );
         fprintf ( fp, "             sNORTH:   %2.3e (%2.1f%%)\n",
                   times_send_north[AVG], times_send_north[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sSOUTH:   %2.3e (%2.1f%%)\n",
                   times_send_south[AVG], times_send_south[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sEAST:    %2.3e (%2.1f%%)\n",
                   times_send_east[AVG], times_send_east[AVG]   / time_send * 100.0 );
         fprintf ( fp, "             sWEST:    %2.3e (%2.1f%%)\n",
                   times_send_west[AVG], times_send_west[AVG]   / time_send * 100.0 );
         fprintf ( fp, "             sFRONT:   %2.3e (%2.1f%%)\n",
                   times_send_front[AVG], times_send_front[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sBACK:    %2.3e (%2.1f%%)\n",
                   times_send_back[AVG], times_send_back[AVG]   / time_send * 100.0 );

         time_recv = times_recv_north[AVG] + times_recv_south[AVG] +
                     times_recv_east[AVG]  + times_recv_west[AVG]  +
                     times_recv_front[AVG] + times_recv_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Recv time: %2.3e (%2.1f%%)\n",
                   time_recv, time_recv / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );

         fprintf ( fp, "             rNORTH:   %2.3e (%2.1f%%)\n",
                   times_recv_north[AVG], times_recv_north[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rSOUTH:   %2.3e (%2.1f%%)\n",
                   times_recv_south[AVG], times_recv_south[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rEAST:    %2.3e (%2.1f%%)\n",
                   times_recv_east[AVG], times_recv_east[AVG]   / time_recv * 100.0 );
         fprintf ( fp, "             rWEST:    %2.3e (%2.1f%%)\n",
                   times_recv_west[AVG], times_recv_west[AVG]   / time_recv * 100.0 );
         fprintf ( fp, "             rFRONT:   %2.3e (%2.1f%%)\n",
                   times_recv_front[AVG], times_recv_front[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rBACK:    %2.3e (%2.1f%%)\n",
                   times_recv_back[AVG], times_recv_back[AVG]   / time_recv * 100.0 );

         break;

      case MG_COMM_STRATEGY_ISR:

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send time: %2.3e (%2.1f%%)\n",
                   times_send[AVG], times_send[AVG] / times_bex[AVG] * 100.0 );

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send wait time: %2.3e (%2.1f%%)\n",
                   times_wait_send[AVG], times_wait_send[AVG] / times_bex[AVG] * 100.0 );

         time_recv = times_recv_north[AVG] + times_recv_south[AVG] +
                     times_recv_east[AVG]  + times_recv_west[AVG]  +
                     times_recv_front[AVG] + times_recv_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Recv time: %2.3e (%2.1f%%)\n",
                   time_recv, time_recv / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );

         fprintf ( fp, "             rNORTH:   %2.3e (%2.1f%%)\n",
                   times_recv_north[AVG], times_recv_north[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rSOUTH:   %2.3e (%2.1f%%)\n",
                   times_recv_south[AVG], times_recv_south[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rEAST:    %2.3e (%2.1f%%)\n",
                   times_recv_east[AVG], times_recv_east[AVG]   / time_recv * 100.0 );
         fprintf ( fp, "             rWEST:    %2.3e (%2.1f%%)\n",
                   times_recv_west[AVG], times_recv_west[AVG]   / time_recv * 100.0 );
         fprintf ( fp, "             rFRONT:   %2.3e (%2.1f%%)\n",
                   times_recv_front[AVG], times_recv_front[AVG] / time_recv * 100.0 );
         fprintf ( fp, "             rBACK:    %2.3e (%2.1f%%)\n",
                   times_recv_back[AVG], times_recv_back[AVG]   / time_recv * 100.0 );

         break;

      case MG_COMM_STRATEGY_SIR:

         time_send = times_send_north[AVG] + times_send_south[AVG] +
                     times_send_east[AVG]  + times_send_west[AVG]  +
                     times_send_front[AVG] + times_send_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send time: %2.3e (%2.1f%%)\n",
                   time_send, time_send / times_bex[AVG] * 100.0 );

         fprintf ( fp, "\n" );
         fprintf ( fp, "             sNORTH:   %2.3e (%2.1f%%)\n",
                   times_send_north[AVG], times_send_north[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sSOUTH:   %2.3e (%2.1f%%)\n",
                   times_send_south[AVG], times_send_south[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sEAST:    %2.3e (%2.1f%%)\n",
                   times_send_east[AVG], times_send_east[AVG]   / time_send * 100.0 );
         fprintf ( fp, "             sWEST:    %2.3e (%2.1f%%)\n",
                   times_send_west[AVG], times_send_west[AVG]   / time_send * 100.0 );
         fprintf ( fp, "             sFRONT:   %2.3e (%2.1f%%)\n",
                   times_send_front[AVG], times_send_front[AVG] / time_send * 100.0 );
         fprintf ( fp, "             sBACK:    %2.3e (%2.1f%%)\n",
                   times_send_back[AVG], times_send_back[AVG]   / time_send * 100.0 );

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Recv time: %2.3e (%2.1f%%)\n",
                   times_recv[AVG], times_recv[AVG] / times_bex[AVG] * 100.0 );

         time_wait_recv = times_wait_recv_north[AVG] + times_wait_recv_south[AVG] +
                          times_wait_recv_east[AVG]  + times_wait_recv_west[AVG]  +
                          times_wait_recv_front[AVG] + times_wait_recv_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Receive wait time: %2.3e (%2.1f%%)\n",
                   time_wait_recv, time_wait_recv / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );
         fprintf ( fp, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_north[AVG], times_wait_recv_north[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_south[AVG], times_wait_recv_south[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_east[AVG], times_wait_recv_east[AVG]   / time_wait_recv * 100.0 );
         fprintf ( fp, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_west[AVG], times_wait_recv_west[AVG]   / time_wait_recv * 100.0 );
         fprintf ( fp, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_front[AVG], times_wait_recv_front[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wBACK:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_back[AVG], times_wait_recv_back[AVG]   / time_wait_recv * 100.0 );
         fprintf ( fp, "\n" );

         break;

      case MG_COMM_STRATEGY_ISIR:

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send time: %2.3e (%2.1f%%)\n",
                   times_send[AVG], times_send[AVG] / times_bex[AVG] * 100.0 );

         time_wait_send = times_wait_send_north[AVG] + times_wait_send_south[AVG] +
                          times_wait_send_east[AVG]  + times_wait_send_west[AVG]  +
                          times_wait_send_front[AVG] + times_wait_send_back[AVG];

         fprintf ( fp, "\n" );
         fprintf ( fp, "          Send wait time: %2.3e (%2.1f%%)\n",
                   time_wait_send, time_wait_send / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );
         fprintf ( fp, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_send_north[AVG], times_wait_send_north[AVG] / time_wait_send * 100.0 );
         fprintf ( fp, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_send_south[AVG], times_wait_send_south[AVG] / time_wait_send * 100.0 );
         fprintf ( fp, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_send_east[AVG], times_wait_send_east[AVG]   / time_wait_send * 100.0 );
         fprintf ( fp, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_send_west[AVG], times_wait_send_west[AVG]   / time_wait_send * 100.0 );
         fprintf ( fp, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_send_front[AVG], times_wait_send_front[AVG] / time_wait_send * 100.0 );
         fprintf ( fp, "             wBACK:    %2.3e (%2.1f%%)\n",
                   times_wait_send_back[AVG], times_wait_send_back[AVG]   / time_wait_send * 100.0 );

         fprintf ( fp, "          Recv time: %2.3e (%2.1f%%)\n",
                   times_recv[AVG], times_recv[AVG] / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );

         time_wait_recv = times_wait_recv_north[AVG] + times_wait_recv_south[AVG] +
                          times_wait_recv_east[AVG]  + times_wait_recv_west[AVG]  +
                          times_wait_recv_front[AVG] + times_wait_recv_back[AVG];

         fprintf ( fp, "          Receive wait time: %2.3e (%2.1f%%)\n",
                   time_wait_recv, time_wait_recv / times_bex[AVG] * 100.0 );
         fprintf ( fp, "\n" );
         fprintf ( fp, "             wNORTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_north[AVG], times_wait_recv_north[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wSOUTH:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_south[AVG], times_wait_recv_south[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wEAST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_east[AVG], times_wait_recv_east[AVG]   / time_wait_recv * 100.0 );
         fprintf ( fp, "             wWEST:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_west[AVG], times_wait_recv_west[AVG]   / time_wait_recv * 100.0 );
         fprintf ( fp, "             wFRONT:   %2.3e (%2.1f%%)\n",
                   times_wait_recv_front[AVG], times_wait_recv_front[AVG] / time_wait_recv * 100.0 );
         fprintf ( fp, "             wBACK:    %2.3e (%2.1f%%)\n",
                   times_wait_recv_back[AVG], times_wait_recv_back[AVG]   / time_wait_recv * 100.0 );

         break;

   } // switch ( params.comm_strategy )

   time_unpack = times_unpack_north[AVG] + times_unpack_south[AVG] +
                 times_unpack_east[AVG]  + times_unpack_west[AVG]  +
                 times_unpack_front[AVG] + times_unpack_back[AVG];

   fprintf ( fp, "\n" );
   fprintf ( fp, "          Unpack time: %2.3e (%2.1f%%)\n",
             time_unpack, time_unpack / times_bex[AVG] * 100.0 );
   fprintf ( fp, "\n" );
   fprintf ( fp, "             uNORTH:   %2.3e (%2.1f%%)\n",
             times_unpack_north[AVG], times_unpack_north[AVG] / time_unpack * 100.0 );
   fprintf ( fp, "             uSOUTH:   %2.3e (%2.1f%%)\n",
             times_unpack_south[AVG], times_unpack_south[AVG] / time_unpack * 100.0 );
   fprintf ( fp, "             uEAST:    %2.3e (%2.1f%%)\n",
             times_unpack_east[AVG], times_unpack_east[AVG]   / time_unpack * 100.0 );
   fprintf ( fp, "             uWEST:    %2.3e (%2.1f%%)\n",
             times_unpack_west[AVG], times_unpack_west[AVG]   / time_unpack * 100.0 );
   fprintf ( fp, "             uFRONT:   %2.3e (%2.1f%%)\n",
             times_unpack_front[AVG], times_unpack_front[AVG] / time_unpack * 100.0 );
   fprintf ( fp, "             uBACK:    %2.3e (%2.1f%%)\n",
             times_unpack_back[AVG], times_unpack_back[AVG]   / time_unpack * 100.0 );

#endif // _MG_TIMING_L1 }
   fprintf ( fp, "    Other time: %2.3e \n", time_other );
   fprintf ( fp, "      Other time %% total: %2.1f \n", time_other/times_all[MAX] * 100.0 );
   fprintf ( fp, "\n" );
#endif // _MG_MPI }

   gflops_pe = (MG_REAL)(num_flops_stencil_tstep_pe+num_flops_bc_tstep_pe+num_flops_sum_grid_tstep_pe) / times_all[MAX] / GIGA;

   fprintf ( fp, "\n" );
   fprintf ( fp, " Computational performance GFLOPS: \n" );
   fprintf ( fp, "\n" );

   fprintf ( fp, "    Total (GFLOPS):      %2.2e \n", gflops_pe * numpes );
#if defined _MG_MPI // {
   fprintf ( fp, "    Per pe (GFLOPS):     %2.2e \n", gflops_pe );
#endif // }
#if defined _MG_TIMING_L0 || _MG_TIMING_L1 // {
   fprintf ( fp, "\n" );
   gflops_pe_stencil  = (MG_REAL)(num_flops_stencil_tstep_pe) / times_stencil[AVG] / GIGA;
   gflops_pe_bc       = (MG_REAL)(num_flops_bc_tstep_pe) / time_bc_total / GIGA;
   gflops_pe_sum_grid = (MG_REAL)(num_flops_sum_grid_tstep_pe) / times_sum_grid[AVG] / GIGA;

   fprintf ( fp, "       Stencil GFLOPS, per pe (excluding comm):             %2.2e \n", gflops_pe_stencil );
   fprintf ( fp, "       Boundary conditions GFLOPS, per pe (excluding comm): %2.2e \n", gflops_pe_bc );
   if ( params.check_answer_freq <= params.numtsteps ) {
      fprintf ( fp, "       Sum grid GFLOPS, per pe (excluding comm):            %2.2e \n", gflops_pe_sum_grid );
   }
#endif // }
#if defined _MG_OPENMP // {
   fprintf ( fp, "\n" );
   fprintf ( fp, "    Per thread GFLOPS: %2.2e \n", gflops_pe / (MG_REAL)omp_get_max_threads() );
#endif // }
   fprintf ( fp, "\n" );

   fprintf ( fp, " Memory allocated (GBytes): \n" );
   fprintf ( fp, "\n" );

   fprintf ( fp, "    Total (GBytes):  %2.2e \n", mem_allocated * numpes );
#if defined _MG_MPI // {
   fprintf ( fp, "    Per pe (GBytes): %2.2e \n", mem_allocated );
#endif // }

   fclose ( fp ); // Close results.yaml file.

*/

   return ( ierr );

} // End MG_Report_performance

// ======================================================================================

double MG_Timer (void)
{
   // ---------------
   // Local Variables
   // ---------------

   struct timeval tp;

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_MPI // {
   return ( CALL_MPI_Wtime () );
#else
   gettimeofday( &tp, NULL );
   return ( ((double)(tp.tv_sec) + (double)(tp.tv_usec/1000000.0)) );
#endif // }
}

// ======================================================================================

int MG_Profile_stats ( double* times, double *time_stats, int flag )
{
   // Compute average, minimum, and maximum of the input times.

   // ---------------
   // Local Variables
   // ---------------

   int
      i,             // Counter.
      ierr = 0;      // Return status.

   double
      in,
      in_avg,
      in_max,
      in_min,
      in_sum,
      out_avg,
      out_max,
      out_min,
      out_sum,
      *out;

   // ---------------------
   // Executable Statements
   // ---------------------

   switch ( flag ) {

      case 0:    // Single thread execution only.

         in_avg = *times;
         in_min = *times;
         in_max = *times;

         break;

      case 1:   // One timing per thread.

         //printf ( "[pe %d] THREAD Input: (%2.3e, %2.3e, %2.3e, %2.3e) \n", mgpp.mype, times[0], times[1], times[2], times[3] );

         in_sum = times[0];
         in_min = times[0];
         in_max = times[0];
         for ( i=1; i<mgpp.num_threads; i++ ) {
            in_sum += times[i];
            if ( times[i] < in_min ) {
               in_min = times[i];
            }
            if ( times[i] > in_max ) {
               in_max = times[i];
            }
         }
         in_avg = in_sum / mgpp.num_threads;

         //printf ( "[pe %d] Rank input: avg, min, max = (%2.3e, %2.3e, %2.3e) \n", mgpp.mype, in_avg, in_min, in_max );

         break;

      default:

         ierr = -1;
         MG_Assert ( ierr == 0, "MG_Profile_stats:unknown flag)" );

   } // End switch ( flag )

#if defined _MG_MPI // {

   ierr = MPI_Reduce ( &in_avg, &out_sum, 1, MPI_DOUBLE, MPI_SUM, mgpp.rootpe, MPI_COMM_MG );
   MG_Assert ( ierr == MPI_SUCCESS, "MG_Profile_stats: CALL_MPI_Reduce ( in_avg )" );

   out_avg = out_sum / (double)mgpp.numpes;

   ierr = MPI_Reduce ( &in_min, &out_min, 1, MPI_DOUBLE, MPI_MIN, mgpp.rootpe, MPI_COMM_MG );
   MG_Assert ( ierr == MPI_SUCCESS, "MG_Profile_stats: CALL_MPI_Reduce ( in_min )" );

   ierr = MPI_Reduce ( &in_max, &out_max, 1, MPI_DOUBLE, MPI_MAX, mgpp.rootpe, MPI_COMM_MG );
   MG_Assert ( ierr == MPI_SUCCESS, "MG_Profile_stats: CALL_MPI_Reduce ( in_max )" );

#else

   out_avg = in_avg;
   out_min = in_min;
   out_max = in_max;         

#endif // }

   if ( mgpp.mype == mgpp.rootpe ) {

      //if ( flag == 1 ) printf ( "[pe %d] *** GLOBAL OUTPUT: avg, min, max = (%2.3e, %2.3e, %2.3e) \n", mgpp.mype, out_avg, out_min, out_max );

      time_stats[MIN] = out_min;
      time_stats[MAX] = out_max;
      time_stats[AVG] = out_avg;

   }
   return ( ierr );
}

//  ===================================================================================

MG_REAL MG_Compute_stddev ( MG_REAL *values, int numvals, MG_REAL mean ) 
{
//#include <math.h> 

   // ---------------
   // Local Variables
   // ---------------

   int  
      i;       // Counter

   MG_REAL  
      tmp, 
      mg_stddev;

   // ---------------------
   // Executable statements
   // ---------------------

   tmp = 0.0;
   for ( i=0; i<numvals; i++ )
      tmp += ( values[i] - mean )*( values[i] - mean );

   //mg_stddev = SquareRoot ( tmp / MG_REAL( numvals ));
   //printf ( "tmp, sqrt(tmp) = %2.3e, %2.3e *********** \n", tmp / MG_REAL( numvals ), mg_stddev );
   //mg_stddev = sqrt ( tmp / MG_REAL( numvals ));
   mg_stddev = -1.0;   // FIXME math.h is messed up on several platforms with different compilers. Why?

   return ( mg_stddev );

} // End MG_Compute_stddev

//  ===================================================================================

int MG_Max3i ( int a, int b, int c )
{
   // ---------------
   // Local Variables
   // ---------------

   int
      tmp,       // Temporary variable.
      max3i;     // Minimum values to be returned.

   // ---------------------
   // Executable statements
   // ---------------------

   tmp   = MG_Max ( a, b );
   max3i = MG_Max ( tmp, c );

   return ( max3i );
}

//  ===================================================================================

int MG_Min3i ( int a, int b, int c )
{
   // ---------------
   // Local Variables
   // ---------------

   int
      tmp,       // Temporary variable.
      min3i;     // Minimum values to be returned.

   // ---------------------
   // Executable statements
   // ---------------------

   tmp   = MG_Min ( a, b );
   min3i = MG_Min ( tmp, c );

   return ( min3i );
}

//  ===================================================================================

#if defined HAVE_GEMINI_COUNTERS // {

static gemini_state_t state;

int gpcd_start(void)
{
   int ierr = 0;

   gemini_init_state(MPI_COMM_WORLD, &state);
   gemini_read_counters(MPI_COMM_WORLD, &state);

   return ( ierr );
}


int gpcd_end(void)
{
   int ierr = 0;

   gemini_read_counters(MPI_COMM_WORLD, &state);
   gemini_print_counters(MPI_COMM_WORLD, &state, "miniGhost Gemini Performance Counters");

   return ( ierr );
}

#endif // }

//  ===================================================================================

