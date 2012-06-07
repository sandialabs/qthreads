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
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

// Declarations
[double** Lkji <triple>: k,j,i];    // The input/result matrix

[int p <int>];              // Tile loop end value
[int b <int>];              // Tile size 

// Tags
<int singleton>;		
<int control_S1: k>;        // Tag values are indices of Step 1 [k = 0...p-1]
<pair control_S2: k,j>;     // Tag values are indices of Step 2 [j = k+1...p-1]
<triple control_S3: k,j,i>; // Tag values are indices of Step 3 [i = k+1...j]


// Step Prescriptions
<singleton> :: (k_compute);		
<control_S1> :: (S1_compute),(kj_compute);		
<control_S2> :: (S2_compute),(kji_compute);
<control_S3> :: (S3_compute);


// Input from the caller: tile pointers, tile size and loop end value
env -> [Lkji], [p: sTag], [b: sTag], <singleton>;

// Step execution
// The k_compute step produces 'k' loop indices (in the form of tag instance)
[p: sTag] -> (k_compute);
(k_compute) -> <control_S1>;

// The kj_compute step produces 'j' loop indices (in the form of tag instance)
[p: sTag] -> (kj_compute);
(kj_compute) -> <control_S2>;

// The kji_compute step produces 'i' loop indices (in the form of tag instance)
[p: sTag] -> (kji_compute);
(kji_compute) -> <control_S3>;

// Step 1 Executions
[Lkji],
[b: sTag] -> (S1_compute);

(S1_compute) -> [Lkji];

// Step 2 Executions
[Lkji],
[b: sTag] -> (S2_compute);

(S2_compute) -> [Lkji];

// Step 3 Executions
[Lkji],
[b: sTag] -> (S3_compute);

(S3_compute) -> [Lkji];
 

// Return to the caller
[Lkji] -> env;
