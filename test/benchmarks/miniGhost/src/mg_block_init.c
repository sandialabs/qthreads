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

int MG_Block_init ( InputParams *params, BlockInfo **blk )
{
   // -------
   // Purpose
   // -------
   // 
   // Call the selected block configuration scheme.

   // ------------------
   // Local Declarations
   // ------------------

   int
      blknum,             // Counter.
      blkpx,
      blkpy,
      blkpz,
      gd,                 // Shorthand.
      i, j, k,            // Counters
      ierr = 0,           // Return status.
      mindim,             // Minimum dimension of a block's halo.
      rem_x,              // Remainder of domain in x direction.
      rem_y,              // Remainder of domain in y direction.
      rem_z,              // Remainder of domain in z direction.
      xstart, xend,       // Temporary counters.
      ystart, yend,
      zstart, zend;

   // ---------------------
   // Executable statements 
   // ---------------------

   // Define blocks. Re-ordering option after that.

   params->nblks_xdir = params->nx / params->blkxlen;
   rem_x = params->nx % params->blkxlen;
   if ( rem_x != 0 ) {
      params->nblks_xdir++;
   }
   params->nblks_ydir = params->ny / params->blkylen;
   rem_y = params->ny % params->blkylen;
   if ( rem_y != 0 ) {
      params->nblks_ydir++;
   }
   params->nblks_zdir = params->nz / params->blkzlen;
   rem_z = params->nz % params->blkzlen;
   if ( rem_z != 0 ) {
      params->nblks_zdir++;
   }
   params->numblks = params->nblks_xdir * params->nblks_ydir * params->nblks_zdir;

   blknum = 0; 
   blkpx = 0;
   blkpy = 0;
   blkpz = 0;
   zstart = 1; zend = params->blkzlen;
   for ( k=0; k<params->nblks_zdir; k++ ) {
      
      ystart = 1; yend = params->blkylen;
      for ( j=0; j<params->nblks_ydir; j++ ) {
         xstart = 1; xend = params->blkxlen;
         for ( i=0; i<params->nblks_xdir; i++ ) {

            blk[blknum] = (BlockInfo*)MG_CALLOC ( 1, sizeof(BlockInfo) );
            MG_Assert ( blk[blknum] != NULL, "main: Allocation of *blk" );
            if ( blk[i] == NULL ) {
               fprintf ( stderr, "Allocation of blk[%d] of size %d failed \n",
                         i, (int)sizeof(BlockInfo) );
               ierr = -1;
               MG_Assert ( !ierr, "MG_Block_init: Allocation of arrays blk[i]" );
            }
            blk[blknum]->id = blknum;
            blk[blknum]->mypx = blkpx;
            blk[blknum]->mypy = blkpy;
            blk[blknum]->mypz = blkpz;

            // Set offsets into variables.
            blk[blknum]->xstart = xstart;
            blk[blknum]->xend   = xend;
            blk[blknum]->ystart = ystart;
            blk[blknum]->yend   = yend;
            blk[blknum]->zstart = zstart;
            blk[blknum]->zend   = zend;

            blk[blknum]->info = 1;

            xstart = xend + 1;
            xend = xstart + params->blkxlen - 1;

            blknum++;
            blkpx++;
         }
         ystart = yend + 1;
         yend = ystart + params->blkylen - 1;
         blkpx = 0;
         blkpy++;
      }
      zstart = zend + 1;
      zend = zstart + params->blkzlen - 1;
      blkpy = 0;
      blkpz++;
   }

#if defined _MG_MPI

   gd = params->ghostdepth;

   // Over-ride default 
   // Determine maximum face size
   mindim = MG_Min3i ( params->blkxlen,
                       params->blkylen,
                       params->blkzlen );
   if ( mindim == params->blkxlen )
      max_msg_count = ( params->blkylen + (2*gd) ) * ( params->blkzlen + (2*gd) );
   else if ( mindim == params->blkylen )
      max_msg_count = ( params->blkxlen + (2*gd) ) * ( params->blkzlen + (2*gd) );
   else
      max_msg_count = ( params->blkxlen + (2*gd) ) * ( params->blkylen + (2*gd) );

   // Determine if block participating in computation of physical bounary conditions.

#endif

   ierr = MG_Block_set_neigh ( params, blk );
   MG_Assert ( !ierr, "MG_Block_set_neigh" );

   switch ( params->blkorder ) {

      case ( MG_BLOCK_ORDER_CART ) : // This is the default ordering, configured above.

         break;

      case ( MG_BLOCK_ORDER_MTA )  : // 

         ierr = MG_Block_def_mta ( params, blk );
         MG_Assert ( !ierr, "MG_Block_def_mta" );

         break;

      case ( MG_BLOCK_ORDER_RANDOM )  : // 

         ierr = MG_Block_def_random ( params, blk );
         MG_Assert ( !ierr, "MG_Block_def_random" );

         break;

      case ( MG_BLOCK_ORDER_COMM_FIRST_RAND ) : //

         ierr = MG_Block_def_comm_first_rand ( params, blk );
         MG_Assert ( !ierr, "MG_Block_def_comm_first_rand" );

         break;

      case ( MG_BLOCK_ORDER_TDAG ) : // Task DAG.

         ierr = MG_Block_def_tdag ( params, blk );
         MG_Assert ( !ierr, "MG_Block_def_tdag" );

         break;

      case ( MG_BLOCK_ORDER_WTDAG ) : // Weighted task DAG.

         ierr = MG_Block_def_wtdag ( params, blk );
         MG_Assert ( !ierr, "MG_Block_def_wtdag" );

         break;

      default: 

         ierr = -1;
         MG_Assert ( !ierr, "MG_Block_init:unknown MG_BLOCK_ORDER" );

   } // End switch ( MG_BLOCK_ORDER )

#if defined _MG_MPI || defined _MG_MPIQ
   /* Check that required tag space is supported by MPI implementation */
   {
       int   flag;
       int * value;

       CALL_MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &value, &flag);

       int max_ij;
       int max_tag;

       max_ij = params->nblks_xdir * params->nblks_ydir;
       if (params->nblks_xdir * params->nblks_zdir > max_ij) {
           max_ij = params->nblks_xdir * params->nblks_zdir;
       }
       if (params->nblks_ydir * params->nblks_zdir > max_ij) {
           max_ij = params->nblks_ydir * params->nblks_zdir;
       }

       /* See `mg_comm_utils.c` for block-tag mapping */
       max_tag = params->numvars * max_ij;

       // printf("MPI_TAG_UB: %d\n", *value);
       // printf("max_tag: %d\n", max_tag);

       if ((max_tag - 1) > *value) {
           ierr = -1;
           MG_Assert ( !ierr, "MG_Block_init: too many tags" );
       }
       MG_Assert ( (max_tag - 1) <= *value, "MG_Block_init: too many tags" );
   }
#endif

   return ( ierr );

} // end MG_Block_init

// ======================================================================================
// These functions to be implemented:

int MG_Block_def_mta ( InputParams *params, BlockInfo **blk )
{
   int ierr = 0; // Return status.

   fprintf ( stderr, "MG_Block_def_mta not yet implemented. \n" );
   ierr = -1;

   return ( ierr );
}

int MG_Block_def_tdag ( InputParams *params, BlockInfo **blk )
{
   int ierr = 0; // Return status.

   fprintf ( stderr, "MG_Block_def_mta not yet implemented. \n" );
   ierr = -1;

   return ( ierr );
}

int MG_Block_def_wtdag ( InputParams *params, BlockInfo **blk )
{
   int ierr = 0; // Return status.

   fprintf ( stderr, "MG_Block_def_mta not yet implemented. \n" );
   ierr = -1;

   return ( ierr );
}

// ************************************************************************

int MG_Block_set_neigh ( InputParams *params, BlockInfo **blk )
{
   // -------
   // Purpose
   // -------
   // 
   // Set up the neighbors for interprocess communication and physical boundaries. 

   // ------------------
   // Local Declarations
   // ------------------

   int
      blknum,             // Counter.
      gd,                 // Shorthand.
      i, j, k,            // Counters
      ierr = 0,           // Return status.
      mindim,             // Minimum dimension of a block's halo.
      rem_x,              // Remainder of domain in x direction.
      rem_y,              // Remainder of domain in y direction.
      rem_z,              // Remainder of domain in z direction.
      xstart, xend,       // Temporary counters.
      ystart, yend,
      zstart, zend;

   // ---------------------
   // Executable statements 
   // ---------------------

   gd = params->ghostdepth;

   for ( i=0; i<params->numblks; i++ ) {

      //999printf ( " ============ BLOCK %d ============ \n", i );

      blk[i]->bc[SOUTHWEST] = 0;
      blk[i]->bc[SOUTH]     = 0;
      blk[i]->bc[SOUTHEAST] = 0;

      blk[i]->bc[NORTHWEST] = 0;
      blk[i]->bc[NORTH]     = 0;
      blk[i]->bc[NORTHEAST] = 0;

      blk[i]->bc[EAST]    = 0;
      blk[i]->bc[WEST]    = 0;

      blk[i]->bc[BACK]       = 0;
      blk[i]->bc[BACK_SOUTH] = 0;
      blk[i]->bc[BACK_WEST]  = 0;
      blk[i]->bc[BACK_NORTH] = 0;
      blk[i]->bc[BACK_EAST]  = 0;

      blk[i]->bc[FRONT]       = 0;
      blk[i]->bc[FRONT_SOUTH] = 0;
      blk[i]->bc[FRONT_WEST]  = 0;
      blk[i]->bc[FRONT_NORTH] = 0;
      blk[i]->bc[FRONT_EAST]  = 0;

      blk[i]->bc[BC_COUNTER] = 0;

      params->numblkbc[SOUTHWEST] = 0;
      params->numblkbc[SOUTH]     = 0;
      params->numblkbc[SOUTHEAST] = 0;

      params->numblkbc[NORTHWEST] = 0;
      params->numblkbc[NORTH]     = 0;
      params->numblkbc[NORTHEAST] = 0;

      params->numblkbc[EAST]      = 0;
      params->numblkbc[WEST]      = 0;

      params->numblkbc[FRONT]       = 0;
      params->numblkbc[FRONT_SOUTH] = 0;
      params->numblkbc[FRONT_WEST]  = 0;
      params->numblkbc[FRONT_NORTH] = 0;
      params->numblkbc[FRONT_EAST]  = 0;

      params->numblkbc[BACK]       = 0;
      params->numblkbc[BACK_SOUTH] = 0;
      params->numblkbc[BACK_WEST]  = 0;
      params->numblkbc[BACK_NORTH] = 0;
      params->numblkbc[BACK_EAST]  = 0;

      // South boundary

      if ( params->mypy == 0 ) {
         if ( blk[i]->ystart == gd ) {
            blk[i]->bc[SOUTH] = 1;
            blk[i]->bc[BC_COUNTER]++;
            params->numblkbc[SOUTH]++;

            if ( params->mypx == 0 ) {  // x-z plane for x=1, y=1
               if ( blk[i]->xstart == gd ) {
                  //999printf ( "SOUTHWEST \n" );
                  blk[i]->bc[SOUTHWEST] = 1;
                  blk[i]->bc[BC_COUNTER]++;
                  params->numblkbc[SOUTHWEST]++;
               }
            }
            if ( params->mypx == ( params->npx - 1 ) ) {  // x-z plane for x=nx, y=1
               if ( blk[i]->xend == params->nx+gd-1 ) {
                  //999printf ( "SOUTHEAST \n" );
                  blk[i]->bc[SOUTHEAST] = 1;
                  blk[i]->bc[BC_COUNTER]++;
                  params->numblkbc[SOUTHEAST]++;
               }
            }
         }
      }
   
      // North boundary
   
      if ( params->mypy == params->npy-1 ) {
         if ( blk[i]->yend == params->ny+gd-1 ) {
            blk[i]->bc[NORTH] = 1;
            blk[i]->bc[BC_COUNTER]++;
            params->numblkbc[NORTH]++;
         
            if ( params->mypx == 0 ) {  // x-z plane for x=1, y=ny
               if ( blk[i]->xstart == gd ) {
                  blk[i]->bc[NORTHWEST] = 1;
                  blk[i]->bc[BC_COUNTER]++;
                  params->numblkbc[NORTHEAST]++; 
               }
            }
            if ( params->mypx == ( params->npx - 1 ) ) {  // x-z plane for x=nx, y=ny
               if ( blk[i]->xend == params->nx+gd-1 ) {
                  blk[i]->bc[NORTHEAST] = 1;
                  blk[i]->bc[BC_COUNTER]++;
                  params->numblkbc[NORTHEAST]++;
               }
            }
         }
      }

      // West boundary
   
      if ( params->mypx == 0 ) {
         if ( blk[i]->xstart == gd ) {
            blk[i]->bc[WEST] = 1;
            blk[i]->bc[BC_COUNTER]++;
            params->numblkbc[WEST]++;
         }
      } 
      // East boundary
   
      if ( params->mypx == params->npx-1 ) {
         if ( blk[i]->xend == params->nx+gd-1 ) {
            blk[i]->bc[EAST] = 1;
            blk[i]->bc[BC_COUNTER]++;
            params->numblkbc[EAST]++;
         }
      }
      
      if ( params->stencil == MG_STENCIL_3D7PT || params->stencil == MG_STENCIL_3D27PT ) {
   
         // Front boundary

         if ( params->mypz == 0 ) {
            if ( blk[i]->zstart == gd ) {
               blk[i]->bc[FRONT] = 1;
               blk[i]->bc[BC_COUNTER]++;
               params->numblkbc[FRONT]++;
               if ( params->mypx == 0 ) {
                  if ( blk[i]->xstart == gd ) {
                     blk[i]->bc[FRONT_WEST] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[FRONT_WEST]++;
                  }
               }
               if ( params->mypx == ( params->npx - 1) ) {
                  if ( blk[i]->xend == params->nx+gd-1 ) {
                     blk[i]->bc[FRONT_EAST] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[FRONT_EAST]++;
                  }
               }
               if ( params->mypy == 0 ) {
                  if ( blk[i]->ystart == gd ) {
                     blk[i]->bc[FRONT_SOUTH] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[FRONT_SOUTH]++;
                  }
               }
               if ( params->mypy == ( params->npy - 1 ) ) {
                  if ( blk[i]->yend == params->ny+gd-1 ) {
                     blk[i]->bc[FRONT_NORTH] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[FRONT_NORTH]++;
                  }
               }
            }
         }
   
         // Back boundary
   
         if ( params->mypz == params->npz-1 ) {
            if ( blk[i]->zend == params->nz+gd-1 ) {
               blk[i]->bc[BACK] = 1;
               blk[i]->bc[BC_COUNTER]++;
               params->numblkbc[BACK]++;
               if ( params->mypx == 0 ) {
                  if ( blk[i]->xstart == gd ) {
                     blk[i]->bc[BACK_WEST] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[BACK_WEST]++;
                  }
               }
               if ( params->mypx == ( params->npx - 1) ) {
                  if ( blk[i]->xend == params->nx+gd-1 ) {
                     blk[i]->bc[BACK_EAST] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[BACK_EAST]++;
                  }
               }
               if ( params->mypy == 0 ) {
                  if ( blk[i]->ystart == gd ) {
                     blk[i]->bc[BACK_SOUTH] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[BACK_SOUTH]++;
                  }
               }
               if ( params->mypy == ( params->npy - 1 ) ) {
                  if ( blk[i]->yend == params->ny+gd-1 ) {
                     blk[i]->bc[BACK_NORTH] = 1;
                     blk[i]->bc[BC_COUNTER]++;
                     params->numblkbc[BACK_NORTH]++;
                  }
               }
            }
         }
 
      } // End 3D stencil options.

#if defined _MG_MPI

      // Set neighbors.

      //MG_Barrier ( );
      //printf ( "[pe %d] neighbors=(%d,%d) (%d,%d) (%d,%d) \n\n", mgpp.mype, neighbors[NORTH], neighbors[SOUTH], neighbors[EAST], neighbors[WEST], neighbors[BACK], neighbors[FRONT] );
      //MG_Barrier ( );

      blk[i]->neighbors[NORTH] = -1;
      blk[i]->neighbors[SOUTH] = -1;
      blk[i]->neighbors[EAST]  = -1;
      blk[i]->neighbors[WEST]  = -1;
      blk[i]->neighbors[FRONT] = -1;
      blk[i]->neighbors[BACK]  = -1;

      //printf ( "[pe %d] Block %i (%d,%d) (%d,%d) (%d,%d) \n", mgpp.mype, i,  );

      if ( neighbors[NORTH] != -1 ) {
         if ( blk[i]->yend == params->ny + gd - 1 ) { 
            blk[i]->neighbors[NORTH] = neighbors[NORTH]; 
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }
      if ( neighbors[SOUTH] != -1 ) {
         if ( blk[i]->ystart == gd ) {                
            blk[i]->neighbors[SOUTH] = neighbors[SOUTH];
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }
      if ( neighbors[EAST] != -1 ) {
         if ( blk[i]->xend == params->nx + gd - 1 ) { 
            blk[i]->neighbors[EAST] = neighbors[EAST];
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }
      if ( neighbors[WEST] != -1 ) {
         if ( blk[i]->xstart == gd ) {                
            blk[i]->neighbors[WEST] = neighbors[WEST];
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }
      if ( neighbors[BACK] != -1 ) {
         if ( blk[i]->zend == params->nz + gd - 1 ) { 
            blk[i]->neighbors[BACK] = neighbors[BACK];
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }
      if ( neighbors[FRONT] != -1 ) {           
         if ( blk[i]->zstart == gd ) {
            blk[i]->neighbors[FRONT] = neighbors[FRONT];
            //blk[i]->info++;
            blk[i]->info = 2;
         }
      }

#if defined _MG_DEBUG
      MG_Barrier ();
      printf ( "[pe %d] Subblock neighbors to %d %d %d %d %d %d \n", mgpp.mype, blk[i]->neighbors[NORTH], blk[i]->neighbors[SOUTH], blk[i]->neighbors[EAST], blk[i]->neighbors[WEST], blk[i]->neighbors[FRONT], blk[i]->neighbors[BACK] );
      MG_Barrier ();
#endif

#endif

   } // End loop over blocks.

   return ( ierr );

} // end MG_Block_set_neigh
