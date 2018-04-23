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

// MG_Set_diag_comm : 
// MG_Get_tags      : 

int MG_Set_diag_comm ( InputParams params, BlockInfo blk, int count[],
                       int *bfd_xstart, int *bfd_xend, int *bfd_ystart, int *bfd_yend,
                       int *ewd_start, int *ewd_end )
{
   // ---------------
   // Local Variables
   // ---------------

   int
      gd,
      ierr = 0;                       // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

   gd = params.ghostdepth;

   MG_Barrier ( );
   //printf ( "[pe %d] blk.xyz= (%d,%d) (%d,%d) (%d,%d) \n", 
            //mgpp.mype, blk.xstart,blk.xend,blk.ystart,blk.yend,blk.zstart,blk.zend );
   MG_Barrier ( );

   if ( ( blk.neighbors[NORTH] != -1 ) || ( blk.neighbors[SOUTH] ) != -1 ) {
      count[NS] = ( blk.xend - blk.xstart + 1 ) * ( blk.zend - blk.zstart + 1 );
      //count[NS] = params.blkxlen * params.blkzlen;
   }
   *ewd_start = blk.ystart;
   *ewd_end   = blk.yend;
   if ( ( blk.neighbors[EAST] != -1 ) || ( blk.neighbors[WEST] ) != -1 ) {
      if ( blk.neighbors[NORTH] != -1 ) {
         if ( blk.ystart == gd ) {
            *ewd_start -= gd;
         }
      }
      if ( blk.neighbors[SOUTH] != -1 ) {
         if ( blk.yend == params.ny + gd - 1 ) {
            *ewd_end += gd;
         }
      }
       //printf ( "[pe %d] ewd_start/end=(%d, %d) -> (%d, %d) \n", mgpp.mype, blk.ystart, blk.yend, blk.ystart - *ewd_start, blk.yend + *ewd_end );
      count[EW] = ( params.blkylen + gd ) * ( params.blkzlen );
   }
   *bfd_xstart = blk.xstart;
   *bfd_xend   = blk.xend;
   *bfd_ystart = blk.ystart;
   *bfd_yend   = blk.yend;

   if ( ( blk.neighbors[BACK] != -1 ) || ( blk.neighbors[FRONT] != -1 ) ) {
      if ( blk.neighbors[WEST] != -1 ) {
         if ( blk.xstart == gd ) {
            *bfd_xstart -= gd;
         }
      }
      if ( blk.neighbors[EAST] != -1 ) { 
         if ( blk.xend == params.nx + gd - 1 ) {
            *bfd_xend += gd;
         }
      }
      if ( blk.neighbors[SOUTH] != -1 ) {
         if ( blk.ystart == gd ) {
            *bfd_ystart -= gd;
         }
      }
      if ( blk.neighbors[NORTH] != -1 ) {
         if ( blk.yend == params.ny + gd - 1 ) {
            *bfd_yend += gd;
         }
      }
       //printf ( "[pe %d] bfd %d, %d, %d, %d (%d, %d) (%d, %d) \n", mgpp.mype, *bfd_xstart, **bfd_xend, *bfd_ystart, *bfd_yend, blk.xstart-*bfd_xstart, blk.xend+*bfd_xend, blk.ystart-*bfd_ystart, blk.yend+*bfd_yend );

      count[FB] = ( params.blkxlen + gd ) * ( params.blkylen + gd );
   }
   MG_Barrier ( );
   //printf ( "[pe %d] BEX_DIAGS: counts = %d %d %d \n", mgpp.mype, count[NS], count[EW], count[FB] );
   MG_Barrier ( );

   return ( ierr );
}

// ===================================================================================

int MG_Get_tags ( InputParams params, BlockInfo blk, int ivar, int *msgtag )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                // Return status
      thread_id;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   // sNrS: Send north, recv south.
   // sSrN: Send south, recv north.

   // sErW: Send east, recv west.
   // sWrE: Send west, recv east.

   // sFrB: Send front, recv back.
   // sBrF: Send back,  recv front.

   msgtag[sNrS] = -9999; // MPI should break if attempt to use.
   msgtag[sSrN] = -9999;
   msgtag[sErW] = -9999;
   msgtag[sWrE] = -9999;
   msgtag[sFrB] = -9999;
   msgtag[sBrF] = -9999;

#if defined _DONT_WORRY_ABOUT_TAGS       // For an experiment.

   msgtag[sNrS] = 34567;
   msgtag[sSrN] = msgtag[sNrS];
   msgtag[sErW] = 34568;
   msgtag[sWrE] = msgtag[sErW];
   msgtag[sFrB] = 34569;
   msgtag[sBrF] = msgtag[sFrB];

#else

   int const nbx = params.nblks_xdir;
   int const nby = params.nblks_ydir;
   int const nbz = params.nblks_zdir;

   int const px = blk.mypx;
   int const py = blk.mypy;
   int const pz = blk.mypz;

   int max_ij = nbx * nby;
   if (nbx * nbz > max_ij) {
       max_ij = nbx * nbz;
   }
   if (nby * nbz > max_ij) {
       max_ij = nby * nbz;
   }

   msgtag[sNrS] = (ivar * max_ij) + ((pz * nbx) + px);
   msgtag[sSrN] = msgtag[sNrS];
   msgtag[sErW] = (ivar * max_ij) + ((pz * nby) + py);
   msgtag[sWrE] = msgtag[sErW];
   msgtag[sFrB] = (ivar * max_ij) + ((py * nbx) + px);
   msgtag[sBrF] = msgtag[sFrB];

   //int mm;
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
      //printf ( "[pe %d] tags = %d, %d, %d, %d, %d, %d \n", mgpp.mype, msgtag[0], msgtag[1], msgtag[2], msgtag[3], msgtag[4], msgtag[5] );
   //}}

#endif // _DONT_WORRY_ABOUT_TAGS

   ierr = 0;

   return ( ierr );

} // End MG_Boundary_exchange.
