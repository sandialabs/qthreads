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

int MG_Fake_work ( InputParams params, StateVar **g, BlockInfo blk, int ivar )
{
   // This section of code adds FLOPS, but doesn't alter the computation.
   // Lets user configure miniGhost to adhere to his/her application characteristics.
   // A little extra meaningless work to try to prevent compiler from eliminating this work.


   // -----------------------------------------------------------------------------------

   // ------------------
   // Local Declarations
   // ------------------

   static int
      flag = 0;            // Prevents use of uninitialized values2 the first time through here.

   int
      blksize,             // For allocating work space.
      ierr = 0,            // Return status
      i,                   // Counter
      irandnum,
      wvar;                // Variable index (which variable).

   MG_REAL
      *grid_in,
      *work;

   double
      time_start;

   // ---------------------
   // Executable Statements
   // ---------------------

   // Needs ghost space, even though unused, since numbering in stencil computation is 1..n.
   blksize = ( blk.xend - blk.xstart + 1 + params.ghostdepth*2 ) * 
             ( blk.yend - blk.ystart + 1 + params.ghostdepth*2 ) * 
             ( blk.zend - blk.zstart + 1 + params.ghostdepth*2 );
   work = (MG_REAL*)MG_CALLOC( blksize, sizeof(MG_REAL) );
   MG_Assert ( work != NULL, "MG_Fake_work: MG_CALLOC ( work )" );

   for ( i=0; i<g[ivar]->do_more_work_nvars; i++ ) {

      wvar = g[ivar]->do_more_work_vars[i]; 

      if ( flag ) {

         if ( rand() % 2 ) {
            grid_in  = g[wvar]->values1;
         }
         else {
            grid_in  = g[wvar]->values2;
         }
      }
      else { // First time through values2 have not been set.
         grid_in  = g[wvar]->values1;
      }
      //if ( mgpp.mype == 0 ) {
         // printf ( " [pe %d thr %d] %d extra work operating on var %d \n", mgpp.mype, mg_get_os_thread_num(), i, wvar );
      //}


   //printf ( "ivar = %d; blksize=%d, grid_in %x work %x\n", ivar, blksize, grid_in, work );

      // Apply stencil.

      switch ( params.stencil )
      {
         case MG_STENCIL_2D5PT:
   
            ierr = MG_Stencil_2d5pt ( params, grid_in, work, blk );
            MG_Assert ( !ierr, "MG_Fake_work:MG_Stencil_2d5pt" );
            break;
   
         case MG_STENCIL_2D9PT:
   
            ierr = MG_Stencil_2d9pt ( params, grid_in, work, blk );
            MG_Assert ( !ierr, "MG_Fake_work:MG_Stencil_2d9pt" );
            break;
   
         case MG_STENCIL_3D7PT:
   
            ierr = MG_Stencil_3d7pt ( params, grid_in, work, blk );
            MG_Assert ( !ierr, "MG_Fake_work:MG_Stencil_3d7pt" );
            break;
   
         case MG_STENCIL_3D27PT:
   
            ierr = MG_Stencil_3d27pt ( params, grid_in, work, blk );
            MG_Assert ( !ierr, "MG_Fake_work:MG_Stencil_3d27pt" );
            break;
   
         default:
            ierr = -1;
            MG_Assert ( !ierr, "MG_Fake_work: Unknown stencil." );
   
      } // End switch ( params.stencil )
   }
   MG_Assert ( !ierr, "MG_Fake_work" );

   if ( work != NULL ) {
      free ( work );
   }

   flag++;

   return ( ierr );
}

// ======================================================================================
