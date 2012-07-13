//********************************************************************************
// Copyright (c) 2007-2010 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************

//[tile m_tiles <tile_tag>]
  // This item collection contains one item for each tile of the input matrix
  //   and each computed matrix. The tag consists of three integers. The first
  //   integer indicates the matrix, and the second and third integers indicate
  //   the tile row and column. The user code allocates an array for each tile
  //   outside of the runtime system, and passes a pointer to the array as the
  //   data argument for the Put
   
//(compute_inverse)
  // Each step in this step collection computes a single tile for one of the
  //   computed matrices. The tag consists of three integers. The first integer
  //   indicates the matrix, while the second and third integers indicated the
  //   the tile row and column

// Declarations
[tile m_tiles <tile_tag>];
<tile_tag m_steps>;
(compute_inverse)tuner=my_tuner;

// Relations
env -> [m_tiles], <m_steps>;
<m_steps> :: (compute_inverse);
[m_tiles] -> (compute_inverse) -> [m_tiles];
[m_tiles] -> env;
