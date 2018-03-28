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

int MG_Stencil ( InputParams params, StateVar **g, BlockInfo blk, int ivar )
{
   // Gateway to the selected stencil. 
   // FIXME rfbarre: this could be changed to use function pointers.

   // Control function for stencils. 
   // 
   // 1) Inter-process boundary exchange: MG_Boundary_exchange
   // 2) Apply/manage boundary conditions: MG_Boundary_conditions
   // 3) Apply stencil computation: MG_Stencil_xDypt.
   // -----------------------------------------------------------------------------------

   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,            // Return status
      thread_id;

   MG_REAL
      *grid_in,
      *grid_out;

   double
      time_start;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   switch ( g[ivar]->toggle ) 
   {
      case ( 0 ) : 

         grid_in  = g[ivar]->values1;
         grid_out = g[ivar]->values2;

         break;

      case ( 1 ) : 

         grid_in  = g[ivar]->values2;
         grid_out = g[ivar]->values1;

         break;

      default:
         ierr = -1;
         MG_Assert ( !ierr, "MG_Stencil: g->toggle not 0 or 1" );

   } // End switch ( g->toggle )

#if defined _MG_MPI 

   // Boundary exhange.

   MG_Time_start(time_start);

   ierr = MG_Boundary_exchange ( params, grid_in, blk, ivar );
   MG_Assert ( !ierr, "MG_Stencil:MG_Boundary_exchange" );

   MG_Time_accum(time_start,timings.bex[thread_id]);

#endif // _MG_MPI 

   // Apply boundary condition.

   MG_Time_start(time_start);

   ierr = MG_Boundary_conditions ( params, g[ivar], grid_in, blk );
   MG_Assert ( !ierr, "MG_Stencil:MG_Boundary_conditions" );

   MG_Time_accum(time_start,timings.bc_comp[thread_id]);

   // Apply stencil.

   MG_Time_start(time_start);

   switch ( params.stencil )
   {
      case MG_STENCIL_2D5PT:

         ierr = MG_Stencil_2d5pt ( params, grid_in, grid_out, blk );
         MG_Assert ( !ierr, "MG_Stencil:MG_Stencil_2d5pt" );
         break;

      case MG_STENCIL_2D9PT:

         ierr = MG_Stencil_2d9pt ( params, grid_in, grid_out, blk );
         MG_Assert ( !ierr, "MG_Stencil:MG_Stencil_2d9pt" );
         break;

      case MG_STENCIL_3D7PT:

         ierr = MG_Stencil_3d7pt ( params, grid_in, grid_out, blk );
         MG_Assert ( !ierr, "MG_Stencil:MG_Stencil_3d7pt" );
         break;

      case MG_STENCIL_3D27PT:

         ierr = MG_Stencil_3d27pt ( params, grid_in, grid_out, blk );
         MG_Assert ( !ierr, "MG_Stencil:MG_Stencil_3d27pt" );
         break;

      default:
         ierr = -1;
         MG_Assert ( !ierr, "MG_Stencil: Unknown stencil." );

   } // End switch ( params.stencil )

   MG_Assert ( !ierr, "MG_Stencil" );

   if ( g[ivar]->do_more_work_nvars != 0 ) {
      MG_Fake_work ( params, g, blk, ivar );
   }

   MG_Time_accum(time_start,timings.stencil[thread_id]);

   return ( ierr );
}

// ======================================================================================

int MG_Stencil_2d5pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk )
{
   int
      i, j, k,           // Grid sweeping counters
      ierr = 0;          // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_OPENMP
#pragma omp parallel for private(k, j, i)
#endif
   for ( k=blk.zstart; k<=blk.zend; k++ ) {
   for ( j=blk.ystart; j<=blk.yend; j++ ) {
   for ( i=blk.xstart; i<=blk.xend; i++ ) {

      grid_out(i,j,k) = (
                              grid_in(i-1,j,k) + 
           grid_in(i,j-1,k) + grid_in(i,  j,k) + grid_in(i,j+1,k) +
                              grid_in(i+1,j,k)
        )
        / FIVE;
   }}}

   return ( ierr );
}

// ======================================================================================

int MG_Stencil_2d9pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk )
{
   int
      i, j, k,           // Grid sweeping counters
      ierr = 0;          // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_OPENMP
#pragma omp parallel for private(k, j, i)
#endif
   for ( k=blk.zstart; k<=blk.zend; k++ ) {
   for ( j=blk.ystart; j<=blk.yend; j++ ) {
   for ( i=blk.xstart; i<=blk.xend; i++ ) {
 
      grid_out(i,j,k) = (
                               
         grid_in(i-1,j-1,k) + grid_in(i-1,j,k) + grid_in(i-1,j+1,k) +
         grid_in(i,  j-1,k) + grid_in(i,  j,k) + grid_in(i,  j+1,k) +
         grid_in(i+1,j-1,k) + grid_in(i+1,j,k) + grid_in(i+1,j+1,k)
                     
        )
            
        / NINE;
            
#if defined _MG_DEBUG
        printf ( "grid_out(%d,%d,%d)=%2.2e \n", i,j,k,grid_out(i,j,k) );
#endif
   }}}

   return ( ierr );
}

// ======================================================================================

int MG_Stencil_3d7pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk )
{
   int
      i, j, k,           // grid sweep counters
      ierr = 0;          // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_OPENMP
#pragma omp parallel for private(k, j, i)
#endif

   //printf ( " In MG_Stencil_3d7pt: grid_in %x grid_out(work) %x\n", grid_in, grid_out );

   for ( k=blk.zstart; k<=blk.zend; k++ ) {
   for ( j=blk.ystart; j<=blk.yend; j++ ) {
   for ( i=blk.xstart; i<=blk.xend; i++ ) {

      //printf ( "[pe %d] i,j,k=(%d,%d,%d)=%e \n", mgpp.mype, i, j, k, grid_in(i,j,k) );

      grid_out(i,j,k) = (
                                 grid_in(i,j,k-1) +

                              grid_in(i-1,j,k) +
           grid_in(i,j-1,k) + grid_in(i,  j,k) + grid_in(i,j+1,k) +
                              grid_in(i+1,j,k) +

                           grid_in(i,j,k+1)   
        )

        / SEVEN;

#if defined _MG_DEBUG
        printf ( "grid_out(%d,%d,%d)=%e \n", i,j,k,grid_out(i,j,k) );
#endif
   }}}

   return ( ierr );
}

// ======================================================================================

int MG_Stencil_3d27pt ( InputParams params, MG_REAL *grid_in, MG_REAL *grid_out, BlockInfo blk )
{
   int
      i, j, k,           // Grid sweeping counters
      ierr = 0;          // Return status
#if defined _MG_DEBUG
   double
      lsum = 0.0;
#endif

   // ---------------------
   // Executable Statements
   // ---------------------

#if defined _MG_OPENMP
#pragma omp parallel for private(k, j, i)
#endif
   for ( k=blk.zstart; k<=blk.zend; k++ ) {
   for ( j=blk.ystart; j<=blk.yend; j++ ) {
   for ( i=blk.xstart; i<=blk.xend; i++ ) {

      grid_out(i,j,k) = (

         grid_in(i-1,j-1,k-1) + grid_in(i-1,j,k-1) + grid_in(i-1,j+1,k-1) +
         grid_in(i,  j-1,k-1) + grid_in(i,  j,k-1) + grid_in(i,  j+1,k-1) +
         grid_in(i+1,j-1,k-1) + grid_in(i+1,j,k-1) + grid_in(i+1,j+1,k-1) +

         grid_in(i-1,j-1,k)   + grid_in(i-1,j,k)   + grid_in(i-1,j+1,k) +
         grid_in(i,  j-1,k)   + grid_in(i,  j,k)   + grid_in(i,  j+1,k) +
         grid_in(i+1,j-1,k)   + grid_in(i+1,j,k)   + grid_in(i+1,j+1,k) +

         grid_in(i-1,j-1,k+1) + grid_in(i-1,j,k+1) + grid_in(i-1,j+1,k+1) +
         grid_in(i,  j-1,k+1) + grid_in(i,  j,k+1) + grid_in(i,  j+1,k+1) +
         grid_in(i+1,j-1,k+1) + grid_in(i+1,j,k+1) + grid_in(i+1,j+1,k+1)

        )

        / TWENTY_SEVEN;

#if defined _MG_DEBUG
         lsum += grid_out(i,j,k);
         printf ( "grid_out(%d,%d,%d)=%8.4e; lsum=%8.4e \n", i,j,k,grid_out(i,j,k),lsum );
#endif
   }}}

   return ( ierr );
}

// ======================================================================================
