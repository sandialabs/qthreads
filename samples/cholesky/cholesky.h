//********************************************************************************
// Copyright (c) 2007-2012 Intel Corporation. All rights reserved.              **
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
#ifndef cholesky_H_ALREADY_INCLUDED
#define cholesky_H_ALREADY_INCLUDED

#include <memory>

// Forward declaration of the context class (also known as graph)
struct cholesky_context;

// The step classes

struct S1_compute
{
    int execute( const int & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const int & t, cholesky_context & c, int & no ) const;
};

struct S2_compute
{
    int execute( const pair & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const pair & t, cholesky_context & c, int & no ) const;
};

struct S3_compute
{
    int execute( const triple & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const triple & t, cholesky_context & c, int & no ) const;
};

struct k_compute
{
    int execute( const int & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const int & t, cholesky_context & c, int & no ) const;
};

struct kj_compute
{
    int execute( const int & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const int & t, cholesky_context & c, int & no ) const;
};

struct kji_compute
{
    int execute( const pair & t, cholesky_context & c ) const;
    aligned_t** get_dependences(const pair & t, cholesky_context & c, int & no ) const;
};

// The context class
struct cholesky_context : public CnC::context< cholesky_context >
{
    

    // Step Collections
    CnC::step_collection< S1_compute > sc_s1_compute;
    CnC::step_collection< S2_compute > sc_s2_compute;
    CnC::step_collection< S3_compute > sc_s3_compute;
    CnC::step_collection< k_compute > sc_k_compute;
    CnC::step_collection< kj_compute > sc_kj_compute;
    CnC::step_collection< kji_compute > sc_kji_compute;

    // Item collections
    CnC::item_collection< triple, tile_const_ptr_type > Lkji;
    int p,b;

    // Tag collections
    CnC::tag_collection< int > control_S1;
    CnC::tag_collection< pair > control_S2;
    CnC::tag_collection< triple > control_S3;
    CnC::tag_collection< int > singleton;

    // The context class constructor
    cholesky_context( int _b = 0, int _p = 0, int _n = 0 )
        : CnC::context< cholesky_context >(1),
          
          
          // init step colls
          sc_s1_compute( *this ),
          sc_s2_compute( *this ),
          sc_s3_compute( *this ),
          sc_k_compute( *this ),
          sc_kj_compute( *this ),
          sc_kji_compute( *this ),
          // Initialize each item collection
          Lkji( *this ),
          p( _p ),
          b( _b ),
          // Initialize each tag collection
          control_S1( *this ),
          control_S2( *this ),
          control_S3( *this ),
          singleton( *this )
    {
        // Prescriptive relations
        singleton.prescribes( sc_k_compute, *this );
        control_S1.prescribes( sc_s1_compute, *this );
        control_S1.prescribes( sc_kj_compute, *this );
        control_S2.prescribes( sc_s2_compute, *this );
        control_S2.prescribes( sc_kji_compute, *this );
        control_S3.prescribes( sc_s3_compute, *this );

        // control relations
        //sc_k_compute.controls( control_S1 );
        //sc_kj_compute.controls( control_S2 );
        //sc_kji_compute.controls( control_S3 );
        
    }


};

#endif // cholesky_H_ALREADY_INCLUDED
