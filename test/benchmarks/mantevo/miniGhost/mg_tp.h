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

#ifndef _mg_tp_h_
#define _mg_tp_h_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if defined _MG_MPIF
#  if !defined(_MG_QT)
#  error MPIF requires Qthreads; rebuild with `-D_MG_QT`
#  endif
#  if !defined(_MG_MPI)
#  error MPIF requires MPI; rebuild with `-D_MG_MPI`
#  endif

#  define CALL_MPI_Init(argc, argv) MPIF_Init(argc, argv, &params)
#  define CALL_MPI_Finalize       MPI_Finalize
#  define CALL_MPI_Comm_dup       MPI_Comm_dup
#  define CALL_MPI_Errhandler_set MPI_Errhandler_set
#  define CALL_MPI_Comm_rank      MPI_Comm_rank
#  define CALL_MPI_Comm_size      MPI_Comm_size
#  define CALL_MPI_Comm_get_attr  MPI_Comm_get_attr
//#  define CALL_MPI_Barrier(comm)        MPIF_Barrier(params, comm)
#  define CALL_MPI_Barrier        MPIF_Barrier
#  define CALL_MPI_Bcast          MPI_Bcast
#  define CALL_MPI_Abort          MPI_Abort
#  define CALL_MPI_Isend          MPI_Isend
#  define CALL_MPI_Irecv          MPI_Irecv
#  define CALL_MPI_Send(buf,count,datatype,dest,tag,comm)           MPIF_Send(buf,count,datatype, dest, tag, comm, &params)
#  define CALL_MPI_Recv(buf, count, datatype, source, tag, comm, status) MPIF_Recv(buf, count, datatype, source, tag, comm, status, &params)
#  define CALL_MPI_Wait           MPI_Wait
#  define CALL_MPI_Waitany        MPI_Waitany
#  define CALL_MPI_Waitall        MPI_Waitall
#  define CALL_MPI_Allreduce      MPI_Allreduce
#  define CALL_MPI_Gather         MPI_Gather
#  define CALL_MPI_Wtime          MPI_Wtime

#elif defined _MG_MPIQ
#  if !defined(_MG_QT)
#  error MPIQ requires Qthreads; rebuild with `-D_MG_QT`
#  endif
#  if !defined(_MG_MPI)
#  error MPIQ requires MPI; rebuild with `-D_MG_MPI`
#  endif

#  define CALL_MPI_Init           MPIQ_Init
#  define CALL_MPI_Finalize       MPIQ_Finalize
#  define CALL_MPI_Comm_dup       MPIQ_Comm_dup
#  define CALL_MPI_Errhandler_set MPIQ_Errhandler_set
#  define CALL_MPI_Comm_rank      MPIQ_Comm_rank
#  define CALL_MPI_Comm_size      MPIQ_Comm_size
#  define CALL_MPI_Comm_get_attr  MPIQ_Comm_get_attr
#  define CALL_MPI_Barrier        MPIQ_Barrier
#  define CALL_MPI_Bcast          MPIQ_Bcast
#  define CALL_MPI_Abort          MPIQ_Abort
#  define CALL_MPI_Isend          MPIQ_Isend
#  define CALL_MPI_Irecv          MPIQ_Irecv
#  define CALL_MPI_Send           MPIQ_Send
#  define CALL_MPI_Recv           MPIQ_Recv
#  define CALL_MPI_Wait           MPIQ_Wait
#  define CALL_MPI_Waitany        MPIQ_Waitany
#  define CALL_MPI_Waitall        MPIQ_Waitall
#  define CALL_MPI_Allreduce      MPIQ_Allreduce
#  define CALL_MPI_Gather         MPIQ_Gather
#  define CALL_MPI_Wtime          MPI_Wtime
#elif defined _MG_MPI
#  define CALL_MPI_Init           MPI_Init
#  define CALL_MPI_Finalize       MPI_Finalize
#  define CALL_MPI_Comm_dup       MPI_Comm_dup
#  define CALL_MPI_Errhandler_set MPI_Errhandler_set
#  define CALL_MPI_Comm_rank      MPI_Comm_rank
#  define CALL_MPI_Comm_size      MPI_Comm_size
#  define CALL_MPI_Comm_get_attr  MPI_Comm_get_attr
#  define CALL_MPI_Barrier        MPI_Barrier
#  define CALL_MPI_Bcast          MPI_Bcast
#  define CALL_MPI_Abort          MPI_Abort
#  define CALL_MPI_Isend          MPI_Isend
#  define CALL_MPI_Irecv          MPI_Irecv
#  define CALL_MPI_Send           MPI_Send
#  define CALL_MPI_Recv           MPI_Recv
#  define CALL_MPI_Wait           MPI_Wait
#  define CALL_MPI_Waitany        MPI_Waitany
#  define CALL_MPI_Waitall        MPI_Waitall
#  define CALL_MPI_Allreduce      MPI_Allreduce
#  define CALL_MPI_Gather         MPI_Gather
#  define CALL_MPI_Wtime          MPI_Wtime
#endif

#if defined _MG_QT
#  include "qthread.h"
#  include "qthread/sinc.h"
#  define mg_get_os_thread_num() qthread_readstate(CURRENT_UNIQUE_WORKER)
#  define mg_get_num_os_threads() qthread_readstate(ACTIVE_WORKERS)

#elif defined _MG_OPENMP

#define mg_get_num_os_threads() omp_get_num_threads()
#define mg_get_os_thread_num()  omp_get_thread_num()

#else // For non-threaded versions, e.g. serial, MPI-only.

#  define mg_get_os_thread_num() 0
#  define mg_get_num_os_threads() 1
#endif /* defined _MG_QT */

#if defined _MG_MPI
#include "mpi.h"
#endif

#if defined _USE_PAT_API
#include <pat_api.h>
#endif
#if defined HAVE_GEMINI_COUNTERS
#include <gemini.h>
int gpcd_start(void);
int gpcd_end(void);
#endif

// main will manage the global variables.
#if !defined MG_EXTERN
#define MG_EXTERN extern
#else
#define MG_EXTERN_OFF
#endif

#define MG_Max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define MG_Min( a, b ) ( ((a) < (b)) ? (a) : (b) )

#if defined _MG_DOUBLE
#define MG_REAL double
#elif defined _MG_REAL
#define MG_REAL real
#else
Real data type not defined.
#endif // MG_REAL

#include "mg_perf.h"

#define GRID_INIT_VAL 0.0 

   MG_EXTERN int comm_protocol;

typedef struct {

   int
      mype,             // Process id, e.g. MPI_Comm_rank ( MPI_COMM_MG );
      numpes,           // Number of MPI ranks (MPI_Comm_size (MPI_COMM_MG ))
      num_threads,      // Number of OpenMP threads (requested at runtime).
      rootpe,           // "Master" process, in MPI_COMM_MG.
      thread_id;

} ParParams;

   MG_EXTERN ParParams
      mgpp;             // Parallel processing parameters.


#if defined _MG_MPI // Use MPI
#include <mpi.h>

   MG_EXTERN int
      max_msg_count;    // Used for allocating msg buffers in MG_Boundary_exchange.

   MG_EXTERN MPI_Comm
      MPI_COMM_MG;      // MPI_Comm_dup ( MPI_COMM_WORLD );

   MG_EXTERN int
      *neighbors;

#define MAX_NUM_NEIGHBORS 6
#else // Not MPI.
#define MAX_NUM_NEIGHBORS 1
#endif // End _MG_MPI

#if defined _MG_DOUBLE
#define MG_COMM_REAL MPI_DOUBLE
#elif defined _MG_REAL
#define MG_COMM_REAL MPI_FLOAT
#else
MPI data type not defined.
#endif // MG_COMM_REAL

#define FIVE          5.0   // For stencil computations.
#define SEVEN         7.0   // For stencil computations.
#define NINE          9.0   // For stencil computations.
#define TWENTY_SEVEN 27.0   // For stencil computations.

#define GIGA 1000000000.0

// For indexing convenience:
#define MG_ARRAY_SHAPE(i,j,k) ((params.nx+(2*params.ghostdepth))*(params.ny+(2*params.ghostdepth))*(k)) + ((params.nx+(2*params.ghostdepth))*(j)) + (i)

MG_EXTERN MG_REAL *values;
#define values(i,j,k)     values[MG_ARRAY_SHAPE(i,j,k)]

#define values1(i,j,k)    values1[MG_ARRAY_SHAPE(i,j,k)]
#define values2(i,j,k)    values2[MG_ARRAY_SHAPE(i,j,k)]

// Various ways the values arrays are named:
#define grid_in(i,j,k)    grid_in[MG_ARRAY_SHAPE(i,j,k)]
#define grid_out(i,j,k)   grid_out[MG_ARRAY_SHAPE(i,j,k)]
#define grid_vals(i,j,k)  grid_vals[MG_ARRAY_SHAPE(i,j,k)]

typedef struct {

   int
      *pe,         // Process location.
      *x, *y, *z;  // x, y, z coordinates.

   MG_REAL
      *heat;       // Value to be inserted in to pe pe[i] at (x[i],y[i],z[i])

} SpikeInfo;

MG_EXTERN SpikeInfo **spikes;

typedef struct {

   int
      toggle,          // 0:g.values1->g.values2, 1:g.values2->g.values1

      do_more_work_nvars,  // Number of extra variables to be operated on.
      *do_more_work_vars,  // Variables ids for extra work.

      op,              // Collective operation to be applied to grid var.
      check_answer;    // 0, 1.

   MG_REAL

      *thr_max,        // Maximum value for thread across local grid.
      *thr_min,        // Minimum value for thread across local grid.
      *thr_sum,        // Summation for thread across local grid.

      lmax,            // Maximum value across local grid.
      lmin,            // Minimum value across local grid.
      lsum,            // Summation across local grid.

      gmax,            // Maximum value across global grid.
      gmin,            // Minimum value across global grid.
      gsum,            // Summation across global grid.

      *thr_flux,       // Flux out of domain, ordered by threads. 
      lflux,           // Sum of *thr_flux for this MPI process.
      gflux,           // Sum of lflux across all parallel processes.
      source_total;    // Maintaining source inputs.

   MG_REAL             // Correct values alternate between these two, with other serving as workspace.
      *values1,        // Grid cell values; the "state".
      *values2;        // Grid cell values; the "state".

} StateVar;

MG_EXTERN StateVar **g; // One per variable.

typedef struct {

   int

      id,

      neighbors[MAX_NUM_NEIGHBORS],    // Neighbor list for this block.

      xstart,            // Starting and ending indices of the block.
      xend,
      ystart,
      yend,
      zstart,
      zend,

      mypx,              // Block x-position in local grid.
      mypy,              // Block y-position in local grid.
      mypz,              // Block z-position in local grid.

      info,              //  Conveys information regarding the workload of the block.
                         //  1: stencil computation, plus 0-6 physical boundary faces. 
                         //  2: 1, plus 1-6 neighbors for inter-process communication

      bc[19];            // Physical boundary condition flag.

}  BlockInfo; // Information describing a block of the domain space.

MG_EXTERN BlockInfo **blk;

typedef struct {

   int
      nx,                          // Local (to (MPI) process) x-dim
      ny,                          // Local (to (MPI) process) y-dim
      nz,                          // Local (to (MPI) process) z-dim

      comm_method,                 // Boundary exchange aggregation method.
      comm_strategy,               // MPI send/recv strategy.
      scaling,                     // Strong or weak.
      stencil,                     // Stencil to be applied

      init_grid_values,            // Grid initialization 
      boundary_condition,          // Boundary conditions to be applied.

      numvars,                     // Number of variables to be operated on.
      numspikes,                   // Number of heat spikes (sources) to be applied over the run.
      numtsteps,                   // Number of time steps per heat spike.

      error_tol_exp,               // Error tolerance: 10^{-error_tol_exp}
      available_param,             // Placeholder in case another input is desired. 
      npx,                         // Logical processor grid, x-dir
      npy,                         // Logical processor grid, y-dir
      npz,                         // Logical processor grid, z-dir

      ghostdepth,                  // Depth of the halo. Currently 1 (FIXME: rfbarre)

      percent_sum,                 // Percentage of variables for which answer is checked.

      extra_work_nvars,            // Number of variables of extra work.
      nvars_with_extra_work,       // Number of variables of extra work, if a variable is to
                                   // do extra work. Determined by setting extra_work_percent.
      extra_work_percent,          // Percentage of variables that will perform extra work.

      check_answer_freq,           // Check answer every check_answer_freq time steps. Reported (to stdout).

      debug_grid,                  // Inserts spike into 0-initialized grid. Correctness checked
                                   // every time step for every variable.

      num_neighs,                  // Number of parallel process (rank) neighbors.
      num_sum_grid,                // Number of variables globally summed each time step.

      mypx, mypy, mypz,            // Processor position in the processor grid.

      blkorder,                    // Block definition.

      nblks_xdir,                  // Total number of blocks in x direction.
      nblks_ydir,                  // Total number of blocks in y direction.
      nblks_zdir,                  // Total number of blocks in z direction.

      blkxlen,                     // Number of cells in x direction for each block.
      blkylen,                     // Number of cells in y direction for each block.
      blkzlen,                     // Number of cells in z direction for each block.

      *thread_offset_xy,  // Length of data parallel OpenMP offset into grid xy face.
      *thread_offset_xz,  // Length of data parallel OpenMP offset into grid xz face.
      *thread_offset_yz,  // Length of data parallel OpenMP offset into grid yz face.

      numblks,

      numblkbc[18];               // Number of blocks applying bc, per direction.

   MG_REAL
      error_tol,
      *iter_error;                // Iteration error for each variable.

} InputParams; // Problem parameters.

// Message tag offsets for a block:this is a different set from below to avoid confusion.

enum {
      sNrS = 0,               // Subscripts
      sSrN,
      sErW,
      sWrE,
      sFrB,
      sBrF
};


typedef struct {

   // All values accumulate timings over the run.
   double
      total,               // Total time of execution.

      sum_grid,
      sum_grid_comm,

      *stencil,

      *bc_comp,            // Boundary condition: task computation time.
      bc_accum,            // Boundary condition: accumulation of task computation times.

      *bex,                // Boundary exchange 

      *pack_north,         // Msg buffer packing.
      *pack_south,         // Msg buffer packing.
      *pack_east,          // Msg buffer packing.
      *pack_west,          // Msg buffer packing.
      *pack_front,         // Msg buffer packing.
      *pack_back,          // Msg buffer packing.

      *send,               // Accumulated nonblocking sends.

      *send_north,         // Blocking sending.
      *send_south,         // Blocking sending.
      *send_east,          // Blocking sending.
      *send_west,          // Blocking sending.
      *send_front,         // Blocking sending.
      *send_back,          // Blocking sending.

      *recv,               // Accumulated nonblocking recvs.

      *recv_north,         // Blocking recvs
      *recv_south,         // Blocking recvs
      *recv_east,          // Blocking recvs
      *recv_west,          // Blocking recvs
      *recv_front,         // Blocking recvs
      *recv_back,          // Blocking recvs
      
      *unpack_north,       // Msg buffer unpacking.
      *unpack_south,       // Msg buffer unpacking.
      *unpack_east,        // Msg buffer unpacking.
      *unpack_west,        // Msg buffer unpacking.
      *unpack_front,       // Msg buffer unpacking.
      *unpack_back,        // Msg buffer unpacking.

      *wait_send,          // Waitall in iSR.

      *wait_send_north,    // Send wait.
      *wait_send_south,    // Send wait.
      *wait_send_east,     // Send wait.
      *wait_send_west,     // Send wait.
      *wait_send_front,    // Send wait.
      *wait_send_back,     // Send wait.

      *wait_recv_north,    // Recv wait.
      *wait_recv_south,    // Recv wait.
      *wait_recv_east,     // Recv wait.
      *wait_recv_west,     // Recv wait.
      *wait_recv_front,    // Recv wait.
      *wait_recv_back;     // Recv wait.

} Timings;

MG_EXTERN Timings timings;

enum {
      NS = 0,               // Subscripts
      EW,
      FB
};

enum {
      NORTH = 0,
      SOUTH,
      EAST,
      WEST,
      BACK,
      FRONT,

      SOUTHWEST,
      SOUTHEAST,

      NORTHWEST,
      NORTHEAST,

      FRONT_SOUTH,
      FRONT_NORTH,
      FRONT_WEST,
      FRONT_EAST,

      BACK_SOUTH,
      BACK_NORTH,
      BACK_WEST,
      BACK_EAST,

      BC_COUNTER,

      MG_INIT_GRID = 20,
      MG_INIT_GRID_ZEROS,
      MG_INIT_GRID_RANDOM,

      MG_SCALING = 30,
      MG_SCALING_STRONG,
      MG_SCALING_WEAK,

      MG_COMM_METHOD = 40,
      MG_COMM_METHOD_BSPMA,        // Not yet implemented.
      MG_COMM_METHOD_TASK_BLOCKS,
      MG_COMM_METHOD_SVAF,

      MG_STENCIL = 50,
      MG_STENCIL_2D5PT,
      MG_STENCIL_2D9PT,
      MG_STENCIL_3D7PT,
      MG_STENCIL_3D27PT,

      MG_COLLECTIVE_OP = 60,       // Not yet implemented.
      MG_COLLECTIVE_OP_MAX,
      MG_COLLECTIVE_OP_MIN,
      MG_COLLECTIVE_OP_SUM,

      MG_BC = 70,
      MG_BC_DIRICHLET,
      MG_BC_NEUMANN,              // Not yet implemented.
      MG_BC_REFLECTIVE,           // Not yet implemented.
      MG_BC_NONE,

      MG_BLOCK_ORDER = 80,
      MG_BLOCK_ORDER_CART,        // Sweep in order across i, j, k.
      MG_BLOCK_ORDER_MTA,         // MTA inspired.
      MG_BLOCK_ORDER_RANDOM,      // Randomly ordered blocks.
      MG_BLOCK_ORDER_COMM_FIRST_RAND, // Comm.-first random ordered blocks.
      MG_BLOCK_ORDER_TDAG,        // Task DAG.
      MG_BLOCK_ORDER_WTDAG,       // Weighted task DAG.

      MG_COMM_STRATEGY = 90,      // MPI send/recv configuration.
      MG_COMM_STRATEGY_SR,
      MG_COMM_STRATEGY_ISR,
      MG_COMM_STRATEGY_SIR,
      MG_COMM_STRATEGY_ISIR
};

// -------------------
// Function prototypes
// -------------------

int MG_Block_init ( InputParams *params, BlockInfo **blk );

int MG_Block_def_cart ( InputParams *params, BlockInfo **blk );

int MG_Block_def_comm_first_rand ( InputParams *params, BlockInfo **blk );

int MG_Block_def_random ( InputParams *params, BlockInfo **blk );

int MG_Block_def_mta ( InputParams *params, BlockInfo **blk );

int MG_Block_def_tdag ( InputParams *params, BlockInfo **blk );

int MG_Block_def_wtdag ( InputParams *params, BlockInfo **blk );

int MG_Block_set_neigh ( InputParams *params, BlockInfo **blk );

// mg_stencils.c:

int MG_Stencil ( InputParams params, StateVar **g, BlockInfo blk, int ivar );

int MG_Stencil_2d5pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk );

int MG_Stencil_2d9pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk );

int MG_Stencil_3d7pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk );

int MG_Stencil_3d27pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk );

int MG_Fake_work ( InputParams params, StateVar **g, BlockInfo blk, int ivar );

// mg_bc.c

int MG_Boundary_conditions        ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk );

int MG_Flux_accumulate            ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk );

int MG_Flux_accumulate_2d5pt3d7pt ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk );

int MG_Flux_accumulate_2d9pt      ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk );

int MG_Flux_accumulate_3d27pt     ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk );

// mg_boundary_exchange.c:

int MG_Boundary_exchange            ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_SR         ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_iSR        ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_SiR        ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_iSiR       ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_diags_SR   ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_diags_iSR  ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );
 
int MG_Boundary_exchange_diags_SiR  ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Boundary_exchange_diags_iSiR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar );

int MG_Set_diag_comm ( InputParams params, BlockInfo blk, int count[], int *bfd_xstart, int *bfd_xend, int *bfd_ystart, int *bfd_yend, int *ewd_start, int *ewd_end );

int MG_Get_tags ( InputParams params, BlockInfo blk, int ivar, int *msgtags );

// mg_collective.c:

int MG_Collective_op ( InputParams params, StateVar *g );

int MG_Collective_max ( InputParams params, StateVar *g );

int MG_Collective_min ( InputParams params, StateVar *g );

int MG_Collective_sum ( InputParams params, StateVar *g );

// mg_grid_ops.c:

int MG_Lgrid_op ( InputParams params, StateVar *g, BlockInfo blk );

int MG_Lgrid_max ( InputParams params, StateVar *g, BlockInfo blk );

int MG_Lgrid_min ( InputParams params, StateVar *g, BlockInfo blk );

int MG_Lgrid_sum ( InputParams params, StateVar *g, BlockInfo blk );

// mg_grid_utils.c

int MG_Grid_init ( InputParams *params, StateVar **g );

int MG_Fill_grid ( InputParams params, StateVar *g );

int MG_Sum_grid ( InputParams params, StateVar **g, int which, int final_check_flag );

// mg_profiling.c:

int MG_Init_perf ( InputParams params );

int MG_Report_performance ( InputParams params );
 
MG_REAL MG_Compute_stddev ( MG_REAL *values, int numvals, MG_REAL mean );

int MG_Profile_stats ( double* times, double *time_stat, int flag );

int MG_Max3i ( int a, int b, int c );

int MG_Min3i ( int a, int b, int c );

double MG_Timer ( void );

// Aliasing of timing system:
#if defined _MG_TIMING_L0 || defined _MG_TIMING_L1
#define MG_Time_start(time_start) do { time_start = MG_Timer(); } while ( 0 )
#define MG_Time_accum(time_start,this_time) do { this_time += MG_Timer() - time_start; } while ( 0 )
#if defined _MG_TIMING_L1
#define MG_Time_start_l1(time_start) do { time_start = MG_Timer(); } while ( 0 )
#define MG_Time_accum_l1(time_start,this_time) do { this_time += MG_Timer() - time_start; } while ( 0 )
#else
#define MG_Time_start_l1(time_start) do { } while ( 0 )
#define MG_Time_accum_l1(time_start,this_time) do { } while ( 0 )
#endif // _MG_TIMING_L1
#else
#define MG_Time_start(time_start) do { } while ( 0 )
#define MG_Time_accum(time_start,this_time) do { } while ( 0 )
#define MG_Time_start_l1(time_start) do { } while ( 0 )
#define MG_Time_accum_l1(time_start,this_time) do { } while ( 0 )
#endif // _MG_TIMING_L0 || _MG_TIMING_L1

// MG_Time_start ( time_start );
// MG_Time_accum(time_start,timing.pack_north[thread_id]);

// mg_spikes.c

int MG_Spike_init ( InputParams params, SpikeInfo **spikes );

int MG_Spike_insert ( InputParams params, SpikeInfo **spikes, int ispike, StateVar **g, int which );

// mg_utils.c:

void MG_Assert ( int ierr, const char *error_msg );

int MG_Terminate ( );

void *MG_CALLOC ( size_t count, size_t size_of_count );

double *MG_DCALLOC_INIT ( int count );

int MG_Check_input ( InputParams params );

int MG_Init ( int argc, char *argv[], InputParams *params );

int MG_IPOW ( int a, int b );

void *MG_MALLOC ( int count, size_t size );

void *MG_DECALLOC ( void *ptr, int decount, size_t size_of_decount );

int MG_Print_header ( InputParams params );

int MG_Barrier ( );

void MG_Print_help_message( void );

int MG_Process_commandline ( int argc, char* argv[], InputParams *params );

int MG_Env ( FILE *fp );

double SquareRoot(double number);
#endif // End _mg_tp_h_ in mg_tp.h
