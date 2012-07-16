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

#ifndef _H_CHOLESKY_TYPES_INCLUDED_H_
#define _H_CHOLESKY_TYPES_INCLUDED_H_

#define CNC_PRECOND
#define CNC_PRECOND_ONLY
//#define DISABLE_GET_COUNTS
#define USE_CHEATING

#include <cnc/cnc.h>
#include <iostream>
#include <cassert>
#include <memory>
//#include <tr1/memory> 


//using namespace CnC;

typedef std::pair<int,int> pair;
// don't use a vector: tags are copied and vector-copy is expensive
class triple
{
public:
    triple() { m_arr[0] = m_arr[1] = m_arr[2] = 0; }
    triple( int a, int b, int c ) { m_arr[0] = a; m_arr[1] = b; m_arr[2] = c; }
    inline int operator[]( int i )  const { return m_arr[i]; }
    
    friend std::ostream& operator<< (std::ostream& stream, triple& tr);

private:
    int m_arr[3];
};





template <>
class cnc_tag_hash_compare< triple >
{
public:
    size_t hash( const triple & t ) const
    {
        return t[0] + ( t[1] << 6 ) + ( t[2] << 11 );
    }
    bool equal( const triple & a, const triple & b) const { 
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; 
    }
};

inline std::ostream & cnc_format( std::ostream& os, const triple & t )
{
    os << "[ " << t[0] << ", " << t[1] << ", " << t[2] << " ]";
    return os;
}

template< typename T >
class Tile
{
public:
	static int counter;
    Tile( int sz = 0 ) : m_sz( sz ), m_array( NULL )/*, m_full( true )*/
    {
		if( sz ) {
    		//CnC::serializer::construct_array< T >( m_array, sz*sz );
    		m_array = new T[sz*sz];
    		for(int i=0; i<sz*sz; i++) {
    			m_array[i] = T();
    		}
    	}
    }
    ~Tile() {
		counter++;
		delete m_array;
    }
#define TOI( _i, _j, _s ) ((_j)*(_s)+(_i))
    inline T operator()( int i, int j ) const { return m_array[TOI(i,j,m_sz)]; }
    inline T & operator()( int i, int j ) { return m_array[TOI(i,j,m_sz)]; }
private:
    Tile( const Tile< T > & ) { assert( 0 ); }
    Tile & operator=( const Tile< T > & ) { assert( 0 ); return *this; }
    int   m_sz;
    T   * m_array;

};

typedef Tile< double > tile_type;
#ifdef __INTEL_COMPILER
typedef const tile_type* tile_const_ptr_type;
typedef       tile_type* tile_ptr_type;

inline tile_ptr_type make_shared_tile(int b){
    tile_type* ret = new tile_type(b);
    return ret;
}
#else
typedef std::shared_ptr< const tile_type > tile_const_ptr_type;
typedef std::shared_ptr< tile_type > tile_ptr_type;

inline tile_ptr_type make_shared_tile(int b){
    return std::make_shared< tile_type >(b);
}
#endif

#endif //_H_CHOLESKY_TYPES_INCLUDED_H_
