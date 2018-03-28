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

int MG_Flux_accumulate ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk )
{
   // ------------------
   // Local Declarations
   // ------------------

   int       
      ierr = 0;           // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

   switch ( params.stencil )
   {
      case MG_STENCIL_2D5PT:
      case MG_STENCIL_3D7PT:

         ierr = MG_Flux_accumulate_2d5pt3d7pt ( params, g, grid_vals, blk );
         break;

      case MG_STENCIL_2D9PT:

         ierr = MG_Flux_accumulate_2d9pt ( params, g, grid_vals, blk );
         break;

      case MG_STENCIL_3D27PT:

         ierr = MG_Flux_accumulate_3d27pt ( params, g, grid_vals, blk );
         break;

      default:
         MG_Assert ( -1, "MG_Flux_accumulate: Unknown stencil." );

   } // End switch ( params.stencil )

   return ( ierr );
   
} // end MG_Flux_accumulate
