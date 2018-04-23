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

int MG_Boundary_exchange ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // Gateway to the selected stencil. 
   // FIXME rfbarre: this could be changed to use function pointers.

   // Control function for stencils. 
   // 
   // 1) Apply stencil computation: MG_Boundary_exchange_xDypt.
   // 2) Apply collective computaiton: MG_Max/Min/Sum_grid
   // 3) Apply/manage boundary conditions: MG_Boundary_conditions
   // 4) Inter-process boundary exchange: MG_Boundary_exchange
   // -----------------------------------------------------------------------------------

   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,            // Return status
      thread_id;

   double
      time_start;

   // ---------------------
   // Executable Statements
   // ---------------------

   //printf ( "[pe %d] ENTER MG_Boundary_exchange \n", mgpp.mype );
   //MG_Barrier ();

   thread_id = mg_get_os_thread_num();

   switch ( params.stencil )
   {
      case MG_STENCIL_2D5PT:
      case MG_STENCIL_3D7PT:

         switch ( params.comm_strategy ) {

            case MG_COMM_STRATEGY_SR:

               ierr = MG_Boundary_exchange_SR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_SR" );
               break;

            case MG_COMM_STRATEGY_ISR:

               ierr = MG_Boundary_exchange_iSR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_iSR" );
               break;

            case MG_COMM_STRATEGY_SIR:

               ierr = MG_Boundary_exchange_SiR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_SiR" );
               break;

            case MG_COMM_STRATEGY_ISIR:

               ierr = MG_Boundary_exchange_iSiR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_iSiR" );
               break;

            default:

               ierr = -1;
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange: Unknown comm strategy" );
               break;
            }
         break;

      case MG_STENCIL_2D9PT:
      case MG_STENCIL_3D27PT:

         switch ( params.comm_strategy ) {

            case MG_COMM_STRATEGY_SR:

               ierr = MG_Boundary_exchange_diags_SR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_diags_SR" );
               break;

            case MG_COMM_STRATEGY_ISR:

               ierr = MG_Boundary_exchange_diags_iSR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_diags_iSR" );
               break;

            case MG_COMM_STRATEGY_SIR:

               ierr = MG_Boundary_exchange_diags_SiR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_diags_SiR" );
               break;

            case MG_COMM_STRATEGY_ISIR:

               ierr = MG_Boundary_exchange_diags_iSiR ( params, grid_in, blk, ivar );
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_diags_iSiR" );
               break;

            default:

               ierr = -1;
               MG_Assert ( !ierr, "MG_Boundary_exchange:MG_Boundary_exchange_diags: Unknown comm strategy" );
               break;
         }
         break;

      default:

         ierr = -1;

         MG_Assert ( !ierr, "MG_Boundary_exchange: Unknown stencil(bex)" );
         break;

   } // End switch ( params.stencil )

   return ( ierr );
}
