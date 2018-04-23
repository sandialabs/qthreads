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

int MG_Flux_accumulate_2d9pt ( InputParams params, StateVar *g,
                               MG_REAL *grid_vals, BlockInfo blk )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      gd,                // Shorthand.
      i, j, k,           // Counters
      ierr = 0,          // Return status
      nx, ny,            // Shorthand.
      thread_id;         // Shorthand.

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   gd = params.ghostdepth;

   nx = params.nx;
   ny = params.ny;

   //printf ( "blk.bc = %d %d %d %d %d %d \n", blk.bc[SOUTH], blk.bc[NORTH], blk.bc[EAST], blk.bc[WEST], blk.bc[BACK], blk.bc[FRONT] );

   if ( blk.bc[SOUTH] == 1 ) {

      // South boundary

      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                           
                 ( grid_vals(i-1,1,k) + grid_vals(i,1,k) + grid_vals(i+1,1,k) ) / NINE;
            //printf ( "(%d,%d,%d) = %2.2e \n", i, j, k, grid_vals(i-1,1,k) + grid_vals(i,1,k) + grid_vals(i+1,1,k) );
         }
         // Corner cases
         if ( params.mypx == 0 ) {
            if ( blk.xstart == 1 ) {
               g->thr_flux[thread_id] += grid_vals(1,1,k) / NINE;
               //printf ( "(%d,%d,%d) = %2.2e \n", 1, 1, k, grid_vals(1,1,k) );
            }
         }
         if ( params.mypx == ( params.npx - 1) ) {
            if ( blk.xend == nx ) {
               g->thr_flux[thread_id] += grid_vals(nx,1,k) / NINE;
               //printf ( "(%d,%d,%d) = %2.2e \n", nx, 1, k, grid_vals(nx,1,k) );
            }
         }
      }
   }

   //printf( "SOUTH: g->thr_flux[thread_id] = %2.2e \n", g->thr_flux[thread_id] );

   if ( blk.bc[NORTH] == 1 ) {

      // North boundary

      //printf ( "NORTH \n");
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                                    
                 ( grid_vals(i-1,ny,k) + grid_vals(i,ny,k) + grid_vals(i+1,ny,k) ) / NINE;
            //printf ( "(%d,%d,%d) = %2.2e \n", i, j, k, grid_vals(i-1,ny,k) + grid_vals(i,ny,k) + grid_vals(i+1,ny,k) );
         }
         // Corner cases
         if ( params.mypx == 0 ) {
            if ( blk.xstart == 1 ) {
               g->thr_flux[thread_id] += grid_vals(1,ny,k) / NINE;
               //printf ( "(%d,%d,%d) = %2.2e \n", 1, ny, k, grid_vals(1,ny,k) );
            }
         }
         if ( params.mypx == ( params.npx - 1 ) ) {
            if ( blk.xend == nx ) {
               g->thr_flux[thread_id] += grid_vals(nx,ny,k) / NINE;
               //printf ( "(%d,%d,%d) = %2.2e \n", nx, ny, k, grid_vals(nx,ny,k) );
            }
         }
      }
   }
 
   //printf( "NORTH: g->thr_flux[thread_id] = %2.2e \n", g->thr_flux[thread_id] );

   if ( blk.bc[WEST] == 1 ) {

      // West boundary

      //printf ( "WEST \n");
      i = gd;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] +=                           
               ( grid_vals(i,j-1,k) + grid_vals(i,j,k) + grid_vals(i,j+1,k) ) / NINE;
            //printf ( "(%d,%d,%d) = %2.2e \n", i, j, k, grid_vals(i,j-1,k) + grid_vals(i,j,k) + grid_vals(1,j+1,k) );
         }
      }
   }
   //printf( "WEST: g->thr_flux[thread_id] = %2.2e \n", g->thr_flux[thread_id] );

   if ( blk.bc[EAST] == 1 ) {

      // East boundary

      //printf ( "EAST \n");
      i = blk.xend;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] +=                                    
                 ( grid_vals(nx,j-1,k) + grid_vals(nx,j,k) + grid_vals(nx,j+1,k) ) / NINE;
            //printf ( "(%d,%d,%d) = %2.2e \n", nx, j, k, grid_vals(nx,j-1,k) + grid_vals(nx,j,k) + grid_vals(nx,j+1,k) );
         }
      }
   }
   //printf( "EAST: g->thr_flux[thread_id] = %2.2e \n", g->thr_flux[thread_id] );

   return ( ierr );

} // End MG_Flux_accumulate_2d9pt
