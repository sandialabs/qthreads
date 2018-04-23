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

// MPI internode communication strategies implemented in mg_bex_diags_<...>.
// 1) mg_bex_<diags>_SR:   MPI_Send / MPI_Recv
// 2) mg_bex_<diags>_ISR:  MPI_Isend/ MPI_Recv
// 3) mg_bex_<diags>_SIR:  MPI_Send / MPI_Irecv
// 4) mg_bex_<diags>_ISIR: MPI_Isend/ MPI_Irecv

int MG_Boundary_exchange_diags_iSiR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // This is a coordinated exchange so that elements from diagonal neighbors automatically
   // propagates to the gathered halo. This requires completion of NORTH/SOUTH exchange,
   // then EAST/WEST, then FRONT/BACK.

#define SENDREQ_N 0                     // Offsets into msg_req array.
#define SENDREQ_S 1 
#define RECVREQ_N 2 
#define RECVREQ_S 3 

#define SENDREQ_E 0 
#define SENDREQ_W 1 
#define RECVREQ_E 2 
#define RECVREQ_W 3 

#define SENDREQ_F 0 
#define SENDREQ_B 1 
#define RECVREQ_F 2 
#define RECVREQ_B 3 

   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                       // Return status
      count[3],
      bfd_xstart, bfd_xend,           // Offset to capture diagonal elements.
      bfd_ystart, bfd_yend,           // Offset to capture diagonal elements.
      ewd_start, ewd_end,             // Offset to capture diagonal elements.
      i, j, k, m,                     // Counters
      maxlen,                         // Length required for msg buffers.
      msgtag[MAX_NUM_NEIGHBORS],      // MPI semantics allow for tag reuse. Correct?
      num_recvs,                      // Counter
      num_sends,                      // Counter
      num_recvs_outstanding,  
      num_sends_outstanding,
      offset,                         // Offset for packing and unpacking msg buffers.
      recv_offset[MAX_NUM_NEIGHBORS], // Offset into recv buffer.
      thread_id,
      which_neighbor;                 // Identifies incoming message.

   double
      time_start;

   MG_REAL
      *recvbuffer, 
      *sendbuffer;

   MPI_Request
      msg_reqs[4];
      
   MPI_Status 
      recv_status;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   int gd = params.ghostdepth;

   ierr = MG_Set_diag_comm ( params, blk, count,
                             &bfd_xstart, &bfd_xend, &bfd_ystart, &bfd_yend, &ewd_start, &ewd_end );
   MG_Assert ( !ierr, "MG_Boundary_exchange_diags_SR:MG_Set_diag_comm" );

   ierr = MG_Get_tags ( params, blk, ivar, msgtag );
   MG_Assert ( !ierr, "MG_Boundary_exchange_SR::MG_Get_tags" );

   //MG_Barrier ( );

   recv_offset[NORTH] = 0;
   recv_offset[SOUTH] = count[NS];
   recv_offset[EAST]  = 0;
   recv_offset[WEST]  = count[EW];
   recv_offset[BACK]  = 0;
   recv_offset[FRONT] = count[FB];

   //MG_Barrier ( );
   //printf ( "[pe %d] recv_offsets = %d %d %d %d %d %d \n", mgpp.mype, recv_offset[NORTH], 
                //recv_offset[SOUTH], recv_offset[EAST], recv_offset[WEST], recv_offset[BACK], 
                //recv_offset[FRONT] );
   //MG_Barrier ( );

   // Assuming that these buffers cannot be permanently restricted to a particular task.

   maxlen = 2*count[NS];
   if ( count[EW] > maxlen ) {
      maxlen = 2*count[EW];
   }
   if ( count[FB] > maxlen ) {
      maxlen = 2*count[FB];
   }
   //printf ( "\n\n [pe %d] **** MAXLEN = %d \n\n", mgpp.mype, maxlen );
   
   //MG_Barrier ( );
   //printf ( "[pe %d] len = %d \n", mgpp.mype, len );
   //MG_Barrier ( );
    
   recvbuffer = (MG_REAL*)MG_CALLOC ( maxlen, sizeof(MG_REAL) );
   MG_Assert ( recvbuffer != NULL, "MG_Boundary_exchange_diags : MG_CALLOC ( recvbuffer )" );

   sendbuffer = (MG_REAL*)MG_CALLOC ( maxlen, sizeof(MG_REAL) );
   MG_Assert ( sendbuffer != NULL, "MG_Boundary_exchange_diags : MG_CALLOC ( sendbuffer )" );

   //MG_Barrier ( );
   //printf ( "[pe %d] MG_BEX_DIAGS: blk.neighbors=(%d,%d) (%d,%d) (%d,%d) \n\n", mgpp.mype, 
            //blk.neighbors[NORTH], blk.neighbors[SOUTH], blk.neighbors[EAST], 
            //blk.neighbors[WEST], blk.neighbors[BACK], blk.neighbors[FRONT] );
   //MG_Barrier ( );
   
   //printf ( "[pe %d] HERE 1 (%d=%d,%d=%d) \n", mgpp.mype, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );

   // ---------------------------------------
   // North-south exchange: send and complete.
   // ---------------------------------------

   MG_Time_start_l1(time_start);

   num_sends_outstanding = 0;
   num_recvs_outstanding = 0;

   msg_reqs[0] = MPI_REQUEST_NULL;
   msg_reqs[1] = MPI_REQUEST_NULL;
   msg_reqs[2] = MPI_REQUEST_NULL;
   msg_reqs[3] = MPI_REQUEST_NULL;

   if ( blk.neighbors[NORTH] != -1 ) { 
      
      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[NORTH]], count[NS], MG_COMM_REAL, 
                             blk.neighbors[NORTH], msgtag[sSrN], MPI_COMM_MG, &msg_reqs[RECVREQ_N] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(NORTH)" );
      
      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from NORTH \n", mgpp.mype );
   } 
   if ( blk.neighbors[SOUTH] != -1 ) { 
      
      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[SOUTH]], count[NS], MG_COMM_REAL, 
                             blk.neighbors[SOUTH], msgtag[sNrS], MPI_COMM_MG, &msg_reqs[RECVREQ_S] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(SOUTH)" );
      
      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from SOUTH \n", mgpp.mype );
   }  
   //printf ( "[pe %d] HERE 2 (%d=%d,%d=%d) \n", mgpp.mype, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );
   MG_Time_accum_l1 ( time_start, timings.recv[thread_id]);

   //int mm;
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   if ( blk.neighbors[NORTH] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, blk.yend, k);
             //printf ( "[pe %d] pack %4.2e for NORTH \n", mgpp.mype, grid_in(i, blk.yend, k) );
         }
      }
      MG_Time_accum_l1 ( time_start, timings.pack_north[thread_id]);
      MG_Time_start_l1( time_start );

      ierr = CALL_MPI_Isend ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                              msgtag[sNrS], MPI_COMM_MG, &msg_reqs[SENDREQ_N] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(NORTH)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_south[thread_id]);
   } 
   if ( blk.neighbors[SOUTH] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, gd, k);
             //printf ( "[pe %d] pack %4.2e for SOUTH \n", mgpp.mype, grid_in(i, gd, k) );
         }
      }
      MG_Time_accum_l1 ( time_start, timings.pack_south[thread_id]);
      MG_Time_start_l1( time_start );

      ierr = CALL_MPI_Isend ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH],
                              msgtag[sSrN], MPI_COMM_MG, &msg_reqs[SENDREQ_S] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(SOUTH)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_south[thread_id]);
   } 
   //}}
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   num_recvs = num_recvs_outstanding;
   num_sends = num_sends_outstanding;
 
   for ( m=0; m<num_sends+num_recvs; m++ ) {

      MG_Time_start_l1( time_start );

      //printf ( "[pe %d] NS Num send/recv outstanding = %d, %d \n", mgpp.mype, num_sends_outstanding, num_recvs_outstanding );
      ierr = CALL_MPI_Waitany ( 4, msg_reqs, &which_neighbor, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Waitany" );

      switch ( which_neighbor ) {

         case SENDREQ_N:
            MG_Time_accum_l1 ( time_start, timings.wait_send_north[thread_id]);
            num_sends_outstanding--;
            break;
         case SENDREQ_S:
            MG_Time_accum_l1 ( time_start, timings.wait_send_south[thread_id]);
            num_sends_outstanding--;
            break;
         case RECVREQ_N:
            MG_Time_accum_l1 ( time_start, timings.wait_recv_north[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[NORTH]; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, params.ny + gd, k) = recvbuffer[offset++];
                   //printf ( "[pe %d] UNPACK %4.2e from NORTH \n", mgpp.mype, grid_in(i, params.ny + gd, k) );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_north[thread_id]);

            break;

         case RECVREQ_S:

            MG_Time_accum_l1 ( time_start, timings.wait_recv_south[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[SOUTH]; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( i=blk.xstart; i<=blk.xend; i++ ) {
                  grid_in(i, 0, k) = recvbuffer[offset++];
                   //printf ( "[pe %d] UNPACK %4.2e from SOUTH \n", mgpp.mype, grid_in(i, 0, k) );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_south[thread_id]);

            break;

         default:

            fprintf ( stderr, "\n\n ** Error ** MG_Boundary_exchange_diags:unknown msg completion (%d). Should be NORTH or SOUTH, send or receive.\n\n", which_neighbor );
            MG_Assert ( !ierr, "MG_Boundary_exchange_diags" );
      }
   }
   MG_Assert ( num_recvs_outstanding == 0, 
              "MG_Boundary_exchange_diags (NORTH/SOUTH):num_recvs_outstanding != 0" );
   MG_Assert ( num_sends_outstanding == 0,
              "MG_Boundary_exchange_diags (NORTH/SOUTH):num_sends_outstanding != 0" );
   //}}

   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed NS exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );
   
   // -------------------------------------
   // East-west exchange: send and complete
   // -------------------------------------

MG_Barrier ();

   MG_Time_start_l1( time_start );

   num_sends_outstanding = 0;
   num_recvs_outstanding = 0;

   msg_reqs[0] = MPI_REQUEST_NULL;
   msg_reqs[1] = MPI_REQUEST_NULL;
   msg_reqs[2] = MPI_REQUEST_NULL;
   msg_reqs[3] = MPI_REQUEST_NULL;

   if ( blk.neighbors[EAST] != -1 ) {

      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[EAST]], count[EW], MG_COMM_REAL,
                              blk.neighbors[EAST], msgtag[sWrE], MPI_COMM_MG, &msg_reqs[RECVREQ_E] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(EAST)" );

      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from EAST \n", mgpp.mype);
   }
   if ( blk.neighbors[WEST] != -1 ) {

      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[WEST]], count[EW], MG_COMM_REAL,
                             blk.neighbors[WEST], msgtag[sErW], MPI_COMM_MG, &msg_reqs[RECVREQ_W] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(WEST)" );

      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from WEST \n", mgpp.mype);
   }
   MG_Time_accum_l1 ( time_start, timings.recv[thread_id]);

   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   if ( blk.neighbors[EAST] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart - ewd_start; j<=blk.yend + ewd_end; j++ ) {
            sendbuffer[offset++] = grid_in(blk.xend, j, k);
             //printf ( "[pe %d] pack %4.2e for EAST \n", mgpp.mype, grid_in(blk.xend, j, k) );
         }
      }

      MG_Time_accum_l1 ( time_start, timings.pack_east[thread_id]);

      ierr = CALL_MPI_Isend ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                            msgtag[sErW], MPI_COMM_MG, &msg_reqs[SENDREQ_E] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(EAST)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_east[thread_id]);
   } 
   
   if ( blk.neighbors[WEST] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=blk.ystart - ewd_start; j<=blk.yend + ewd_end; j++ ) {
            sendbuffer[offset++] = grid_in(gd, j, k);
             //printf ( "[pe %d] pack %4.2e for WEST \n", mgpp.mype, grid_in(gd, j, k) );
         }
      }
      MG_Time_accum_l1 ( time_start, timings.pack_west[thread_id]);
      MG_Time_start_l1( time_start );

      ierr = CALL_MPI_Isend ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[WEST],
                              msgtag[sWrE], MPI_COMM_MG, &msg_reqs[SENDREQ_W] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(WEST)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_west[thread_id]);
   } 
   //}}
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   num_recvs = num_recvs_outstanding;
   
   for ( m=0; m<num_sends+num_recvs; m++ ) {

      MG_Time_start_l1( time_start );

      //printf ( "[pe %d] EW Num send/recv outstanding = %d, %d \n", mgpp.mype, num_sends_outstanding, num_recvs_outstanding );
      ierr = CALL_MPI_Waitany ( 4, msg_reqs, &which_neighbor, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Waitany" );

      switch ( which_neighbor ) {

         case SENDREQ_E:
            MG_Time_accum_l1 ( time_start, timings.wait_send_north[thread_id]);
            num_sends_outstanding--;
            break;
         case SENDREQ_W:
            MG_Time_accum_l1 ( time_start, timings.wait_send_south[thread_id]);
            num_sends_outstanding--;
            break;
         case RECVREQ_E:
            MG_Time_accum_l1 ( time_start, timings.wait_recv_east[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[EAST]; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( j=blk.ystart - ewd_start; j<=blk.yend + ewd_end; j++ ) {
                  grid_in(params.nx + gd, j, k) = recvbuffer[offset++];
                   //printf ( "[pe %d] UNPACK %4.2e from EAST \n", mgpp.mype, grid_in(params.nx + gd, j, k) );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_east[thread_id]);

            break;

         case RECVREQ_W:

            MG_Time_accum_l1 ( time_start, timings.wait_recv_west[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[WEST]; 
            for ( k=blk.zstart; k<=blk.zend; k++ ) {
               for ( j=blk.ystart - ewd_start; j<=blk.yend + ewd_end; j++ ) {
                  grid_in(0, j, k) = recvbuffer[offset++];
                   //printf ( "[pe %d] UNPACK %4.2e from WEST \n", mgpp.mype, grid_in(0, j, k) );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_west[thread_id]);

            break;

         default:

            fprintf ( stderr, "\n\n ** Error ** MG_Boundary_exchange_diags:unknown msg completion (%d). Should be EAST or WEST, send or receive.\n\n", which_neighbor );
            MG_Assert ( !ierr, "MG_Boundary_exchange_diags" );
      }
   }
   MG_Assert ( num_recvs_outstanding == 0, 
              "MG_Boundary_exchange_diags (EAST/WEST):num_recvs_outstanding != 0" );
   MG_Assert ( num_sends_outstanding == 0,
              "MG_Boundary_exchange_diags (EAST/WEST):num_sends_outstanding != 0" );

   MG_Time_start_l1( time_start );
   //}} 
   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed EW exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );

   // --------------------------------------
   // Back-front exchange: send and complete
   // --------------------------------------

MG_Barrier ();
   MG_Time_start_l1( time_start );

   num_sends_outstanding = 0;
   num_recvs_outstanding = 0;

   msg_reqs[0] = MPI_REQUEST_NULL;
   msg_reqs[1] = MPI_REQUEST_NULL;
   msg_reqs[2] = MPI_REQUEST_NULL;
   msg_reqs[3] = MPI_REQUEST_NULL;

   if ( blk.neighbors[BACK] != -1 ) {

      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[BACK]], count[FB], MG_COMM_REAL,
                             blk.neighbors[BACK], msgtag[sFrB], MPI_COMM_MG, &msg_reqs[RECVREQ_B] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(BACK)" );

      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from BACK \n", mgpp.mype);
   }
   if ( blk.neighbors[FRONT] != -1 ) {

      ierr = CALL_MPI_Irecv ( &recvbuffer[recv_offset[FRONT]], count[FB], MG_COMM_REAL,
                             blk.neighbors[FRONT], msgtag[sBrF], MPI_COMM_MG, &msg_reqs[RECVREQ_F] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Irecv(FRONT)" );

      num_recvs_outstanding++;
       //printf ( "[pe %d] Post RECV from FRONT \n", mgpp.mype );
   }
   MG_Time_accum_l1 ( time_start, timings.recv[thread_id]);
 
    //printf ( "[pe %d] HERE 3 (%d=%d,%d=%d) \n", mgpp.mype, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );

   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   if ( blk.neighbors[BACK] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( j=blk.ystart - bfd_ystart; j<=blk.yend + bfd_yend; j++ ) {
         for ( i=blk.xstart - bfd_xstart; i<=blk.xend + bfd_xend; i++ )  {
            sendbuffer[offset++] = grid_in(i, j, blk.zend);
            //printf ( "[pe %d] pack %4.2e for BACK \n", mgpp.mype, grid_in(i, j, blk.zend) );
         }
      }
      MG_Time_accum_l1 ( time_start, timings.pack_back[thread_id]);
      MG_Time_start_l1( time_start );

      ierr = CALL_MPI_Isend ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[BACK],
                              msgtag[sBrF], MPI_COMM_MG, &msg_reqs[SENDREQ_B] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(BACK)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_back[thread_id]);
      MG_Time_start_l1( time_start );
   } 

   if ( blk.neighbors[FRONT] != -1 ) { 

      MG_Time_start_l1( time_start );

      offset = 0; 
      for ( j=blk.ystart - bfd_ystart; j<=blk.yend + bfd_yend; j++ ) {
         for ( i=blk.xstart - bfd_xstart; i<=blk.xend + bfd_xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, j, gd);
            //printf ( "[pe %d] pack %4.2e for FRONT \n", mgpp.mype, grid_in(i, j, gd) );
         }
      }
      MG_Time_accum_l1 ( time_start, timings.pack_front[thread_id]);
      MG_Time_start_l1( time_start );

      ierr = CALL_MPI_Isend ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                              msgtag[sFrB], MPI_COMM_MG, &msg_reqs[SENDREQ_F] );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Isend(FRONT)" );
      num_sends_outstanding++;

      MG_Time_accum_l1 ( time_start, timings.send_front[thread_id]);
   } 
   //}}
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   num_recvs = num_recvs_outstanding;
   num_sends = num_sends_outstanding;
   
   for ( m=0; m<num_sends+num_recvs; m++ ) {

      MG_Time_start_l1( time_start );

      //printf ( "[pe %d] FB Num send/recv outstanding = %d, %d \n", mgpp.mype, num_sends_outstanding, num_recvs_outstanding );
      ierr = CALL_MPI_Waitany ( 4, msg_reqs, &which_neighbor, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Waitany" );

      MG_Time_start_l1( time_start );

      switch ( which_neighbor ) {

         case SENDREQ_B:
            MG_Time_accum_l1 ( time_start, timings.wait_send_north[thread_id]);
            num_sends_outstanding--;
            break;
         case SENDREQ_F:
            MG_Time_accum_l1 ( time_start, timings.wait_send_south[thread_id]);
            num_sends_outstanding--;
            break;
         case RECVREQ_B:
            MG_Time_accum_l1 ( time_start, timings.wait_recv_back[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[BACK]; 
            for ( j=blk.ystart - bfd_ystart; j<=blk.yend + bfd_yend; j++ ) {
               for ( i=blk.xstart - bfd_xstart; i<=blk.xend + bfd_xend; i++ ) {
                  grid_in(i, j, 0) = recvbuffer[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from BACK \n", mgpp.mype, grid_in(i, j, 0) );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_back[thread_id]);
            break;

         case RECVREQ_F:
            MG_Time_accum_l1 ( time_start, timings.wait_recv_front[thread_id]);
            MG_Time_start_l1( time_start );

            num_recvs_outstanding--;
            offset = recv_offset[FRONT];
             //printf ( "[pe %d] UNPACKING offset=%d, (%d=%d,%d=%d) \n", mgpp.mype, offset, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );
            for ( j=blk.ystart - bfd_ystart; j<=blk.yend + bfd_yend; j++ ) {
               for ( i=blk.xstart - bfd_xstart; i<=blk.xend + bfd_xend; i++ ) {
                  grid_in(i, j, params.nz + gd) = recvbuffer[offset++];
                  //printf ( "[pe %d] UNPACK %4.2e from FRONT; offset=%d \n", mgpp.mype, grid_in(i, j, params.nz + gd), offset );
               }
            }
            MG_Time_accum_l1 ( time_start, timings.unpack_front[thread_id]);
            break;

      }
   }
   MG_Assert ( num_sends_outstanding == 0,
              "MG_Boundary_exchange_diags (BACK/FRONT): num_sends_outstanding != 0" );
   MG_Assert ( num_recvs_outstanding == 0, 
              "MG_Boundary_exchange_diags (BACK/FRONT): num_recvs_outstanding != 0" );
   //}}
   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed FB exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );

   // Boundary exchange complete, release workspace.

   if ( recvbuffer )
      free ( recvbuffer );

   if ( sendbuffer )
      free ( sendbuffer );

   return ( ierr );

} // End MG_Boundary_exchange_diags.
