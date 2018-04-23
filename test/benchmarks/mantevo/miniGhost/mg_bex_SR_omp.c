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

int MG_Boundary_exchange_SR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                // Return status
      count = -1,                   // Message size in datatype elements.
      i, j, k,                 // Counters
      msgtag[MAX_NUM_NEIGHBORS],
      offset;                  // Offset for packing and unpacking msg buffers.

   MG_REAL
      *recvbuffer, 
      *sendbuffer;

   MPI_Status 
      recv_status;

   //int
   //    rank;
   //MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   int
      thread_id;

   double
      time_start,
      time_waiting;


   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

#if defined _MG_DEBUG
      printf ( "[pe %d] ============== Enter boundary exchange ============== \n", mgpp.mype );
#endif

   ierr = MG_Get_tags ( params, blk, ivar, msgtag );
   MG_Assert ( !ierr, "MG_Boundary_exchange_SR::MG_Get_tags" );

   // Assuming that these buffers cannot be permanently restricted to a particular task.
   recvbuffer = (MG_REAL*)MG_CALLOC ( max_msg_count, sizeof(MG_REAL) );
   MG_Assert ( recvbuffer != NULL, "MG_Boundary_exchange_SR : MG_CALLOC ( recvbuffer )" );

   sendbuffer = (MG_REAL*)MG_CALLOC ( max_msg_count, sizeof(MG_REAL) );
   MG_Assert ( sendbuffer != NULL, "MG_Boundary_exchange_SR : MG_CALLOC ( sendbuffer )" );

   int gd = params.ghostdepth;

   MG_Barrier ();

   if ( blk.neighbors[NORTH] != -1 ) {

      // 1. Pack north face
      // 2. Send north face to NORTH neighbor
      // 3. Recv south face from NORTH neighbor
      // 4. Unpack (south frace from NORTH neighbor) into north gz

      int const north_face_idx    = blk.yend;
      int const north_face_gz_idx = blk.yend + 1;

      MG_Time_start_l1(time_start);

      //printf("[%03d] Packing north face y[%d]\n", rank, north_face_idx);

#pragma omp parallel private(offset, k, i)
{
      offset = params.thread_offset_xz[omp_get_thread_num()]; // Pack send buffer.
      //printf ( "[thread %d] BEX: offset = %d \n", omp_get_thread_num(), offset );
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, north_face_idx, k);
}
      MG_Time_accum_l1(time_start,timings.pack_north[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.zend - blk.zstart + 1 ) * ( blk.xend - blk.xstart + 1 );
#if defined _MG_DEBUG
      printf ( "N/S pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[NORTH], count );
#endif

      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[NORTH], 
                            msgtag[sNrS], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(NORTH)" );

      MG_Time_accum_l1(time_start,timings.send_north[thread_id]);

#if defined _MG_DEBUG
      printf ( "N/S pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[NORTH], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[NORTH], 
                            msgtag[sSrN], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(NORTH)" );

      MG_Time_accum_l1(time_start,timings.recv_north[thread_id]);


      MG_Time_start_l1(time_start);

      //printf("[%03d] Unpacking south face from %d into y[%d]\n", rank, blk.neighbors[NORTH], north_face_gz_idx);

#pragma omp parallel private(offset, k, i)
{
      offset = params.thread_offset_xz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, north_face_gz_idx, k) = recvbuffer[offset++];
}

      MG_Time_accum_l1(time_start,timings.unpack_north[thread_id]);

   } // end NORTH neighbor.

   if ( blk.neighbors[SOUTH] != -1 ) {

      // 1. Pack south face
      // 2. Recv north face from SOUTH neighbor
      // 3. Send south face to SOUTH neighbor
      // 4. Unpack (north face from SOUTH neighbor) into south gz

      int const south_face_idx    = blk.ystart;
      int const south_face_gz_idx = blk.ystart - 1;

      MG_Time_start_l1(time_start);

      //printf("[%03d] Packing south face y[%d]\n", rank, south_face_idx);

#pragma omp parallel private(offset, k, i)
{
      offset = params.thread_offset_xz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, south_face_idx, k);
}
      MG_Time_accum_l1(time_start,timings.pack_south[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.zend - blk.zstart + 1 ) * ( blk.xend - blk.xstart + 1 );
#if defined _MG_DEBUG
      printf ( "S/N pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[SOUTH], count );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[SOUTH], 
                            msgtag[sNrS], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.recv_south[thread_id]);

#if defined _MG_DEBUG
      printf ( "S/N pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[SOUTH], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[SOUTH], 
                            msgtag[sSrN], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.send_south[thread_id]);

      MG_Time_start_l1(time_start);

      //printf("[%03d] Unpacking north face from %d into y[%d]\n", rank, blk.neighbors[SOUTH], south_face_gz_idx);

#pragma omp parallel private(offset, k, i)
{     
      offset = params.thread_offset_xz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, south_face_gz_idx, k) = recvbuffer[offset++];
}
      MG_Time_accum_l1(time_start,timings.unpack_south[thread_id]);

   } // end SOUTH neighbor.

#if defined _MG_DEBUG
   printf ( " ==================== [pe %d] Completed NS =================== \n ", mgpp.mype );

   printf ( "[pe %d] EAST/WEST:xend %d %d neigh %d \n", mgpp.mype, blk.xend, params.nx + gd - 1, neighbors[EAST] );
#endif

   if ( blk.neighbors[EAST] != -1 ) {

      // 1. Pack east face
      // 2. Send east face to EAST neighbor
      // 3. Recv west face from EAST neighbor
      // 4. Unpack (west face from EAST neighbor) into east gz

      int const east_face_idx    = blk.xend;
      int const east_face_gz_idx = blk.xend + 1;

      MG_Time_start_l1(time_start);

      //printf("[%03d] Packing east face x[%d]\n", rank, east_face_idx);

#pragma omp parallel private(offset, k, j)
{     
      offset = params.thread_offset_yz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            sendbuffer[offset++] = grid_in(east_face_idx, j, k);
}
      MG_Time_accum_l1(time_start,timings.pack_east[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.zend - blk.zstart + 1 ) * ( blk.yend - blk.ystart + 1 );
#if defined _MG_DEBUG
      printf ( "E/W pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[EAST], count );
#endif
      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[EAST], 
                            msgtag[sErW], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(EAST)" );

      MG_Time_accum_l1(time_start,timings.send_east[thread_id]);

#if defined _MG_DEBUG
      printf ( "E/W pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[EAST], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[EAST], 
                            msgtag[sWrE], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(EAST)" );

      MG_Time_accum_l1(time_start,timings.recv_east[thread_id]);

      MG_Time_start_l1(time_start);

      //printf("[%03d] Unpacking west face from %d into x[%d]\n", rank, blk.neighbors[EAST], east_face_gz_idx);
#pragma omp parallel private(offset, k, j)
{     
      offset = params.thread_offset_yz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            //grid_in(params.nx + gd, j, k) = recvbuffer[offset++];
            grid_in(east_face_gz_idx, j, k) = recvbuffer[offset++];
}
      MG_Time_accum_l1(time_start,timings.unpack_east[thread_id]);
   } // end EAST neighbor.

   if ( blk.neighbors[WEST] != -1 ) {

      // 1. Pack west face
      // 2. Recv east face from WEST neighbor
      // 3. Send west face to WEST neighbor
      // 4. Unpack (east face from WEST neighbor) into west gz

      int const west_face_idx    = blk.xstart;
      int const west_face_gz_idx = blk.xstart - 1;

      MG_Time_start_l1(time_start);

      //printf("[%03d] Packing west face x[%d]\n", rank, west_face_idx);

#pragma omp parallel private(offset, k, j)
{     
      offset = params.thread_offset_yz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            sendbuffer[offset++] = grid_in(west_face_idx, j, k);
}
      MG_Time_accum_l1(time_start,timings.pack_west[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.zend - blk.zstart + 1 ) * ( blk.yend - blk.ystart + 1 );
#if defined _MG_DEBUG
      printf ( "W/E pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[WEST], count );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[WEST], 
                            msgtag[sErW], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(WEST)" );

      MG_Time_accum_l1(time_start,timings.recv_west[thread_id]);

#if defined _MG_DEBUG
      printf ( "W/E pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[WEST], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[WEST], 
                            msgtag[sWrE], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(WEST)" );

      MG_Time_accum_l1(time_start,timings.send_west[thread_id]);

      MG_Time_start_l1(time_start);

      //printf("[%03d] Unpacking east face from %d into x[%d]\n", rank, blk.neighbors[WEST], west_face_gz_idx);

#pragma omp parallel private(offset, k, j)
{     
      offset = params.thread_offset_yz[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( k=blk.zstart; k<=blk.zend; k++ )
         for ( j=blk.ystart; j<=blk.yend; j++ )
            grid_in(west_face_gz_idx, j, k) = recvbuffer[offset++];
}
      MG_Time_accum_l1(time_start,timings.unpack_west[thread_id]);

   } // end WEST neighbor.

#if defined _MG_DEBUG
    printf ( " ==================== [pe %d] Completed EW =================== \n ", mgpp.mype );
#endif

   if ( blk.neighbors[BACK] != -1 ) {

      // 1. Pack back face
      // 2. Recv front face of BACK neighbor
      // 3. Send back face to BACK neighbor
      // 4. Unpack (front face from BACK neighbor) into back gz

      int const back_face_idx    = blk.zend;
      int const back_face_gz_idx = blk.zend + 1;

      MG_Time_start_l1(time_start);

#pragma omp parallel private(offset, j, i)
{     
      offset = params.thread_offset_xy[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ ) 
            sendbuffer[offset++] = grid_in(i, j, back_face_idx);
}
      MG_Time_accum_l1(time_start,timings.pack_back[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.yend - blk.ystart + 1 ) * ( blk.xend - blk.xstart + 1 );
#if defined _MG_DEBUG
      printf ( "B/F pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[BACK], count );
#endif
      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[BACK], 
                            msgtag[sFrB], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(BACK)" );

      MG_Time_accum_l1(time_start,timings.recv_back[thread_id]);

#if defined _MG_DEBUG
      printf ( "B/F pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[BACK], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[BACK], 
                            msgtag[sBrF], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(BACK)" );

      MG_Time_accum_l1(time_start,timings.send_back[thread_id]);


      MG_Time_start_l1(time_start);

      //printf("[%03d] Unpacking front face from %d into z[%d]\n", rank, blk.neighbors[BACK], blk.zend+1);

#pragma omp parallel private(offset, j, i)
{     
      offset = params.thread_offset_xy[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, j, back_face_gz_idx) = recvbuffer[offset++];
}
      MG_Time_accum_l1(time_start,timings.unpack_back[thread_id]);

   } // end BACK neighbor.

   if ( blk.neighbors[FRONT] != -1 ) { // Exchange boundary with FRONT neighbor.

      // 1. Pack front face
      // 2. Send front face to FRONT neighbor
      // 3. Recv back face from FRONT neighbor
      // 4. Unpack (back face from FRONT neighbor) into front gz

      int const front_face_idx    = blk.zstart;
      int const front_face_gz_idx = blk.zstart - 1;

      MG_Time_start_l1(time_start);

#pragma omp parallel private(offset, j, i)
{     
      offset = params.thread_offset_xy[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            sendbuffer[offset++] = grid_in(i, j, front_face_idx);
}
      MG_Time_accum_l1(time_start,timings.pack_front[thread_id]);

      MG_Time_start_l1(time_start);

      count = ( blk.yend - blk.ystart + 1 ) * ( blk.xend - blk.xstart + 1 );
#if defined _MG_DEBUG
      printf ( "F/B pe %d sending to pe %d count %d msg \n",
               mgpp.mype, neighbors[FRONT], count );
#endif
      ierr = CALL_MPI_Send ( sendbuffer, count, MG_COMM_REAL, neighbors[FRONT], 
                             msgtag[sFrB], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Send(FRONT)" );

      MG_Time_accum_l1(time_start,timings.send_front[thread_id]);

#if defined _MG_DEBUG
      printf ( "F/B pe %d recving from %d count %d msg \n",
               mgpp.mype, neighbors[FRONT], count );
#endif
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count, MG_COMM_REAL, neighbors[FRONT], 
                            msgtag[sBrF], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_SR:CALL_MPI_Recv(FRONT)" );

      MG_Time_accum_l1(time_start,timings.recv_front[thread_id]);

      MG_Time_start_l1(time_start);

#pragma omp parallel private(offset, j, i)
{     
      offset = params.thread_offset_xy[omp_get_thread_num()]; // Pack send buffer.
#pragma omp for
      for ( j=blk.ystart; j<=blk.yend; j++ )
         for ( i=blk.xstart; i<=blk.xend; i++ )
            grid_in(i, j, front_face_gz_idx) = recvbuffer[offset++];
}
      MG_Time_accum_l1(time_start,timings.unpack_front[thread_id]);

   } // end FRONT neighbor.

#if defined _MG_DEBUG
       printf ( " ==================== [pe %d] Completed FB; exit BEX =================== \n ", mgpp.mype );
#endif

   if ( recvbuffer != NULL )
      free ( recvbuffer );

   if ( sendbuffer != NULL )
      free ( sendbuffer );

   ierr = 0;   // MPI_SUCCESS need not = 0.

   return ( ierr );

} // End MG_Boundary_exchange_SR.
