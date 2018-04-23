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

int MG_Boundary_conditions ( InputParams params, StateVar *g, MG_REAL *grid_vals, BlockInfo blk )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0;      // Return status

   // ---------------------
   // Executable Statements
   // ---------------------

   params.boundary_condition = MG_BC_DIRICHLET; // FIXME rfbarre: add to commandline input once options are available.
   switch ( params.boundary_condition )
   {
      case ( MG_BC_DIRICHLET ):
         ierr = MG_Flux_accumulate ( params, g, grid_vals, blk );
         break;
      case ( MG_BC_NEUMANN ):
         ierr = -1;
         MG_Assert ( !ierr, 
                     "MG_Boundary_conditions:Neumann boundary conditions (MG_BC_NEUMANN) not yet implemented." );
         break;
      case ( MG_BC_REFLECTIVE ):
         ierr = -1;
         MG_Assert ( !ierr, 
                     "MG_Boundary_conditions:Neumann boundary conditions (MG_BC_REFLECTIVE) not yet implemented." );
         break;
      case ( MG_BC_NONE ):
         ierr = 0;
         break;
      default:
         fprintf ( stderr, 
                   "Unknown boundary conditions: %d \n", params.boundary_condition );
         ierr = -1;
         MG_Assert ( !ierr, "MG_Boundary_conditions: unknown boundary conditions" );
   }
   return ( ierr );
}

// ======================================================================================
