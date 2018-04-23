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

int MG_Boundary_exchange_iSR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                // Return status
      count[3],                // Message size in datatype elements.
      i, j, k,                 // Counters
      msgtag[MAX_NUM_NEIGHBORS],
      offset,                  // Offset for packing and unpacking msg buffers.
      thread_id;

   MG_REAL
      *recvbuffer, 
      *sendbuffer;

   MPI_Request
      send_reqs[MAX_NUM_NEIGHBORS];

   MPI_Status 
      recv_status,
      send_statuses[MAX_NUM_NEIGHBORS];

   double
      time_start;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   //printf ( "[pe %d] ============== Enter boundary exchange ============== \n", mgpp.mype );

   int gd = params.ghostdepth;

   count[NS] = ( blk.xend - blk.xstart + 1 ) * ( blk.zend - blk.zstart + 1 );
   count[EW] = ( blk.yend - blk.ystart + 1 ) * ( blk.zend - blk.zstart + 1 );
   count[FB] = ( blk.xend - blk.xstart + 1 ) * ( blk.yend - blk.ystart + 1 );

   max_msg_count = count[NS] ;
   if ( count[EW] > max_msg_count  ) {
      max_msg_count = count[EW];
   }
   if ( count[FB] > max_msg_count  ) {
      max_msg_count = count[FB];
   }
   ierr = MG_Get_tags ( params, blk, ivar, msgtag );
   MG_Assert ( !ierr, "MG_Boundary_exchange_SR::MG_Get_tags" );

   send_reqs[0] = MPI_REQUEST_NULL;
   send_reqs[1] = MPI_REQUEST_NULL;
   send_reqs[2] = MPI_REQUEST_NULL;
   send_reqs[3] = MPI_REQUEST_NULL;
   send_reqs[4] = MPI_REQUEST_NULL;
   send_reqs[5] = MPI_REQUEST_NULL;

   // Assuming that these buffers cannot be permanently restricted to a particular task.
   recvbuffer = (MG_REAL*)MG_CALLOC ( max_msg_count, sizeof(MG_REAL) );
   MG_Assert ( recvbuffer != NULL, "MG_Boundary_exchange : MG_CALLOC ( recvbuffer )" );

   sendbuffer = (MG_REAL*)MG_CALLOC ( max_msg_count, sizeof(MG_REAL) );
   MG_Assert ( sendbuffer != NULL, "MG_Boundary_exchange : MG_CALLOC ( sendbuffer )" );

   if ( blk.neighbors[NORTH] != -1 ) {

      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, blk.yend, k);

      MG_Time_accum_l1(time_start,timings.pack_north[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "pe %d sending NORTH to pe %d count %d msg \n", 
               mgpp.mype, blk.neighbors[NORTH], count[NS] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                              msgtag[sNrS], MPI_COMM_MG, &send_reqs[NORTH] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(NORTH)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "pe %d recving from NORTH %d count %d msg \n",
               mgpp.mype, blk.neighbors[NORTH], count[NS] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                             msgtag[sSrN], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(NORTH)" );

      MG_Time_accum_l1(time_start,timings.recv_north[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, params.ny + gd, k) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_north[thread_id]);

   } // end NORTH neighbor.

   if ( blk.neighbors[SOUTH] != -1 ) {

      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, gd, k);

      MG_Time_accum_l1(time_start,timings.pack_south[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "pe %d recving from SOUTH %d count %d msg \n",
               mgpp.mype, blk.neighbors[SOUTH], count[NS] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH], 
                             msgtag[sNrS], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.recv_south[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "S/N pe %d sending to pe %d count %d msg \n",
               mgpp.mype, blk.neighbors[SOUTH], count[NS] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH], 
                              msgtag[sSrN], MPI_COMM_MG, &send_reqs[SOUTH] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, 0, k) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_south[thread_id]);

   } // end SOUTH neighbor.

#if defined _MG_DEBUG
   printf ( " ==================== [pe %d] Completed NS =================== \n ", mgpp.mype );

   printf ( "[pe %d] EAST/WEST:xend %d %d neigh %d \n", mgpp.mype, blk.xend, params.nx + gd - 1, blk.neighbors[EAST] );
#endif
   if ( blk.neighbors[EAST] != -1 ) {

      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            sendbuffer[offset++] = grid_in(blk.xend, j, k);

      MG_Time_accum_l1(time_start,timings.pack_east[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "E/W pe %d sending to pe %d count %d msg \n",
               mgpp.mype, blk.neighbors[EAST], count[EW] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                              msgtag[sErW], MPI_COMM_MG, &send_reqs[EAST] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(EAST)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "E/W pe %d recving from %d count %d msg \n",
               mgpp.mype, blk.neighbors[EAST], count[EW] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                             msgtag[sWrE], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(EAST)" );

      MG_Time_accum_l1(time_start,timings.recv_east[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            grid_in(params.nx + gd, j, k) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_east[thread_id]);

   } // end EAST neighbor.

   if ( blk.neighbors[WEST] != -1 ) {

      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            sendbuffer[offset++] = grid_in(gd, j, k);

      MG_Time_accum_l1(time_start,timings.pack_west[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "W/E pe %d recving from %d count %d msg \n",
               mgpp.mype, blk.neighbors[WEST], count[EW] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[EW], MG_COMM_REAL, blk.neighbors[WEST], 
                             msgtag[sErW], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(WEST)" );

      MG_Time_accum_l1(time_start,timings.recv_west[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "W/E pe %d sending to pe %d count %d msg \n",
               mgpp.mype, blk.neighbors[WEST], count[EW] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[WEST], 
                              msgtag[sWrE], MPI_COMM_MG, &send_reqs[WEST] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(WEST)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            grid_in(0, j, k) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_west[thread_id]);

   } // end WEST neighbor.

#if defined _MG_DEBUG
    printf ( " ==================== [pe %d] Completed EW =================== \n ", mgpp.mype );
#endif

   if ( blk.neighbors[BACK] != -1 ) {


      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ ) 
            sendbuffer[offset++] = grid_in(i, j, blk.zend);

      MG_Time_accum_l1(time_start,timings.pack_back[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "B/F pe %d recving from %d count %d msg \n",
               mgpp.mype, blk.neighbors[BACK], count[FB] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[FB], MG_COMM_REAL, blk.neighbors[BACK], 
                             msgtag[sFrB], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(BACK)" );

      MG_Time_accum_l1(time_start,timings.recv_back[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "B/F pe %d sending to pe %d count %d msg \n",
               mgpp.mype, blk.neighbors[BACK], count[FB] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[BACK], 
                              msgtag[sBrF], MPI_COMM_MG, &send_reqs[BACK] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(BACK)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, j, 0) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_back[thread_id]);

   } // end BACK neighbor.

   if ( blk.neighbors[FRONT] != -1 ) { // Exchange boundary with FRONT neighbor.

      MG_Time_start_l1(time_start);

      offset = 0; // Pack send buffer.
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, j, gd);

      MG_Time_accum_l1(time_start,timings.pack_front[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "F/B pe %d sending to pe %d count %d msg \n",
               mgpp.mype, blk.neighbors[FRONT], count[FB] );
#endif
      ierr = CALL_MPI_Isend ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                              msgtag[sFrB], MPI_COMM_MG, &send_reqs[FRONT] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Isend(FRONT)" );

      MG_Time_accum_l1(time_start,timings.send[thread_id]);
      MG_Time_start_l1(time_start);

#if defined _MG_DEBUG
      printf ( "F/B pe %d recving from %d count %d msg \n",
               mgpp.mype, blk.neighbors[FRONT], count[FB] );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                             msgtag[sBrF], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange:CALL_MPI_Recv(FRONT)" );

      MG_Time_accum_l1(time_start,timings.recv_front[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; // Unpack received buffer into ghost space.
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, j, params.nz + gd) = recvbuffer[offset++];

      MG_Time_accum_l1(time_start,timings.unpack_front[thread_id]);

   } // end FRONT neighbor.

   // Complete all sends.


   MG_Time_start_l1(time_start);

   ierr = CALL_MPI_Waitall ( MAX_NUM_NEIGHBORS, send_reqs, send_statuses );

   MG_Time_accum_l1(time_start,timings.wait_send[thread_id]);

#if defined _MG_DEBUG
    printf ( " ==================== [pe %d] Completed FB; exit BEX =================== \n ", mgpp.mype );
#endif

   if ( recvbuffer != NULL )
      free ( recvbuffer );
   if ( sendbuffer != NULL )
      free ( sendbuffer );

   ierr = 0;   // MPI_SUCCESS need not = 0.

   return ( ierr );

} // End MG_Boundary_exchange.
