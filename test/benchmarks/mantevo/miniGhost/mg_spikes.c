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

int MG_Spike_init ( InputParams params, SpikeInfo **spikes )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,          // Return status
      ivar, ispike;      // Counters

   // ---------------------
   // Executable Statements
   // ---------------------

   // Allocate arrays within the struct.
   for ( ispike=0; ispike<params.numspikes; ispike++ ) {

      spikes[ispike] = (SpikeInfo*)MG_CALLOC ( 1, sizeof(SpikeInfo) );
      MG_Assert ( spikes[ispike] != NULL, "MG_Spike_init: spikes[ispike] == NULL" );

      spikes[ispike]->heat = (MG_REAL*)MG_CALLOC ( params.numvars, sizeof(MG_REAL) );
      MG_Assert ( spikes[ispike] != NULL, "MG_Spike_init: spikes[ispike] == NULL" );

      spikes[ispike]->pe = (int*)MG_CALLOC ( params.numvars, sizeof(int) );
      MG_Assert ( spikes[ispike]->pe != NULL, "MG_Spike_init: spikes[ispike]->pe == NULL" );

      spikes[ispike]->x = (int*)MG_CALLOC ( params.numvars, sizeof(int) );
      MG_Assert ( spikes[ispike]->x != NULL, "MG_Spike_init: spikes[ispike]->x == NULL" );

      spikes[ispike]->y = (int*)MG_CALLOC ( params.numvars, sizeof(int) );
      MG_Assert ( spikes[ispike]->y != NULL, "MG_Spike_init: spikes[ispike]->y == NULL" );

      spikes[ispike]->z = (int*)MG_CALLOC ( params.numvars, sizeof(int) );
      MG_Assert ( spikes[ispike]->z != NULL, "MG_Spike_init: spikes[ispike]->z == NULL" );
   }

   // Assuming all pes generate the same stream of random numbers.  FIXME rfbarre
   // If this is not the case, have pe 0 generate and bcast.
   for ( ispike=0; ispike<params.numspikes; ispike++ ) {
      for ( ivar=0; ivar<params.numvars; ivar++ ) {
         spikes[ispike]->heat[ivar] = (MG_REAL)(( rand ( ) % MG_Max3i ( params.nx, params.ny,params.nz ) ) + 1 );

         spikes[ispike]->pe[ivar] = rand ( ) % mgpp.numpes;
         spikes[ispike]->x[ivar]  = ( rand ( ) % params.nx ) + 1;
         spikes[ispike]->y[ivar]  = ( rand ( ) % params.ny ) + 1;
         spikes[ispike]->z[ivar]  = ( rand ( ) % params.nz ) + 1;
      }
   }
   return ( ierr );

} // End MG_Spike_init.

// ======================================================================================

int MG_Spike_insert ( InputParams params, SpikeInfo **spikes, int ispike, StateVar **g, int which )
{
   // ------------------
   // Local Declarations
   // ------------------

   // Insert heat source (spikes) into arrays.
   // This value is added to the total heat applied to the variable.

   int
      ierr;                   // return status.

   MG_REAL
      *this_g;                // Pointer to grid variable.
#define this_g(i,j,k)   this_g[MG_ARRAY_SHAPE(i,j,k)]

   // ------------------
   // Local Declarations
   // ------------------

   int
      i, ivar, j, k;       // Counters

   // ---------------------
   // Executable statements
   // ---------------------

   ierr = 0;

   switch ( params.init_grid_values )
   {
#if defined _MG_SERIAL   // FIXME: This needs to be centered for parallel as well.
      case MG_INIT_GRID_ZEROS:

         for ( ivar=0; ivar<params.numvars; ivar++ ) {

            if ( mgpp.mype == ( spikes[ispike]->pe[ivar] ) ) {

               if ( which%2 ) {
                  this_g = g[ivar]->values1;
               }
               else {
                  this_g = g[ivar]->values2;
               }

               i = ( params.nx + params.ghostdepth ) / 2;
               j = ( params.ny + params.ghostdepth ) / 2;
               k = ( params.nz + params.ghostdepth ) / 2;
               this_g(i,j,k) += spikes[ispike]->heat[ivar];
            }
         }
         break;

      case MG_INIT_GRID_RANDOM:

         for ( ivar=0; ivar<params.numvars; ivar++ ) {

            if ( mgpp.mype == ( spikes[ispike]->pe[ivar] ) ) {

               if ( which%2 ) {
                  this_g = g[ivar]->values1;
               }
               else {
                  this_g = g[ivar]->values2;
               }
               i = spikes[ispike]->x[ivar];
               j = spikes[ispike]->y[ivar];
               k = spikes[ispike]->z[ivar];

               this_g(i,j,k) += spikes[ispike]->heat[ivar];
#if defined _MG_DEBUG
               printf ( "Insert %d spike %e for this_g[%d](%d,%d,%d) now equals %e \n",
                        ispike, spikes[ispike]->heat[ivar], ivar, i, j, k, this_g(i,j,k) );
#endif
            }
         }
         break;
#else
      case MG_INIT_GRID_ZEROS:
      case MG_INIT_GRID_RANDOM:

         for ( ivar=0; ivar<params.numvars; ivar++ ) {

            if ( mgpp.mype == ( spikes[ispike]->pe[ivar] ) ) {

               if ( which%2 ) {
                  this_g = g[ivar]->values1;
               }
               else {
                  this_g = g[ivar]->values2;
               }
               i = spikes[ispike]->x[ivar];
               j = spikes[ispike]->y[ivar];
               k = spikes[ispike]->z[ivar];

               this_g(i,j,k) += spikes[ispike]->heat[ivar];
#if defined _MG_DEBUG
               printf ( "[pe %d] Insert %d spike %e for this_g[%d](%d,%d,%d) now equals %e \n",
                        mgpp.mype, ispike, spikes[ispike]->heat[ivar], ivar, i, j, k, this_g(i,j,k) );
#endif
            }
         }
         break;
#endif

      default:

         if ( mgpp.rootpe == mgpp.mype )
#if defined _MG_DEBUG
            fprintf ( stderr, "MG_Spike_insert: Unknown grid initialization: %d \n", params.init_grid_values );
#endif

         ierr = -1;
         MG_Assert ( !ierr, "MG_Spike_insert: Unknown grid initialization" );

         break;

   } // End switch ( params.init_grid_values )

   // Update amount of heat injected into the system.

#if defined _MG_DEBUG
   for ( ivar=0; ivar<params.numvars; ivar++ ) {
      printf ( "source_total[%d] = %e \n", 0, g[ivar]->source_total );
   }
#endif
   for ( ivar=0; ivar<params.numvars; ivar++ ) {
      g[ivar]->source_total += spikes[ispike]->heat[ivar];
#if defined _MG_DEBUG
      printf ( "With spike, source_total[%d] = %e; spikes[%d]->heat[%d] = %8.8e \n", ivar, g[ivar]->source_total, ispike, ivar, spikes[ispike]->heat[ivar] );
#endif
   }

   return ( ierr );

} // end MG_Spike_insert

//  ===================================================================================
