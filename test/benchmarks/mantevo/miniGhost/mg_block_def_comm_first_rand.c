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

static void swap_blocks(BlockInfo **blk, int i, int j)
{
   int 
      k,            // Counters
      neighbors_i[6],     // Temporaries.
      bc_i[6],            // Temporaries.
      info_i,             // Temporary.
      xstart_i, xend_i,   // Temporaries.
      ystart_i, yend_i,
      zstart_i, zend_i;

      xstart_i = blk[i]->xstart;
      xend_i   = blk[i]->xend;
      ystart_i = blk[i]->ystart;
      yend_i   = blk[i]->yend;
      zstart_i = blk[i]->zstart;
      zend_i   = blk[i]->zend;

      info_i   = blk[i]->info;

#if defined _MG_MPI
      for ( k=0; k<6; k++ ) {
         neighbors_i[k] = blk[i]->neighbors[k];
      }
#endif
      for ( k=0; k<6; k++ ) {
         bc_i[k] = blk[i]->bc[k];
      }

      blk[i]->xstart = blk[j]->xstart;
      blk[i]->xend   = blk[j]->xend;
      blk[i]->ystart = blk[j]->ystart;
      blk[i]->yend   = blk[j]->yend;
      blk[i]->zstart = blk[j]->zstart;
      blk[i]->zend   = blk[j]->zend;

      blk[i]->info   = blk[j]->info;

#if defined _MG_MPI
      for ( k=0; k<6; k++ ) {
         blk[i]->neighbors[k] = blk[j]->neighbors[k];
      }
#endif

      for ( k=0; k<6; k++ ) {
         blk[i]->bc[k] = blk[j]->bc[k];
      }

      blk[j]->xstart = xstart_i;
      blk[j]->xend   = xend_i; 
      blk[j]->ystart = ystart_i;
      blk[j]->yend   = yend_i; 
      blk[j]->zstart = zstart_i;
      blk[j]->zend   = zend_i; 

      blk[j]->info   = info_i;

#if defined _MG_MPI
      for ( k=0; k<6; k++ ) {
         blk[j]->neighbors[k] = neighbors_i[k];
      }
#endif

      for ( k=0; k<6; k++ ) {
         blk[j]->bc[k] = bc_i[k];
      }
}

int MG_Block_def_comm_first_rand ( InputParams *params, BlockInfo **blk )
{
   // -------
   // Purpose
   // -------
   // 
   // Blocks are ordered such that tasks with comm. come before those with no
   // no comm. Blocks order is randomized before partititioning.

   // ------------------
   // Local Declarations
   // ------------------

   int 
      i, j,               // Counters
      ierr = 0;           // Return status.

   // ---------------------
   // Executable statements 
   // ---------------------

   // Re-order the blocks, using the shuffle algorithm (described in "The Art of Computer Programming").

   for ( i=params->numblks-1; i>0; i-- ) {
      j = ( rand() + mgpp.mype ) % ( i );
      swap_blocks(blk, i, j);
   }

   // Partition comm. and comp. tasks
   j = 0;
   for (i = 0; i < params->numblks; i++) {
       if (1 == blk[i]->info) {
           /* This is a comp. task */
           if (j <= i) {
               j = i;
           }
           for ( ; j < params->numblks; j++) {
               if (1 != blk[i]->info) {
                   swap_blocks(blk, i, j);
               }
           }
           if (j == params->numblks) {
               break;
           }
       } else {
           /* This is a comm. task */
       }
   }

   return ( ierr );

} // end MG_Block_def_comm_first

// ======================================================================================
