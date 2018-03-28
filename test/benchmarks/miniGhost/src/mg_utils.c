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

int MG_Init ( int argc, char *argv[], InputParams *params )
{
   // ------------------
   // Local Declarations
   // ------------------

   int 
      i,                  // Counter
      ierr,               // Return status.
      gd,                 // Shorthand for params.ghostdepth
      mype_xy,            // tmp var
      remainder;

   // ---------------------
   // Executable statements 
   // ---------------------

   ierr = 0;

   // Set up computing environment

   mgpp.rootpe = 0; // For various control purposes.

/* TODO: For qthreads, make this call from all workers? (will not effect correctness) */

#if defined _MG_MPI

   CALL_MPI_Init ( &argc, &argv);
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Init" );

   ierr = CALL_MPI_Comm_dup ( MPI_COMM_WORLD, &MPI_COMM_MG );
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Comm_dup" );

   ierr = CALL_MPI_Errhandler_set ( MPI_COMM_MG, MPI_ERRORS_ARE_FATAL );
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Errhandler_set" );

   ierr = CALL_MPI_Comm_rank ( MPI_COMM_MG, &mgpp.mype );
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Comm_rank" );

   ierr = CALL_MPI_Comm_size ( MPI_COMM_MG, &mgpp.numpes );
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Comm_size" );

#elif defined _MG_SERIAL
   mgpp.mype = 0;    // Formulas based on these values should work correctly.
   mgpp.numpes = 1;
#endif

#if defined _MG_OPENMP
#pragma omp parallel
#endif
{
   mgpp.num_threads = mg_get_num_os_threads();
   mgpp.thread_id  = mg_get_os_thread_num();
}

#if defined _MG_DEBUG
   printf ( " [pe %d] OpenMP thread %d of %d. \n", mgpp.mype, mgpp.thread_id, mgpp.num_threads );
#endif

   // Default settings:
   params->comm_method         = MG_COMM_METHOD_TASK_BLOCKS;
   params->comm_strategy       = MG_COMM_STRATEGY_SR;
   params->scaling             = MG_SCALING_WEAK;
   params->stencil             = MG_STENCIL_3D7PT;
   params->nx                  = 100;
   params->ny                  = 100;
   params->nz                  = 100;
   params->init_grid_values    = MG_INIT_GRID_RANDOM;
   params->ghostdepth          = 1;
   params->boundary_condition  = MG_BC_DIRICHLET;

   params->numvars             = 1;

   params->extra_work_nvars    = 0;
   params->extra_work_percent  = 0;

   params->numspikes           = 1;
   params->numtsteps           = 10;
   params->error_tol_exp       = 5;

   params->npx                 = mgpp.numpes;
   params->npy                 = 1;
   params->npz                 = 1;

   params->percent_sum         = 100;
   params->check_answer_freq   = 1;
   params->debug_grid          = 0;

   params->blkorder            = MG_BLOCK_ORDER_CART;
   params->blkxlen             = params->nx;
   params->blkylen             = params->ny;
   params->blkzlen             = params->nz;

   params->available_param     = 0;

   // End default settings

   ierr = MG_Process_commandline ( argc, argv, params );
   MG_Assert ( !ierr, "MG_Init:MG_Process_commandline" );

   // ---------------------------------
   // Set position in 3d processor grid
   // ---------------------------------

   mype_xy = mgpp.mype % ( params->npx*params->npy );

   params->mypy = mype_xy / params->npx;
   remainder = mype_xy % params->npx;
   if ( remainder != 0 )
      params->mypx = remainder;
   else
      params->mypx = 0;

   params->mypz = mgpp.mype / ( params->npx*params->npy );

   MG_Barrier ( );
#if defined _MG_MPI

   // --------------
   // Set neighbors.
   // --------------

   gd = params->ghostdepth;

   // This uses the default block settings, which is the block equals the domain, i.e. no sub-blocks.
   // This will be over-written in MG_Block_init strategy choice.
   // Determine maximum face size
   int mindim = MG_Min3i ( params->blkxlen + ( 2*gd ),
                           params->blkylen + ( 2*gd ),
                           params->blkzlen + ( 2*gd ) );
   if ( mindim == params->blkxlen ) 
      max_msg_count = ( params->blkylen + (2*gd) ) * ( params->blkzlen + (2*gd) );
   else if ( mindim == params->blkylen )
      max_msg_count = ( params->blkxlen + (2*gd) ) * ( params->blkzlen + (2*gd) );
   else
      max_msg_count = ( params->blkxlen + (2*gd) ) * ( params->blkylen + (2*gd) );

   neighbors = (int*)MG_CALLOC ( MAX_NUM_NEIGHBORS, sizeof(int) );
   MG_Assert ( neighbors != NULL, "MG_Init: MG_CALLOC ( neighbors )" );
   for ( i=0; i<MAX_NUM_NEIGHBORS; i++ )
      neighbors[i] = -1;

   params->num_neighs = 0;
   if ( params->mypy != 0  ) {
      neighbors[SOUTH] = mgpp.mype - params->npx;
      params->num_neighs++;
   }
   if ( params->mypy != params->npy-1 ) {
      neighbors[NORTH] = mgpp.mype + params->npx;
      params->num_neighs++;
   }
   if ( params->mypx != 0 ) {
      neighbors[WEST] = mgpp.mype - 1;
      params->num_neighs++;
   }
   if ( params->mypx != params->npx-1 ) {
      neighbors[EAST] = mgpp.mype + 1;
      params->num_neighs++;
   }
   CALL_MPI_Barrier ( MPI_COMM_MG);
   //printf ( " [pe %d] params->mypz = %d, params->npx/y/z = %d,%d,%d\n", mgpp.mype, params->mypz, params->npx, params->npy, params->npz ); 
   CALL_MPI_Barrier ( MPI_COMM_MG);
   if ( params->mypz != 0 ) {
      neighbors[FRONT] = mgpp.mype - ( params->npx*params->npy );
      //printf ( " [pe %d] FRONT neighbor = %d \n", mgpp.mype, neighbors[FRONT] );
      params->num_neighs++;
   }
   CALL_MPI_Barrier ( MPI_COMM_MG);

   if ( params->mypz != params->npz-1 ) {
      neighbors[BACK] = mgpp.mype + ( params->npx*params->npy );
      //printf ( " [pe %d] BACK neighbor = %d \n", mgpp.mype, neighbors[BACK] );
      params->num_neighs++;
   }
   CALL_MPI_Barrier ( MPI_COMM_MG);

#if defined _MG_DEBUG
   MG_Barrier ();
   printf ( " [pe %d] NEIGHBORS SET TO (%d %d) (%d %d) (%d %d) \n", mgpp.mype, neighbors[NORTH], 
            neighbors[SOUTH], neighbors[EAST], neighbors[WEST], neighbors[FRONT], neighbors[BACK] );
   MG_Barrier ();
#endif

#endif

   MG_Init_perf ( *params );

   if ( params->debug_grid == 1 ) 
      params->percent_sum = 100;   // Setting for action is in MG_Grid_init.

   return ( ierr );

} // end MG_Init

// ======================================================================================

int MG_Process_commandline ( int argc, char* argv[], InputParams *params )
{
   // -----------------------
   // Parameterized variables
   // -----------------------

   enum {
      _SCALING = 0,
      _NX,
      _NY,
      _NZ,
      _INIT_GRID_VALUES,
      _GHOST_DEPTH,
      _NUMVARS,
      _PERCENT_SUM,
      _NUMSPIKES,
      _NUMTSTEPS,
      _EXTRA_WORK_NVARS,
      _EXTRA_WORK_PERCENT,
      _STENCIL,
      _COMM_METHOD,
      _COMM_STRATEGY,
      _BLK_DEF,
      _BLK_X,
      _BLK_Y,
      _BLK_Z,
      _ERROR_TOL,
      _REPORT_DIFFUSION,
      _NPX,
      _NPY,
      _NPZ,
      _CHECK_ANSWER_FREQ,
      _DEBUG_GRID,
      _LAST_VAL
};

   // ------------------
   // Local Declarations
   // ------------------

   char
      *blkorder_s,
      *boundary_condition_s,
      *comm_method_s,
      *comm_strategy_s,
      *init_grid_values_s,
      *scaling_s,
      *stencil_s;

   int
      flag_blklen,
      count_problem_params = _LAST_VAL,
      i,
      ierr,
      problem_params[count_problem_params],
      remainder,
      tmp_nx, tmp_ny, tmp_nz;

   // ---------------------
   // Executable Statements
   // ---------------------

   size_t len = 30;

   flag_blklen = 0;    // If user does not set this, blocksize is set to nx, ny, nz.

   blkorder_s           = (char*)calloc ( len, sizeof(char) );
   boundary_condition_s = (char*)calloc ( len, sizeof(char) );
   comm_method_s        = (char*)calloc ( len, sizeof(char) );
   comm_strategy_s      = (char*)calloc ( len, sizeof(char) );
   init_grid_values_s   = (char*)calloc ( len, sizeof(char) );
   scaling_s            = (char*)calloc ( len, sizeof(char) );
   stencil_s            = (char*)calloc ( len, sizeof(char) );

   tmp_nx = params->nx; // Sets default value.
   tmp_ny = params->ny; // Sets default value.
   tmp_nz = params->nz; // Sets default value.

   if ( mgpp.mype == 0 ) {
      for ( i=1; i<argc; i++ ) {
         if ( strcmp ( argv[i], "--scaling" ) == 0 ) {
            strcpy ( scaling_s, argv[++i] );
            if ( strcmp ( scaling_s, "MG_SCALING_WEAK" ) == 0 )  {
               params->scaling = MG_SCALING_WEAK;
            }
            else if ( strcmp ( scaling_s, "MG_SCALING_STRONG" ) == 0 ) {
               params->scaling = MG_SCALING_STRONG;
            }
            else {
                  fprintf ( stderr, "Unknown scaling option (%s). \n", scaling_s );
                  MG_Print_help_message ( );
                  ierr = -1;
                  MG_Assert ( !ierr, "MG_Process_commandline: Unknown scaling option." );
            }
         }
         else if ( !strcmp ( argv[i], "--nx" ) ) {
            tmp_nx = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--ny" ) ) {
            tmp_ny = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--nz" ) ) {
            tmp_nz = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--ndim" ) ) {
            tmp_nx = atoi ( argv[++i] );
            tmp_ny = tmp_nx;
            tmp_nz = tmp_nx;
         }
         else if ( !strcmp ( argv[i], "--init_grid_values" ) ) {
            strcpy ( init_grid_values_s, argv[++i] );
            if ( strcmp ( init_grid_values_s, "MG_INIT_GRID_RANDOM" ) == 0 ) {
               params->init_grid_values = MG_INIT_GRID_RANDOM;
            }
            else if ( strcmp ( init_grid_values_s, "MG_INIT_GRID_ZEROS" ) == 0 ) {
               params->init_grid_values = MG_INIT_GRID_ZEROS;
            }
            else {
               fprintf ( stderr, "Unknown init_grid_values option (%s). \n", init_grid_values_s );
               MG_Print_help_message ( );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Process_commandline: Unknown init_grid_values option." );
            }
         }
         else if ( !strcmp ( argv[i], "--ghostdepth" ) ) {
            params->ghostdepth = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--boundary_condition" ) ) {
            strcpy ( boundary_condition_s, argv[++i] );
            if ( strcmp ( boundary_condition_s, "MG_BC_DIRICHLET" ) == 0 ) {
               params->boundary_condition = MG_BC_DIRICHLET;
            }
            else if ( strcmp ( boundary_condition_s, "MG_BC_NONE" ) == 0 ) {
               params->boundary_condition = MG_BC_NONE;
            }
            else {
               fprintf ( stderr, "Unknown boundary condition option (%s). \n", boundary_condition_s );
               MG_Print_help_message ( );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Process_commandline: Unknown boundary condition." );
            }
         }
         else if ( !strcmp ( argv[i], "--block_order" ) ) {
            strcpy ( blkorder_s, argv[++i] );
            if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_CART" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_CART;
            }
            else if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_MTA" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_MTA;
            }
            else if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_RANDOM" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_RANDOM;
            }
            else if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_COMM_FIRST_RAND" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_COMM_FIRST_RAND;
            }
            else if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_TDAG" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_TDAG;
            }
            else if ( strcmp ( blkorder_s, "MG_BLOCK_ORDER_WTDAG" ) == 0 ) {
               params->blkorder = MG_BLOCK_ORDER_WTDAG;
            }
         } 
         else if ( !strcmp ( argv[i], "--blkxlen" ) ) {
            params->blkxlen = atoi ( argv[++i] );
            flag_blklen = 1;
         }
         else if ( !strcmp ( argv[i], "--blkylen" ) ) {
            params->blkylen = atoi ( argv[++i] );
            flag_blklen = 1;
         }
         else if ( !strcmp ( argv[i], "--blkzlen" ) ) {
            params->blkzlen = atoi ( argv[++i] );
            flag_blklen = 1;
         }
         else if ( !strcmp ( argv[i], "--blkxyzlen" ) ) {
            int blocksize_xyz = atoi ( argv[++i] );
            params->blkxlen = blocksize_xyz;
            params->blkylen = blocksize_xyz;
            params->blkzlen = blocksize_xyz;
            flag_blklen = 1;
         } 
         else if ( !strcmp ( argv[i], "--numvars" ) ) {
            params->numvars = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--percent_sum" ) ) {
            params->percent_sum = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--numspikes" ) ) {
            params->numspikes = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--numtsteps" ) ) {
            params->numtsteps = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--stencil" ) ) {
            strcpy ( stencil_s, argv[++i] );
            if ( strcmp ( stencil_s, "MG_STENCIL_2D5PT" ) == 0 )
               params->stencil = MG_STENCIL_2D5PT;
            else if ( strcmp ( stencil_s, "MG_STENCIL_2D9PT" ) == 0 )
               params->stencil = MG_STENCIL_2D9PT;
            else if ( strcmp ( stencil_s, "MG_STENCIL_3D7PT" ) == 0 )
               params->stencil = MG_STENCIL_3D7PT;
            else if ( strcmp ( stencil_s, "MG_STENCIL_3D27PT" ) == 0 )
               params->stencil = MG_STENCIL_3D27PT;
            else {
               fprintf ( stderr, "Unknown stencil option (%s). \n", stencil_s );
               MG_Print_help_message ( );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Process_commandline: Unknown stencil option." );
            }
         }
         else if ( !strcmp ( argv[i], "--extra_work_nvars" ) ) {
            params->extra_work_nvars = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--extra_work_percent" ) ) {
            params->extra_work_percent = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--comm_method" ) ) {
            strcpy ( comm_method_s, argv[++i] );
            if ( strcmp ( comm_method_s, "MG_COMM_METHOD_BSPMA" ) == 0 ) {
               params->comm_method = MG_COMM_METHOD_BSPMA;
            }
            else if ( strcmp ( comm_method_s, "MG_COMM_METHOD_SVAF" ) == 0 ) {
               params->comm_method = MG_COMM_METHOD_SVAF;
            }
            else if ( strcmp ( comm_method_s, "MG_COMM_METHOD_TASK_BLOCKS" ) == 0 ) {
               params->comm_method = MG_COMM_METHOD_TASK_BLOCKS;
            }
            else {
               fprintf ( stderr, "Unknown comm_method option (%s). \n", comm_method_s );
               MG_Print_help_message ( );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Process_commandline: Unknown comm_method option." );
            }
         }
         else if ( !strcmp ( argv[i], "--comm_strategy" ) ) {
            strcpy ( comm_strategy_s, argv[++i] );
            if ( strcmp ( comm_strategy_s, "MG_COMM_STRATEGY_SR" ) == 0 ) {
               params->comm_strategy = MG_COMM_STRATEGY_SR;
            }
            else if ( strcmp ( comm_strategy_s, "MG_COMM_STRATEGY_ISR" ) == 0 ) {
               params->comm_strategy = MG_COMM_STRATEGY_ISR;
            }
            else if ( strcmp ( comm_strategy_s, "MG_COMM_STRATEGY_SIR" ) == 0 ) {
               params->comm_strategy = MG_COMM_STRATEGY_SIR;
            }
            else if ( strcmp ( comm_strategy_s, "MG_COMM_STRATEGY_ISIR" ) == 0 ) {
               params->comm_strategy = MG_COMM_STRATEGY_ISIR;
            }
            else {
               fprintf ( stderr, "Unknown comm_strategy option (%s). \n", comm_strategy_s );
               MG_Print_help_message ( );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Process_commandline: Unknown comm_method option." );
            }
         }
         else if ( !strcmp ( argv[i], "--error_tol" ) ) {
            params->error_tol_exp = atoi ( argv[++i] ); // Converted below to real value.
         }
         else if ( !strcmp ( argv[i], "--available_param" ) ) {
            params->available_param = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--npx" ) ) {
            params->npx = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--npy" ) ) {
            params->npy = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--npz" ) ) {
            params->npz = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--npdim" ) ) {
            params->npx = atoi ( argv[++i] );
            params->npy = params->npx;
            params->npz = params->npx;
         }
#if defined _MG_QT
         /* TODO: decide whether to support --qt_num_threads commandline option.
                  This is read after the Qthreads is initialized, so has no
                  effect on the hardware parallelism used.
         else if ( !strcmp ( argv[i], "--qt_num_threads" ) ) {
            num_threads = atoi ( argv[++i] );
            setenv("QT_HWPAR", argv[i], 1);
         }
         */
#endif

         else if ( !strcmp ( argv[i], "--check_answer_freq" ) ) {
            params->check_answer_freq = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--debug_grid" ) ) {
            params->debug_grid = atoi ( argv[++i] );
         }
         else if ( !strcmp ( argv[i], "--help" ) ) {
            MG_Print_help_message ();
            ierr = -1;
            MG_Assert ( !ierr, "MG_Process_commandline: Listing of command line options requested to be printed to stdio" );
         }
         else {
            fprintf ( stderr, "\n" );
            fprintf ( stderr, "\n ** Error ** Unknown input parameter %s (%s). \n", argv[i], argv[i+1] );
            MG_Print_help_message ();
            ierr = -1;
            MG_Assert ( !ierr, "MG_Process_commandline: Unknown input parameter." );
         }
      } // End parsing command line.

      if ( params->scaling == MG_SCALING_WEAK ) {
         params->nx = tmp_nx;
         params->ny = tmp_ny;
         params->nz = tmp_nz;
      }
      else if ( params->scaling == MG_SCALING_STRONG ) {
         params->nx = tmp_nx / params->npx;
         remainder = tmp_nx % params->npx;
         if ( params->nx < remainder ) {
            params->nx++;
         }
         params->ny = tmp_ny / params->npy;
         remainder = tmp_ny % params->npy;
         if ( mgpp.mype < remainder ) {
            params->ny++;
         }
         params->nz = tmp_nz / params->npz;
         remainder = tmp_nz % params->npz;
         if ( mgpp.mype < remainder ) {
            params->nz++;
         }
      }
      else {
         fprintf ( stderr, "\n" );
         fprintf ( stderr,
                   "\n ** Error ** Unknown scaling %d; options are MG_SCALING_WEAK aqnd MG_SCALING_STRONG. \n",
                   params->scaling );
         ierr = -1;
         MG_Assert ( !ierr, "MG_Process_commandline: Unknown scaling option" );

      } // end ( params->scaling )

      if ( flag_blklen == 0 ) {
         params->blkxlen = params->nx;
         params->blkylen = params->ny;
         params->blkzlen = params->nz;
      }

      ierr = MG_Check_input ( *params );
      MG_Assert ( !ierr, "MG_Process_commandline: MG_Check_input" );

      i=0;
      problem_params[_SCALING]            = params->scaling;
      problem_params[_NX]                 = params->nx;
      problem_params[_NY]                 = params->ny;
      problem_params[_NZ]                 = params->nz;

      problem_params[_INIT_GRID_VALUES]   = params->init_grid_values;

      problem_params[_GHOST_DEPTH]        = params->ghostdepth;

      problem_params[_NUMVARS]            = params->numvars;
      problem_params[_PERCENT_SUM]        = params->percent_sum;
      problem_params[_NUMSPIKES]          = params->numspikes;
      problem_params[_NUMTSTEPS]          = params->numtsteps;

      problem_params[_EXTRA_WORK_NVARS]   = params->extra_work_nvars;
      problem_params[_EXTRA_WORK_PERCENT] = params->extra_work_percent;

      problem_params[_STENCIL]            = params->stencil;
      problem_params[_ERROR_TOL]          = params->error_tol_exp;
      problem_params[_REPORT_DIFFUSION]   = params->available_param;
#if defined _MG_MPI
      problem_params[_COMM_METHOD]        = params->comm_method;
      problem_params[_COMM_STRATEGY]      = params->comm_strategy;

      problem_params[_NPX]                = params->npx;
      problem_params[_NPY]                = params->npy;
      problem_params[_NPZ]                = params->npz;
#else
      problem_params[_NPX]                = 1;
      problem_params[_NPY]                = 1;
      problem_params[_NPZ]                = 1;
#endif
      problem_params[_CHECK_ANSWER_FREQ]  = params->check_answer_freq;

      problem_params[_DEBUG_GRID]         = params->debug_grid;

      problem_params[_BLK_DEF]            = params->blkorder;

      problem_params[_BLK_X]              = params->blkxlen;
      problem_params[_BLK_Y]              = params->blkylen;
      problem_params[_BLK_Z]              = params->blkzlen;

   } // End mgpp.rootpe processing.


#if defined _MG_MPI
   ierr = CALL_MPI_Bcast( problem_params, count_problem_params, MPI_INT, mgpp.rootpe, MPI_COMM_WORLD );
   MG_Assert ( MPI_SUCCESS == ierr, "CALL_MPI_Bcast( problem_params" );
#endif

   params->scaling             = problem_params[_SCALING];

   params->nx                  = problem_params[_NX];
   params->ny                  = problem_params[_NY];
   params->nz                  = problem_params[_NZ];

   params->init_grid_values    = problem_params[_INIT_GRID_VALUES];

   params->ghostdepth          = problem_params[_GHOST_DEPTH];

   params->numvars             = problem_params[_NUMVARS];
   params->percent_sum         = problem_params[_PERCENT_SUM];

   params->numspikes           = problem_params[_NUMSPIKES];
   params->numtsteps           = problem_params[_NUMTSTEPS];

   params->extra_work_nvars    = problem_params[_EXTRA_WORK_NVARS];
   params->extra_work_percent  = problem_params[_EXTRA_WORK_PERCENT];

   params->stencil             = problem_params[_STENCIL];
   params->comm_method         = problem_params[_COMM_METHOD];
   params->comm_strategy       = problem_params[_COMM_STRATEGY];

   params->error_tol_exp       = problem_params[_ERROR_TOL];
   params->available_param     = problem_params[_REPORT_DIFFUSION];

   params->npx                 = problem_params[_NPX];
   params->npy                 = problem_params[_NPY];
   params->npz                 = problem_params[_NPZ];

   params->check_answer_freq   = problem_params[_CHECK_ANSWER_FREQ];
   if ( params->check_answer_freq == 0 ) { 
      params->check_answer_freq = params->numtsteps + 1;
   }
   params->debug_grid          = problem_params[_DEBUG_GRID];

   params->blkorder            = problem_params[_BLK_DEF];

   params->blkxlen             = problem_params[_BLK_X];
   params->blkylen             = problem_params[_BLK_Y];
   params->blkzlen             = problem_params[_BLK_Z];

   ierr = MG_Check_input ( *params );
   MG_Assert ( !ierr, "MG_Check_input" );

   params->error_tol = 1.0 / (MG_REAL)(MG_IPOW ( 10, params->error_tol_exp ) );
   params->iter_error = (MG_REAL*)calloc ( params->numvars, sizeof(MG_REAL) );
   MG_Assert ( params->iter_error != NULL, 
               "MG_Process_commandline: Failed to allocate space for params->iter_error" );
   for ( i=0; i<params->numvars; i++ ) {
      params->iter_error[i] = 0.0;
   }
   return ( ierr );

} // End MG_Process_commandline.

// ======================================================================================

void MG_Print_help_message ()
{
   // ---------------------
   // Executable statements
   // ---------------------

   fprintf ( stderr, "\n" );
   fprintf ( stderr, "\n (Optional) command line input is of the form: \n" );
   fprintf ( stderr, "\n" );

   fprintf ( stderr, " --nx  ( > 0 )\n" );
   fprintf ( stderr, " --ny  ( > 0 )\n" );
   fprintf ( stderr, " --nz  ( > 0 )\n" );
   fprintf ( stderr, " --ndim : for cubes, i.e. nx=ny=nz.  ( > 0 )\n" );
   fprintf ( stderr, "\n" );

   fprintf ( stderr, " --blkorder (MG_BLOCK_ORDER_CART, MG_BLOCK_ORDER_RANDOM \n" );
   fprintf ( stderr, " --blkxlen ( > 0 )\n" );
   fprintf ( stderr, " --blkylen ( > 0 )\n" );
   fprintf ( stderr, " --blkzlen ( > 0 )\n" );
   fprintf ( stderr, "\n" );

   fprintf ( stderr, " --stencil \n" );
   fprintf ( stderr, " --boundary_condition \n" );

   fprintf ( stderr, " --numvars (0 < numvars <= 40)\n" );

   fprintf ( stderr, " --percent_sum (0 through 100) \n" );

   fprintf ( stderr, " --numspikes ( > 0 )\n" );
   fprintf ( stderr, " --numtsteps ( > 0 )\n" );

   fprintf ( stderr, " --extra_work_nvasrs ( > 0 )\n" );
   fprintf ( stderr, " --extra_work_percent ( > 0 )\n" );

   fprintf ( stderr, " --stencil \n" );
   fprintf ( stderr, " --error_tol ( e^{-error_tol}; >= 0) \n" );
   fprintf ( stderr, " --available_param (>= 0) \n" );
#if !defined _MG_SERIAL
   fprintf ( stderr, " --scaling\n" );
   fprintf ( stderr, " --comm_method \n" );
   fprintf ( stderr, " --comm_strategy \n" );
   fprintf ( stderr, " --npx ( 0 < npx <= numpes )\n" );
   fprintf ( stderr, " --npy ( 0 < npy <= numpes )\n" );
   fprintf ( stderr, " --npz ( 0 < npz <= numpes )\n" );
   fprintf ( stderr, " --npdim : for cubes, i.e. npx=npy=npz. ( npdim^3 = numpes )\n" );
#endif
   fprintf ( stderr, " --check_answer_freq : >= 0 \n" );

   fprintf ( stderr, " --debug_grid ( 0, 1 )\n" );
   fprintf ( stderr, "\n" );

#if defined _MG_MPI
   CALL_MPI_Abort ( MPI_COMM_WORLD, -1 );
   exit(0);
#elif defined _MG_OPENMP
   exit(0);
#elif defined _MG_SERIAL
   exit(0);
#else
   Communication protocol not defined in makefile. ( -D_MPI or -D_SERIAL).
#endif
} // End MG_Print_help_message.

// ======================================================================================

int MG_Check_input ( InputParams params )
{

   // ------------------
   // Local Declarations
   // ------------------

   int
      num_input_err = 0;

   // ---------------------
   // Executable statements
   // ---------------------

   // Unimplemented but planned, so defined in mg_tp.h
   if ( params.comm_method == MG_COMM_METHOD_BSPMA ) {
      MG_Assert ( -1, "BSPMA not yet implemented." );
   }
   if ( params.boundary_condition == MG_BC_NEUMANN || params.boundary_condition == MG_BC_REFLECTIVE ) {
      MG_Assert ( -1, "Boundary condition choice not yet implemented." );
   }

   if ( params.nx <= 0 ) {
      num_input_err++;
      fprintf ( stderr, " [pe %d] ** Input error **: nx %d <= 0. \n", mgpp.mype, params.nx );
   }
   if ( params.ny <= 0 ) {
      num_input_err++;
      fprintf ( stderr, " [pe %d] ** Input error **: ny %d <= 0. \n", mgpp.mype, params.ny );
   }
   if ( params.nz <= 0 ) {
      num_input_err++;
      fprintf ( stderr, " [pe %d] ** Input error **: nz %d <= 0. \n", mgpp.mype, params.nz );
   }
   if ( ( ( params.nx % params.blkxlen ) + ( params.ny % params.blkylen ) + ( params.nz % params.blkzlen ) ) != 0 ) {
      fprintf ( stderr, " %d %d %d %d %d %d \n", params.nx, params.blkxlen, params.ny, 
                params.blkylen,params.nz, params.blkzlen );
      fprintf ( stderr, " %d %d %d \n", params.nx % params.blkxlen, params.ny % params.blkylen, 
                params.nz % params.blkzlen );
      fprintf ( stderr, " %d \n", ( ( params.nx % params.blkxlen ) + ( params.ny % params.blkylen ) + ( params.nz % params.blkzlen ) ) );
      num_input_err++;
      fprintf ( stderr, " [pe %d] ** Input error **: blocksize (%d,%d,%d) must evenly divide domain dimensions (%d,%d,%d)\n", mgpp.mype, params.blkxlen, params.blkylen, params.blkzlen, params.nx, params.ny, params.nz );
   }
   if ( params.numvars <= 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: numvars %d <= 0. \n", mgpp.mype, params.numvars );
   }
   if ( params.extra_work_nvars > params.numvars ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: extra_work_nvars %d > numvars %d \n", mgpp.mype, params.extra_work_nvars, params.numvars );
   }
   if ( params.extra_work_percent < 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: extra_work_percent %d < 0 \n", mgpp.mype, params.extra_work_percent );
   }
   if ( params.extra_work_percent > 100 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: extra_work_percent %d > 100 \n", mgpp.mype, params.extra_work_percent );
   }
   if ( params.numspikes <= 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: numspikes %d <= 0. \n", mgpp.mype, params.numspikes );
   }
   if ( params.numtsteps < 1 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: numtsteps %d < 1. \n", mgpp.mype, params.numtsteps );
   }
   if ( params.error_tol_exp <= 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: error_tol 10^{%d}. \n", mgpp.mype, params.error_tol_exp );
   }
   if ( params.npx < 1 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: npx %d < 1. \n", mgpp.mype, params.npx );
   }
   if ( params.npy < 1 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: npy %d < 1. \n", mgpp.mype, params.npy );
   }
   if ( params.npz < 1 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: npz %d < 1. \n", mgpp.mype, params.npz );
   }
   if ( params.npx*params.npy*params.npz != mgpp.numpes ) {
      num_input_err++;
      fprintf ( stderr,
                "\n\n [pe %d] ** Input error **: logical process grid (npx, npy, npz) = (%d, %d, %d) is not equal to the number of parallel processes (%d).\n",
                 mgpp.mype, params.npx, params.npy, params.npz, mgpp.numpes );
   }
   //if ( params.available_param < 0 ) {
   //   num_input_err++;
   //   fprintf ( stderr, "\n\n [pe %d] ** Input error **: available_param %d != 0 or 1. \n", mgpp.mype, params.available_param );
   //}
   if ( params.check_answer_freq < 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: check_answer_freq %d < 0. \n", mgpp.mype, params.check_answer_freq );
   }
   if ( params.percent_sum < 0) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: percent_sum %d < 0.\n", mgpp.mype, params.percent_sum );
   }
   if ( params.percent_sum > 100 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: percent_sum > numvars = %d.\n", mgpp.mype, params.percent_sum );
   }
   if ( params.debug_grid < 0 ) {
      num_input_err++;
      fprintf ( stderr, "\n\n [pe %d] ** Input error **: debug_grid %d < 0. \n", mgpp.mype, params.debug_grid );
   }

   return ( num_input_err );

} // End MG_Check_input.

// ======================================================================================

void MG_Assert ( int mg_status, const char *error_msg )
{

   // ---------------------
   // Executable statements
   // ---------------------

#if defined _MG_MPI
   if ( mg_status == 0 ) {
      fprintf (stderr, "\n\n [pe %d] ** Error ** %s. \n\n\n", mgpp.mype, error_msg );
      fflush ( stdout );
      fflush ( stderr );
      CALL_MPI_Abort ( MPI_COMM_WORLD, -1 );
      exit ( -1 );
   }
#else
   if ( mg_status == 0 ) {
      fprintf (stderr, " ** Error ** %s. \n", error_msg );
      fflush ( stdout );
      fflush ( stderr );
      exit ( -1 );
      }
#endif

   return;

} // End MG_Assert

// ======================================================================================

int MG_Terminate (  )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0; // Return status.

   // ---------------------
   // Executable statements
   // ---------------------

#if defined _MG_MPI
   ierr = CALL_MPI_Finalize ( );
   MG_Assert ( !ierr, "MG_Terminate: CALL_MPI_Finalize" );
#endif

   return ( ierr );

} // End MG_Assert

// ======================================================================================

int MG_Print_header ( InputParams params )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0; // Return status.

   // ---------------------
   // Executable statements
   // ---------------------

   int mype        = mgpp.mype;
   int rootpe      = mgpp.rootpe;
   int numpes      = mgpp.numpes;
   int num_threads = mgpp.num_threads;

   if ( mype != rootpe ) 
      return ( ierr );

   MG_REAL mem_allocated = (MG_REAL)( ((params.nx+params.ghostdepth)*(params.ny+params.ghostdepth)*(params.nz+params.ghostdepth)) * sizeof(MG_REAL) ) * ( params.numvars + 1 ) / GIGA;

   fprintf ( stdout, "\n" );
   fprintf ( stdout, "\n ========================================================================= \n" );

#if defined _MG_SERIAL
#if defined _MG_OPENMP
   fprintf ( stdout, " Mantevo miniGhost, task parallel (OpenMP) version. \n" );
   fprintf ( stdout, "\n" );
   if ( num_threads == 1 ) {
      fprintf ( stdout, " 1 OpenMP thread. \n" );
   }
   else {
      fprintf ( stdout, " %d OpenMP threads. \n", num_threads );
   }
   fprintf ( stdout, "\n" );
#else
   fprintf ( stdout, " Mantevo miniGhost, task parallel (serial) version. \n" );
   fprintf ( stdout, "\n" );
#endif
#elif defined _MG_MPI && _MG_OPENMP
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI+OpenMP) version \n" );
   fprintf ( stdout, "\n" );
   if ( num_threads == 1 ) {
      fprintf ( stdout, " 1 OpenMP thread on each of %d MPI processes. \n", numpes );
   }
   else {
      fprintf ( stdout, " %d OpenMP threads on each of %d MPI processes. \n", num_threads, numpes );
   }
   fprintf ( stdout, "\n" );
#elif defined _MG_MPIQ && defined _MG_MPI
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI+Qthreads) version, %d MPI processes\n", numpes );
   fprintf ( stdout, "\n" );
#elif defined _MG_MPI
   fprintf ( stdout, " Mantevo miniGhost, task parallel (MPI) version. \n" );
   fprintf ( stdout, "\n" );
   if ( numpes == 1 ) {
      fprintf ( stdout, " 1 MPI process " );
   }
   else {
      fprintf ( stdout, " %d MPI processes ", numpes );
   }
#else
  // FIXME: need to clean up logic to match the actual configuration (DTS)
  MG_Print_header: Communication protocol not defined.
#endif

   switch ( params.stencil ) {
      case ( MG_STENCIL_2D5PT  ) :
         fprintf ( stdout, " Applying 5 point stencil in 2 dimensions. \n" );
         break;
      case ( MG_STENCIL_2D9PT  ) :
         fprintf ( stdout, " Applying 9 point stencil in 2 dimensions. \n" );
         break;
      case ( MG_STENCIL_3D7PT  ) :
         fprintf ( stdout, " Applying 7 point stencil in 3 dimensions. \n" );
         break;
      case ( MG_STENCIL_3D27PT ) :
         fprintf ( stdout, " Applying 27 point stencil in 3 dimensions. \n" );
         break;
      default:
         break;
   } // End switch ( params.stencil )
   fprintf ( stdout, "\n" );

#if defined _MG_MPI
   switch ( params.comm_strategy ) {
      case ( MG_COMM_STRATEGY_SR ) :
         fprintf ( stdout, " Blocking send / blocking recv. (MG_COMM_STRATEGY_SR) \n" );
         break;
      case ( MG_COMM_STRATEGY_ISR ) :
         fprintf ( stdout, " Nonblocking send / blocking recv. (MG_COMM_STRATEGY_ISR) \n" );
         break;
      case ( MG_COMM_STRATEGY_SIR ) :
         fprintf ( stdout, " Blocking send / nonblocking recv. (MG_COMM_STRATEGY_SIR) \n" );
         break;
      case ( MG_COMM_STRATEGY_ISIR ) :
         fprintf ( stdout, " Nonblocking send / nonblocking recv. (MG_COMM_STRATEGY_ISIR) \n" );
         break;
      default:
         break;
   } // End switch ( params.comm_strategy )
#if defined _DONT_WORRY_ABOUT_TAGS
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Tags are SHARED for all messages between pairs of comm partners,\n i.e. compiled with -D_DONT_WORRY_ABOUT_TAGS.\n" );
   fprintf ( stdout, "\n" );
#endif
#endif

   switch ( params.blkorder ) {
      case ( MG_BLOCK_ORDER_CART ) : // This is the default ordering.
         fprintf ( stdout, " Regular Cartesian block decomposition strategy \n" );
         break;
      case ( MG_BLOCK_ORDER_MTA )  : // 
         fprintf ( stdout, " MTA-style block decomposition strategy: \n" );
         break;
      case ( MG_BLOCK_ORDER_RANDOM )  : // 
         fprintf ( stdout, " Random block decomposition strategy: \n" );
         break;
      case ( MG_BLOCK_ORDER_COMM_FIRST_RAND )  : // 
         fprintf ( stdout, " Comm-first random block decomposition strategy: \n" );
         break;
      case ( MG_BLOCK_ORDER_TDAG ) : // Task DAG.
         fprintf ( stdout, " Task DAG block decomposition strategy: \n" );
         break;
      case ( MG_BLOCK_ORDER_WTDAG ) : // Weighted task DAG.
         fprintf ( stdout, " Weighted task DAG block decomposition strategy: \n" );
         break;
   }
   fprintf ( stdout, "\n" );
   fprintf ( stdout, " Global grid dimension = %d x %d x %d \n", params.nx*params.npx, params.ny*params.npy, params.nz*params.npz );
   fprintf ( stdout, "\n" );
#if defined _MG_MPI
   fprintf ( stdout, " Local grid dimension  = %d x %d x %d \n", params.nx, params.ny, params.nz );
   fprintf ( stdout, "\n" );
#endif
   fprintf ( stdout, " block size            = %d x %d x %d \n", params.blkxlen, params.blkylen, params.blkzlen );
   fprintf ( stdout, "\n" );

   fprintf ( stdout, " Number of variables = %d \n", params.numvars );
   fprintf ( stdout, "\n" );

   if ( params.extra_work_nvars != 0 ) {
      fprintf ( stdout, " Extra work: %d%% of variables to be operated on using %d additional variables. \n", 
                        params.extra_work_percent, params.extra_work_nvars );
   }
   else {
      fprintf ( stdout, " (No extra work injected.) \n" );
   }
   fprintf ( stdout, "\n" );

   fprintf ( stdout, " %d time steps to be executed for each of %d heat spikes. \n", 
             params.numtsteps, params.numspikes );
   fprintf ( stdout, "\n" );
#if defined _MG_MPI
   fprintf ( stdout, " %2.2e GBytes allocated per MPI rank, total %2.2e GBytes. \n", mem_allocated,  mem_allocated*numpes );
   fprintf ( stdout, "\n" );
#endif
#if defined _MG_OPENMP
   fprintf ( stdout, " %2.2e GBytes for each OpenMP thread. \n", mem_allocated / num_threads );
   fprintf ( stdout, "\n" );
#endif
#if defined _MG_SERIAL
   fprintf ( stdout, " %2.2e GBytes allocated. \n", mem_allocated );
   fprintf ( stdout, "\n" );
#endif

   fprintf ( stdout, " Begin program execution. \n" );
   fprintf ( stdout, " ========================================================================= \n" );

   return ( ierr );

} // End MG_Print_header.

//  ===================================================================================

int MG_Barrier ( )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0;       // Return status.

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_MPI
   ierr = CALL_MPI_Barrier ( MPI_COMM_MG );
#endif

   return ( ierr );

} // End MG_Barrier

//  ===================================================================================

void *MG_CALLOC ( int count, size_t size_of_count )
{
   // ---------------
   // Local Variables
   // ---------------

   void *ptr;

   // ---------------------
   // Executable statements
   // ---------------------

   //printf ( "****                    calloc len %d of size %d \n", count, (int)size_of_count );
   ptr = calloc( (size_t)count, size_of_count );
   //printf ( "****                    ptr = %p \n", ptr );
   if ( ptr == NULL )
      return ( ptr );

   memory_stats.numallocs++;
   memory_stats.count += count;
   memory_stats.bytes += count * (int)size_of_count;

#if defined MG_DEBUG || defined _MG_DEBUG_MEMORY
   fprintf ( stdout, "[pe %d] MG_CALLOC:%d (%d %d); total (%d,%e GBytes)\n",
             mgpp.mype, memory_stats.numallocs, count, count*(int)size_of_count,
             memory_stats.count, (double)(memory_stats.bytes)/GIGA );
#endif

   return ( ptr );
}

//  ===================================================================================

void *MG_DECALLOC ( void *ptr, int decount, size_t size_of_decount )
{
   // Free memory previously allocated using MG_CALLOC.

   // ---------------
   // Local Variables
   // ---------------

   // ---------------------
   // Executable statements
   // ---------------------

   free ( ptr );

   memory_stats.numdeallocs++;
   memory_stats.decount -= decount;
   memory_stats.debytes -= decount * (int)size_of_decount;

#if defined MG_DEBUG || defined _MG_DEBUG_MEMORY
   fprintf ( stdout, "[pe %d] MG_DECALLOC:%d (%d %d); total (%d,%e GBytes)\n",
             mgpp.mype, memory_stats.numallocs, count, count*(int)size_of_count,
             memory_stats.count, (double)(memory_stats.bytes)/GIGA );
#endif

   return ( ptr );
}

//  ===================================================================================

double *MG_DCALLOC_INIT ( int count )
{
   // To ensure timing arrays initialized to 0.

   // ---------------
   // Local Variables
   // ---------------

   int
      i;

   double 
      *ptr;

   // ---------------------
   // Executable statements
   // ---------------------

   //printf ( "****                    calloc len %d of size %d \n", count, (int)size_of_count );
   ptr = (double*)calloc( (size_t)count, sizeof(double) );
   //printf ( "****                    ptr = %p \n", ptr );
   if ( ptr == NULL )
      return ( ptr );

   for ( i=0; i<count; i++ ) {
      ptr[i] = 0.0;
   }

   return ( ptr );
}

//  ===================================================================================

int MG_IPOW ( int a, int b )
{
   // The pow() functions compute x raised to the power y.
   // math.h is broken on my Mac.

   int
      i,      // Counter.
      num;    // a^b.

   num = 1;
   for ( i=0; i<b; i++ ) {
      num *= a;
   }

   return ( num );
}

//  ===================================================================================
