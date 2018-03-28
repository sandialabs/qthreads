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

// MPI internode communication strategies implemented in mg_bex_<...>.
// 1) mg_bex_<diags>_SR:   MPI_Send / MPI_Recv
// 2) mg_bex_<diags>_ISR:  MPI_Isend/ MPI_Recv
// 3) mg_bex_<diags>_SIR:  MPI_Send / MPI_Irecv
// 4) mg_bex_<diags>_ISIR: MPI_Isend/ MPI_Irecv

#include "mg_tp.h"

int MG_Boundary_exchange_iSiR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                       // Return status
      count[3],
      i, j, k, m,                     // Counters
      msgtag[MAX_NUM_NEIGHBORS],      // MPI semantics allow for tag reuse. Correct?
      num_recvs,                      // Counter
      num_recvs_outstanding,          // Counter
      num_sends,                      // Counter
      num_sends_outstanding,          // Counter
      offset,                         // Offset for packing and unpacking msg buffers.
      thread_id,
      which_neighbor;                 // Identifies incoming message.

   double
      time_start;

   MG_REAL
      *recvbuffer_north, 
      *recvbuffer_south,
      *recvbuffer_east,
      *recvbuffer_west,
      *recvbuffer_front,
      *recvbuffer_back,

      *sendbuffer_north,
      *sendbuffer_south,
      *sendbuffer_east,
      *sendbuffer_west,
      *sendbuffer_front,
      *sendbuffer_back;

   MPI_Request
      msg_reqs[2*MAX_NUM_NEIGHBORS];
      
   MPI_Status
      recv_status;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   //printf ( "\n [pe %d] ======================== Enter MG_BEX ===================== \n\n ", mgpp.mype );
   //MG_Barrier ( );
   //printf ( "[pe %d] Begin... \n", mgpp.mype );

   for ( i=0; i<2*MAX_NUM_NEIGHBORS; i++ ) {
      msg_reqs[i] = MPI_REQUEST_NULL;
   }
   count[NS] = ( blk.xend - blk.xstart + 1 ) * ( blk.zend - blk.zstart + 1 );
   count[EW] = ( blk.yend - blk.ystart + 1 ) * ( blk.zend - blk.zstart + 1 );
   count[FB] = ( blk.xend - blk.xstart + 1 ) * ( blk.yend - blk.ystart + 1 );

   ierr = MG_Get_tags ( params, blk, ivar, msgtag );
   MG_Assert ( !ierr, "MG_Boundary_exchange_SR::MG_Get_tags" );

   // Assuming that these buffers cannot be permanently restricted to a particular task.
   recvbuffer_north = (MG_REAL*)MG_CALLOC ( count[NS], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_north != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_north )" );

   recvbuffer_south = (MG_REAL*)MG_CALLOC ( count[NS], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_south != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_south )" );

   recvbuffer_east = (MG_REAL*)MG_CALLOC ( count[EW], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_east != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_east )" );

   recvbuffer_west = (MG_REAL*)MG_CALLOC ( count[EW], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_west != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_west )" );

   recvbuffer_front = (MG_REAL*)MG_CALLOC ( count[FB], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_front != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_front )" );

   recvbuffer_back = (MG_REAL*)MG_CALLOC ( count[FB], sizeof(MG_REAL) );
   MG_Assert ( recvbuffer_back != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer_back )" );

   sendbuffer_north = (MG_REAL*)MG_CALLOC ( count[NS], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_north != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_north )" );

   sendbuffer_south = (MG_REAL*)MG_CALLOC ( count[NS], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_south != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_south )" );

   sendbuffer_east = (MG_REAL*)MG_CALLOC ( count[EW], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_east != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_east )" );

   sendbuffer_west = (MG_REAL*)MG_CALLOC ( count[EW], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_west != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_west )" );

   sendbuffer_front = (MG_REAL*)MG_CALLOC ( count[FB], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_front != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_front )" );

   sendbuffer_back = (MG_REAL*)MG_CALLOC ( count[FB], sizeof(MG_REAL) );
   MG_Assert ( sendbuffer_back != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer_back )" );

   int gd = params.ghostdepth;

   //printf ( "[pe %d] MG_BEX: blk.neighbors=(%d,%d) (%d,%d) (%d,%d) \n\n", mgpp.mype, 
            //blk.neighbors[NORTH], blk.neighbors[SOUTH], blk.neighbors[EAST], 
            //blk.neighbors[WEST], blk.neighbors[BACK], blk.neighbors[FRONT] );

   // --------------
   // Post receives.
   // --------------

   num_recvs_outstanding = 0;
   num_sends_outstanding = 0;

   //int mm;
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   MG_Time_start_l1(time_start);

   if ( blk.neighbors[NORTH] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_north, count[NS], MG_COMM_REAL, 
                             blk.neighbors[NORTH], msgtag[sSrN], MPI_COMM_MG, 
                             &msg_reqs[NORTH+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(NORTH)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from NORTH \n", mgpp.mype );
   } 

   if ( blk.neighbors[SOUTH] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_south, count[NS], MG_COMM_REAL, 
                             blk.neighbors[SOUTH], msgtag[sNrS], MPI_COMM_MG, 
                             &msg_reqs[SOUTH+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(SOUTH)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from SOUTH \n", mgpp.mype );

   } 

   if ( blk.neighbors[EAST] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_east, count[EW], MG_COMM_REAL, 
                             blk.neighbors[EAST], msgtag[sWrE], MPI_COMM_MG, 
                             &msg_reqs[EAST+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(EAST)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from EAST \n", mgpp.mype);

   } 

   if ( blk.neighbors[WEST] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_west, count[EW], MG_COMM_REAL, 
                             blk.neighbors[WEST], msgtag[sErW], MPI_COMM_MG, 
                             &msg_reqs[WEST+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(WEST)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from WEST \n", mgpp.mype);

   } 

   if ( blk.neighbors[BACK] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_back, count[FB], MG_COMM_REAL, 
                             blk.neighbors[BACK], msgtag[sFrB], MPI_COMM_MG, 
                             &msg_reqs[BACK+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(BACK)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from BACK \n", mgpp.mype);

   } 

   if ( blk.neighbors[FRONT] != -1 ) { 

      ierr = CALL_MPI_Irecv ( recvbuffer_front, count[FB], MG_COMM_REAL, 
                             blk.neighbors[FRONT], msgtag[sBrF], MPI_COMM_MG, 
                             &msg_reqs[FRONT+MAX_NUM_NEIGHBORS] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Irecv(FRONT)" );

      num_recvs_outstanding++;
      //printf ( "[pe %d] Post RECV from FRONT \n", mgpp.mype );

   } 
   MG_Time_accum_l1(time_start,timings.recv[thread_id]);
   //}} // End debug stmts.

   // ------------------------
   // Pack and send face data.
   // ------------------------

   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   if ( blk.neighbors[NORTH] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer_north[offset++] = grid_in(i, blk.yend, k);
            //printf ( "[pe %d] pack %4.2e for NORTH \n", mgpp.mype, grid_in(i, blk.yend, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_north[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_north, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                             msgtag[sNrS], MPI_COMM_MG, &msg_reqs[NORTH] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(NORTH)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
   } 

   if ( blk.neighbors[SOUTH] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer_south[offset++] = grid_in(i, gd, k);
            //printf ( "[pe %d] pack %4.2e for SOUTH \n", mgpp.mype, grid_in(i, gd, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_south[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_south, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH],
                             msgtag[sSrN], MPI_COMM_MG, &msg_reqs[SOUTH] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(SOUTH)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
   } 

   if ( blk.neighbors[EAST] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            sendbuffer_east[offset++] = grid_in(blk.xend, j, k);
            //printf ( "[pe %d] pack %4.2e for EAST \n", mgpp.mype, grid_in(blk.xend, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_east[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_east, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                              msgtag[sErW], MPI_COMM_MG, &msg_reqs[EAST] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(EAST)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
   } 

   if ( blk.neighbors[WEST] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart; j<=blk.yend; j++ ) {
            sendbuffer_west[offset++] = grid_in(gd, j, k);
            //printf ( "[pe %d] pack %4.2e for WEST \n", mgpp.mype, grid_in(gd, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_west[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_west, count[EW], MG_COMM_REAL, blk.neighbors[WEST],
                              msgtag[sWrE], MPI_COMM_MG, &msg_reqs[WEST] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(WEST)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
   } 

   if ( blk.neighbors[BACK] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ )  {
            sendbuffer_back[offset++] = grid_in(i, j, blk.zend);
            //printf ( "[pe %d] pack %4.2e for BACK \n", mgpp.mype, grid_in(i, j, blk.zend) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_back[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_back, count[FB], MG_COMM_REAL, blk.neighbors[BACK],
                              msgtag[sBrF], MPI_COMM_MG, &msg_reqs[BACK] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(BACK)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
   } 

   if ( blk.neighbors[FRONT] != -1 ) { 

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( j=blk.ystart; j<=blk.yend; j++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer_front[offset++] = grid_in(i, j, gd);
            //printf ( "[pe %d] pack %4.2e for FRONT \n", mgpp.mype, grid_in(i, j, gd) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_front[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Isend ( sendbuffer_front, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                              msgtag[sFrB], MPI_COMM_MG, &msg_reqs[FRONT] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(FRONT)" );
      num_sends_outstanding++;

      MG_Time_accum_l1(time_start,timings.send[thread_id]);

   } 
   //}}
   //printf ( "[pe %d] Past SENDING 2\n", mgpp.mype );

   // Apply boundary conditions,providing more time for incoming messages.

   //MG_Time_start(time_start);

   //ierr = MG_Boundary_conditions ( params, g, grid_in, *blk );
   //MG_Assert ( !ierr, "MG_Stencil:MG_Boundary_conditions" );

   //MG_Time_accum(time_start,timings.bc_tasks[thread_id]);

   // ---------------------------
   // Complete sends and receives
   // ---------------------------

   num_recvs = num_recvs_outstanding;
   num_sends = num_sends_outstanding;

   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   for ( m=0; m<num_sends+num_recvs; m++ ) {

      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Waitany ( 2*MAX_NUM_NEIGHBORS, msg_reqs, &which_neighbor, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Waitany" );

      switch ( which_neighbor ) {

         case NORTH:
            MG_Time_accum_l1(time_start,timings.wait_send_north[thread_id]);
            num_sends_outstanding--;
            break;
         case SOUTH:
            MG_Time_accum_l1(time_start,timings.wait_send_south[thread_id]);
            num_sends_outstanding--;
            break;
         case EAST:
            MG_Time_accum_l1(time_start,timings.wait_send_east[thread_id]);
            num_sends_outstanding--;
            break;
         case WEST:
            MG_Time_accum_l1(time_start,timings.wait_send_west[thread_id]);
            num_sends_outstanding--;
            break;
         case FRONT:
            MG_Time_accum_l1(time_start,timings.wait_send_front[thread_id]);
            num_sends_outstanding--;
            break;
         case BACK:
            MG_Time_accum_l1(time_start,timings.wait_send_back[thread_id]);
            num_sends_outstanding--;
            break;
            break;

         case NORTH+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_north[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, params.ny + gd, k) = recvbuffer_north[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from NORTH \n", mgpp.mype, grid_in(i, params.ny + gd, k) );
               }
            }
            MG_Time_accum_l1(time_start,timings.unpack_north[thread_id]);

            break;

         case SOUTH+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_south[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, 0, k) = recvbuffer_south[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from SOUTH \n", mgpp.mype, grid_in(i, 0, k) );
               }
            }
            MG_Time_accum_l1(time_start,timings.unpack_south[thread_id]);

            break;

         case EAST+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_east[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( j=blk.ystart; j<=blk.yend; j++ ) {
                  grid_in(params.nx + gd, j, k) = recvbuffer_east[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from EAST \n", mgpp.mype, grid_in(params.nx + gd, j, k) );
               }
            }
            MG_Time_accum_l1(time_start,timings.unpack_east[thread_id]);

            break;

         case WEST+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_west[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( j=blk.ystart; j<=blk.yend; j++ ) {
                  grid_in(0, j, k) = recvbuffer_west[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from WEST \n", mgpp.mype, grid_in(0, j, k) );
               }
            }
            MG_Time_accum_l1(time_start,timings.unpack_west[thread_id]);

            break;

         case BACK+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_back[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0; 
            for ( j=blk.ystart; j<=blk.yend; j++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, j, 0) = recvbuffer_back[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from BACK \n", mgpp.mype, grid_in(i, j, 0) );
               }
            }

            MG_Time_accum_l1(time_start,timings.unpack_back[thread_id]);

            break;

         case FRONT+MAX_NUM_NEIGHBORS:

            MG_Time_accum_l1(time_start,timings.wait_recv_front[thread_id]);
            MG_Time_start_l1(time_start); 

            num_recvs_outstanding--;
            offset = 0;
            for ( j=blk.ystart; j<=blk.yend; j++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, j, params.nz + gd) = recvbuffer_front[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from FRONT \n", mgpp.mype, grid_in(i, j, params.nz + gd) );
               }
            }
            MG_Time_accum_l1(time_start,timings.unpack_front[thread_id]);

            break;

      } // End switch ( which_neighbor )

   } // End loop on completing receives.
   //}}

   //printf ( "[pe %d] THROUGH RECV COMPLETION \n", mgpp.mype );
   //MG_Barrier ( );

   MG_Assert ( num_sends_outstanding == 0, "MG_Boundary_exchange:num_sends_outstanding != 0" );
   MG_Assert ( num_recvs_outstanding == 0, "MG_Boundary_exchange:num_recvs_outstanding != 0" );

   if ( recvbuffer_north )
      free ( recvbuffer_north );
   if ( recvbuffer_south )
      free ( recvbuffer_south );
   if ( recvbuffer_east )
      free ( recvbuffer_east );
   if ( recvbuffer_west )
      free ( recvbuffer_west );
   if ( recvbuffer_front )
      free ( recvbuffer_front );
   if ( recvbuffer_back )
      free ( recvbuffer_back );

   if ( sendbuffer_north )
      free ( sendbuffer_north );
   if ( sendbuffer_south )
      free ( sendbuffer_south );
   if ( sendbuffer_east )
      free ( sendbuffer_east );
   if ( sendbuffer_west )
      free ( sendbuffer_west );
   if ( sendbuffer_front )
      free ( sendbuffer_front );
   if ( sendbuffer_back )
      free ( sendbuffer_back );

   return ( ierr );

} // End MG_Boundary_exchange.
