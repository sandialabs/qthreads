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
// USA// Questions? Contact Richard F. Barrett (rfbarre@sandia.gov) or
//                    Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************

#include "mg_tp.h"

int MG_Flux_accumulate_2d5pt3d7pt ( InputParams params, StateVar *g, 
                                    MG_REAL *grid_vals, BlockInfo blk )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      gd,                // Shorthand.
      i, j, k,           // Counters
      ierr = 0,          // Return status
      thread_id;         // Shorthand.

   MG_REAL
      divisor;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   gd = params.ghostdepth;

   switch ( params.stencil )
   {
      case MG_STENCIL_2D5PT:

         divisor = FIVE;
         break;

      case MG_STENCIL_3D7PT:
       
         divisor = SEVEN;
         break;

      default:
         MG_Assert ( -1, "MG_Flux_accumulate_2d5pt3d7pt: Unknown stencil." );

   } // End switch ( params.stencil )

   if ( blk.bc[SOUTH] == 1 ) {

      // South boundary

      j = gd;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   }
   
   if ( blk.bc[NORTH] == 1 ) {

      // North boundary
   
      j = params.ny; // blk.yend;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   }

   if ( blk.bc[WEST] == 1 ) {
 
      // West boundary
   
      i = gd;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   }

   if ( blk.bc[EAST] == 1 ) {
 
      // East boundary
   
      i = blk.xend;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   } 

   if ( blk.bc[FRONT] == 1 ) {

      // Front boundary
   
      k = gd;
      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   }

   if ( blk.bc[BACK] == 1 ) {
 
      // Back boundary
   
      k = blk.zend;
      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] += grid_vals(i,j,k) / divisor;
         }
      }
   }
   
#if defined _MG_DEBUG
   printf ( "[pe %d] g->thr_flux[%d] = %2.2e TOTAL \n", mgpp.mype, thread_id, g->thr_flux[thread_id] );
   //printf ( "========================================= \n" );
#endif

   //}}

   return ( ierr );
   
} // end MG_Flux_accumulate_2d5pt3d7pt
