// ************************************************************************
//
//            minighost: stencil computations with boundary exchange.
//              copyright (2012) sandia corporation
//
// under terms of contract de-ac04-94al85000, there is a non-exclusive
// license for use of this work by or on behalf of the u.s. government.
//
// this library is free software; you can redistribute it and/or modify
// it under the terms of the gnu lesser general public license as
// published by the free software foundation; either version 2.1 of the
// license, or (at your option) any later version.
//
// this library is distributed in the hope that it will be useful, but
// without any warranty; without even the implied warranty of
// merchantability or fitness for a particular purpose.  see the gnu
// lesser general public license for more details.
//
// you should have received a copy of the gnu lesser general public
// license along with this library; if not, write to the free software
// foundation, inc., 59 temple place, suite 330, boston, ma 02111-1307
// usa
// questions? contact richard f. barrett (rfbarre@sandia.gov) or
//                 michael a. heroux (maherou@sandia.gov)
//
// ************************************************************************

#include "mg_tp.h"

int MG_Flux_accumulate_3d27pt ( InputParams params, StateVar *g, 
                                MG_REAL *grid_vals, BlockInfo blk )
{
   // ------------------
   // Local Declarations
   // ------------------

   int       
      gd,                // Shorthand.
      i, j, k,           // Counters
      ierr = 0,          // Return status
      nx, ny, nz,        // Shorthand.
      thread_id;         // Shorthand.

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   gd = params.ghostdepth;

   nx = params.nx;
   ny = params.ny;
   nz = params.nz;

   //int m;
   //for ( m=0; m<mgpp.numpes; m++ ) {
   //MG_Barrier ( );
   //if ( mgpp.mype==m ) {
   //printf ( "\n[pe %d] blk.bc = %d %d %d %d %d %d \n\n", mgpp.mype, blk.bc[NORTH], blk.bc[SOUTH], blk.bc[EAST], blk.bc[WEST], blk.bc[FRONT], blk.bc[BACK] );

   // South boundary

   if ( blk.bc[SOUTHWEST] == 1 ) {

      //printf ( "HERE: %d %d \n", blk.zstart, blk.zend );
      //for ( j=blk.zstart; k<=blk.zend; k++ ) {
         //g->thr_flux[thread_id] +=
            //( grid_vals(1,1,k-1) + grid_vals(1,1,k) + grid_vals(1,1,k+1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] SOUTHWEST BC (1, 1, %d) = %4.2e \n", mgpp.mype, k,
                   //grid_vals(1,1,k-1) + grid_vals(1,1,k) + grid_vals(1,1,k+1) );
      //}
      k = 1;
      g->thr_flux[thread_id] += 
         ( grid_vals(1,1,k-1) + grid_vals(1,1,k) + grid_vals(1,1,k+1) ) / TWENTY_SEVEN;
      //printf ( "[pe %d] SOUTHWEST BC (1, 1, %d) = %4.2e \n", mgpp.mype, k,
                //grid_vals(1,1,k-1) + grid_vals(1,1,k) + grid_vals(1,1,k+1) );
   }
   if ( blk.bc[SOUTH] == 1 ) {

      j = gd;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                                   
              ( grid_vals(i-1,j,k-1) + grid_vals(i,j,k-1) + grid_vals(i+1,j,k-1) + 
                grid_vals(i-1,j,k  ) + grid_vals(i,j,k  ) + grid_vals(i+1,j,k  ) + 
                grid_vals(i-1,j,k+1) + grid_vals(i,j,k+1) + grid_vals(i+1,j,k+1) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] SOUTH BC (%d, %d, %d) = %4.2e \n", mgpp.mype, i, j, k,
                //grid_vals(i-1,j,k-1) + grid_vals(i,j,k-1) + grid_vals(i+1,j,k-1) +
                //grid_vals(i-1,j,k  ) + grid_vals(i,j,k  ) + grid_vals(i+1,j,k  ) +
                //grid_vals(i-1,j,k+1) + grid_vals(i,j,k+1) + grid_vals(i+1,j,k+1) );
         }
      }
   }
   if ( blk.bc[SOUTHEAST] == 1 ) {

      //for ( k=blk.zstart; k<=blk.zend; k++ ) {
         //g->thr_flux[thread_id] +=
            //( grid_vals(nx,1,k-1) + grid_vals(nx,1,k) + grid_vals(nx,1,k+1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] SOUTHEAST BC (%d, 1, %d) = %4.2e \n", mgpp.mype, nx, k,                
                  //grid_vals(nx,1,k-1) + grid_vals(nx,1,k) + grid_vals(nx,1,k+1) );
      //}
      k = blk.zend;
      g->thr_flux[thread_id] +=
         ( grid_vals(nx,1,k-1) + grid_vals(nx,1,k) + grid_vals(nx,1,k+1) ) / TWENTY_SEVEN;
      //printf ( "[pe %d] SOUTHEAST BC (nx, 1, %d) = %4.2e \n", mgpp.mype, k,
                //grid_vals(nx,1,k-1) + grid_vals(nx,1,k) + grid_vals(nx,1,k+1) );
   }

   // North boundary

   if ( blk.bc[NORTHWEST] == 1 ) {

      //for ( k=blk.zstart; k<=blk.zend; k++ ) {
         //g->thr_flux[thread_id] +=
            //( grid_vals(1,ny,k-1) + grid_vals(1,ny,k) + grid_vals(1,ny,k+1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] NORTHWEST BC (1, %d, %d) = %4.2e \n", mgpp.mype, ny, k,
                  //grid_vals(1,ny,k-1) + grid_vals(1,ny,k) + grid_vals(1,ny,k+1) );
      //}
      k=1;
      g->thr_flux[thread_id] +=
         ( grid_vals(1,ny,k-1) + grid_vals(1,ny,k) + grid_vals(1,ny,k+1) ) / TWENTY_SEVEN;
      //printf ( "[pe %d] NORTHWEST BC (1, %d, %d) = %4.2e \n", mgpp.mype, ny, k,
               //grid_vals(1,ny,k-1) + grid_vals(1,ny,k) + grid_vals(1,ny,k+1) );

   }
   if ( blk.bc[NORTH] == 1 ) {

      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                                          
              ( grid_vals(i-1,ny,k-1) + grid_vals(i,ny,k-1) + grid_vals(i+1,ny,k-1) +  
                grid_vals(i-1,ny,k  ) + grid_vals(i,ny,k  ) + grid_vals(i+1,ny,k  ) +   
                grid_vals(i-1,ny,k+1) + grid_vals(i,ny,k+1) + grid_vals(i+1,ny,k+1) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] NORTH BC (%d, %d, %d) = %4.2e \n", mgpp.mype, i, ny, k,
                //grid_vals(i-1,ny,k-1) + grid_vals(i,ny,k-1) + grid_vals(i+1,ny,k-1) +
                //grid_vals(i-1,ny,k  ) + grid_vals(i,ny,k  ) + grid_vals(i+1,ny,k  ) +
                //grid_vals(i-1,ny,k+1) + grid_vals(i,ny,k+1) + grid_vals(i+1,ny,k+1) );
         }
      }
   }
   if ( blk.bc[NORTHEAST] == 1 ) {

      //for ( k=blk.zstart; k<=blk.zend; k++ ) {
         //g->thr_flux[thread_id] +=
            //( grid_vals(nx,ny,k-1) + grid_vals(nx,ny,k) + grid_vals(nx,ny,k+1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] NORTHEAST BC (%d, %d, %d) = %4.2e \n", mgpp.mype, nx, ny, k,
                  //grid_vals(nx,ny,k-1) + grid_vals(nx,ny,k) + grid_vals(nx,ny,k+1) );
      //}
      k=blk.zend;
      g->thr_flux[thread_id] +=
         ( grid_vals(nx,ny,k-1) + grid_vals(nx,ny,k) + grid_vals(nx,ny,k+1) ) / TWENTY_SEVEN;
      //printf ( "[pe %d] NORTHEAST BC (%d, %d, %d) = %4.2e \n", mgpp.mype, nx, ny, k,
               //grid_vals(nx,ny,k-1) + grid_vals(nx,ny,k) + grid_vals(nx,ny,k+1) );

   }

   if ( blk.bc[WEST] == 1 ) {

      // West boundary

      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] +=                                 
               ( grid_vals(1,j-1,k-1) + grid_vals(1,j,k-1) + grid_vals(1,j+1,k-1) + 
                 grid_vals(1,j-1,k  ) + grid_vals(1,j,k  ) + grid_vals(1,j+1,k  ) + 
                 grid_vals(1,j-1,k+1) + grid_vals(1,j,k+1) + grid_vals(1,j+1,k+1) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] WEST BC (1, %d, %d) = %4.2e \n", mgpp.mype, j, k,
                 //grid_vals(1,j-1,k-1) + grid_vals(1,j,k-1) + grid_vals(1,j+1,k-1) +
                 //grid_vals(1,j-1,k  ) + grid_vals(1,j,k  ) + grid_vals(1,j+1,k  ) +
                 //grid_vals(1,j-1,k+1) + grid_vals(1,j,k+1) + grid_vals(1,j+1,k+1) );
         }
      }
   }

   if ( blk.bc[EAST] == 1 ) {

      // East boundary

      for ( k=blk.zstart; k<=blk.zend; k++ ) {
            for ( j=blk.ystart; j<=blk.yend; j++ ) {
            g->thr_flux[thread_id] +=                                    
               ( grid_vals(nx,j-1,k-1) + grid_vals(nx,j,k-1) + grid_vals(nx,j+1,k-1) + 
                 grid_vals(nx,j-1,k  ) + grid_vals(nx,j,k  ) + grid_vals(nx,j+1,k  ) + 
                 grid_vals(nx,j-1,k+1) + grid_vals(nx,j,k+1) + grid_vals(nx,j+1,k+1) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] EAST BC (%d, %d, %d) = %4.2e \n", mgpp.mype, nx, j, k,
                     //grid_vals(nx,j-1,k-1) + grid_vals(nx,j,k-1) + grid_vals(nx,j+1,k-1) +
                     //grid_vals(nx,j-1,k  ) + grid_vals(nx,j,k  ) + grid_vals(nx,j+1,k  ) +
                     //grid_vals(nx,j-1,k+1) + grid_vals(nx,j,k+1) + grid_vals(nx,j+1,k+1) );
         }
      }
   }

   // Front boundary

   if ( blk.bc[FRONT] == 1 ) {

      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                                 
               ( grid_vals(i-1,j-1,1) + grid_vals(i-1,j,1) + grid_vals(i-1,j+1,1) + 
                 grid_vals(i,  j-1,1) + grid_vals(i,  j,1) + grid_vals(i,  j+1,1) + 
                 grid_vals(i+1,j-1,1) + grid_vals(i+1,j,1) + grid_vals(i+1,j+1,1) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] FRONT BC (%d, %d, 1) = %4.2e \n", mgpp.mype, i, j,
                      //grid_vals(i-1,j-1,1) + grid_vals(i-1,j,1) + grid_vals(i-1,j+1,1) +
                 //grid_vals(i,  j-1,1) + grid_vals(i,  j,1) + grid_vals(i,  j+1,1) +
                 //grid_vals(i+1,j-1,1) + grid_vals(i+1,j,1) + grid_vals(i+1,j+1,1) );
         }
      }
   }
   if ( blk.bc[FRONT_WEST] == 1 ) {

      if ( blk.ystart == gd ) {
         g->thr_flux[thread_id] += ( ( grid_vals(1,1,1)) / TWENTY_SEVEN );
            //printf ( "[pe %d] FRONT_WEST BC (1, 1, 1) = %4.2e \n", mgpp.mype,
                      //grid_vals(1,1,1) );
      }
      for ( j=blk.ystart; j<=blk.yend; j++ ) { // x-y plane for x=1, z=1
         g->thr_flux[thread_id] +=
            ( grid_vals(1,j-1,1) + grid_vals(1,j,1) + grid_vals(1,j+1,1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] FRONT_WEST BC (1, %d, 1) = %4.2e \n", mgpp.mype, j,
                  //grid_vals(1,j-1,1) + grid_vals(1,j,1) + grid_vals(1,j+1,1) );
      }
      if ( blk.yend == params.ny+gd-1 ) {
         g->thr_flux[thread_id] += ( grid_vals(1,ny,1) / TWENTY_SEVEN );
            //printf ( "[pe %d] FRONT_WEST BC (1, %d, 1) = %4.2e \n", mgpp.mype, ny,
                     //grid_vals(1,ny,1) );
      }
   }
   if ( blk.bc[FRONT_EAST] == 1 ) {

      if ( blk.ystart == gd ) {
         g->thr_flux[thread_id] += ( grid_vals(nx,1,1) / TWENTY_SEVEN );
            //printf ( "[pe %d] FRONT_EAST BC (%d, 1, 1) = %4.2e \n", mgpp.mype, nx,
                     //grid_vals(nx,1,1) );
      }
      for ( j=blk.ystart; j<=blk.yend; j++ ) { // x-y plane for x=nx, z=1
         g->thr_flux[thread_id] +=
            ( grid_vals(nx,j-1,1) + grid_vals(nx,j,1) + grid_vals(nx,j+1,1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] FRONT_EAST BC (%d, %d, 1) = %4.2e \n", mgpp.mype, nx, j,
                  //grid_vals(nx,j-1,1) + grid_vals(nx,j,1) + grid_vals(nx,j+1,1) );
      }
      if ( blk.yend == params.ny+gd-1 ) {
         g->thr_flux[thread_id] += ( grid_vals(nx,ny,1) / TWENTY_SEVEN );
            //printf ( "[pe %d] FRONT_EAST BC (%d, %d, 1) = %4.2e \n", mgpp.mype, nx, ny,
                     //grid_vals(nx,ny,1));
      }
   }
   if ( blk.bc[FRONT_SOUTH] == 1 ) {

      for ( i=blk.xstart; i<=blk.xend; i++ ) {
         g->thr_flux[thread_id] +=
            ( grid_vals(i-1,1,1) + grid_vals(i,1,1) + grid_vals(i+1,1,1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] FRONT_SOUTH BC (%d, 1, 1) = %4.2e \n", mgpp.mype, i, 
                   //grid_vals(i-1,1,1) + grid_vals(i,1,1) + grid_vals(i+1,1,1) );
      }
   }
   if ( blk.bc[FRONT_NORTH] == 1 ) {

      for ( i=blk.xstart; i<=blk.xend; i++ ) {
         g->thr_flux[thread_id] +=
            ( grid_vals(i-1,ny,1) + grid_vals(i,ny,1) + grid_vals(i+1,ny,1) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] FRONT_NORTH BC (%d, %d, 1) = %4.2e \n", mgpp.mype, i, ny,
                  //grid_vals(i-1,ny,1) + grid_vals(i,ny,1) + grid_vals(i+1,ny,1) );
      }
   }

   if ( blk.bc[BACK] == 1 ) {

      // Back boundary

      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            g->thr_flux[thread_id] +=                                 
               ( grid_vals(i-1,j-1,nz) + grid_vals(i-1,j,nz) + grid_vals(i-1,j+1,nz) + 
                 grid_vals(i,  j-1,nz) + grid_vals(i,  j,nz) + grid_vals(i,  j+1,nz) + 
                 grid_vals(i+1,j-1,nz) + grid_vals(i+1,j,nz) + grid_vals(i+1,j+1,nz) ) / TWENTY_SEVEN;
            //printf ( "[pe %d] BACK BC (%d, %d, %d) = %4.2e \n", mgpp.mype, i, j, nz,
                     //grid_vals(i-1,j-1,nz) + grid_vals(i-1,j,nz) + grid_vals(i-1,j+1,nz) +
                 //grid_vals(i,  j-1,nz) + grid_vals(i,  j,nz) + grid_vals(i,  j+1,nz) +
                 //grid_vals(i+1,j-1,nz) + grid_vals(i+1,j,nz) + grid_vals(i+1,j+1,nz) );
         }
      }
   }
   if ( blk.bc[BACK_WEST] == 1 ) {

      if ( blk.ystart == gd ) {
         g->thr_flux[thread_id] += ( grid_vals(1,1,nz) / TWENTY_SEVEN );
            //printf ( "[pe %d] BACK WEST BC (1, 1, %d) = %4.2e \n", mgpp.mype, nz,
            //grid_vals(1,1,nz) );
      }
      for ( j=blk.ystart; j<=blk.yend; j++ ) { // x-y plane for x=1, z=nz
         g->thr_flux[thread_id] +=
            ( grid_vals(1,j-1,nz) + grid_vals(1,j,nz) + grid_vals(1,j+1,nz) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] BACK WEST BC (1, %d, %d) = %4.2e \n", mgpp.mype, j, nz,
                  //grid_vals(1,j-1,nz) + grid_vals(1,j,nz) + grid_vals(1,j+1,nz) );
      }
      if ( blk.yend == params.ny+gd-1 ) {
         g->thr_flux[thread_id] +=
            ( grid_vals(1,ny,nz) / TWENTY_SEVEN );
         //printf ( "[pe %d] BACK WEST BC (1, %d, %d) = %4.2e \n", mgpp.mype, ny, nz,
                  //grid_vals(1,ny,nz));
      }
   }
   if ( blk.bc[BACK_EAST] == 1 ) {

      if ( blk.ystart == gd ) {
         g->thr_flux[thread_id] += ( grid_vals(nx,1,nz) / TWENTY_SEVEN );
         //printf ( "[pe %d] BACK_EAST BC (%d, 1, %d) = %4.2e \n", mgpp.mype, nx, nz,
         //grid_vals(nx,1,nz) );
      }
      for ( j=blk.ystart; j<=blk.yend; j++ ) { // x-y plane for x=nx, z=nz
         g->thr_flux[thread_id] +=
            ( grid_vals(nx,j-1,nz) + grid_vals(nx,j,nz) + grid_vals(nx,j+1,nz) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] BACK EAST BC (%d, %d, %d) = %4.2e \n", mgpp.mype, nx,j,nz,
                  //grid_vals(nx,j-1,nz) + grid_vals(nx,j,nz) + grid_vals(nx,j+1,nz) );
      }
      if ( blk.ystart == params.ny+gd-1 ) {
         g->thr_flux[thread_id] += ( grid_vals(nx,ny,nz) / TWENTY_SEVEN );
         //printf ( "[pe %d] BACK EAST BC (%d, %d, %d) = %4.2e \n", mgpp.mype, nx,ny,nz,
                  //grid_vals(nx,ny,nz) );
      }
   }
   if ( blk.bc[BACK_SOUTH] == 1 ) {

      for ( i=blk.xstart; i<=blk.xend; i++ ) {
         g->thr_flux[thread_id] +=
            ( grid_vals(i-1,1,nz) + grid_vals(i,1,nz) + grid_vals(i+1,1,nz) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] BACK SOUTH BC (%d, 1, %d) = %4.2e \n", mgpp.mype, i, nz,
                  //grid_vals(i-1,1,nz) + grid_vals(i,1,nz) + grid_vals(i+1,1,nz) );
      }
   }
   if ( blk.bc[BACK_NORTH] == 1 ) {

      for ( i=blk.xstart; i<=blk.xend; i++ ) {
         g->thr_flux[thread_id] +=
            ( grid_vals(i-1,ny,nz) + grid_vals(i,ny,nz) + grid_vals(i+1,ny,nz) ) / TWENTY_SEVEN;
         //printf ( "[pe %d] BACK_NORTH BC (%d, %d, %d) = %4.2e \n", mgpp.mype, i,ny,nz,
                   //grid_vals(i-1,ny,nz) + grid_vals(i,ny,nz) + grid_vals(i+1,ny,nz) );
      }
   }
   //}} // Debugging loop.

   return ( ierr );
   
} // End MG_Flux_accumulate_3d27pt
