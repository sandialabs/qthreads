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

#ifndef _H_filesystem_TYPES_INCLUDED_H_
#define _H_filesystem_TYPES_INCLUDED_H_

double t;
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


/* start: extra types for strict version */
class triple_epilog
{
public:
    triple_epilog() { m_arr[0] = m_arr[1] = m_arr[2] = 0; }
    triple_epilog( int a, int b, int c, inode* f, inode* s ) { m_arr[0] = a; m_arr[1] = b; m_arr[2] = c; first = f; second = s; }
    inline int operator[]( int i )  const { return m_arr[i]; }
    
    friend std::ostream& operator<< (std::ostream& stream, triple_epilog& tr);
	inode* first;
	inode* second;
private:
    int m_arr[3];
};





template <>
class cnc_tag_hash_compare< triple_epilog >
{
public:
    size_t hash( const triple_epilog & t ) const
    {
        return t[0] + ( t[1] << 6 ) + ( t[2] << 11 );
    }
    bool equal( const triple_epilog & a, const triple_epilog & b) const { 
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; 
    }
};

class triple_epilog2
{
public:
    triple_epilog2() { m_arr[0] = m_arr[1] = m_arr[2] = 0; s1i = NULL; s2i = NULL; }
    triple_epilog2( int a, int b, int c, inode* f, inode* s, block* b1, block* b2 ) 
	{ 
		m_arr[0] = a; m_arr[1] = b; m_arr[2] = c; 
		first = f; second = s; 
		s1i = b1;
		s2i = b2;
	}
    inline int operator[]( int i )  const { return m_arr[i]; }
    
    friend std::ostream& operator<< (std::ostream& stream, triple_epilog2& tr);
	inode* first;
	inode* second;
	block* s1i;
	block* s2i;
private:
    int m_arr[3];
};





template <>
class cnc_tag_hash_compare< triple_epilog2 >
{
public:
    size_t hash( const triple_epilog2 & t ) const
    {
        return t[0] + ( t[1] << 6 ) + ( t[2] << 11 );
    }
    bool equal( const triple_epilog2 & a, const triple_epilog2 & b) const { 
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; 
    }
};
/* end extra types for strict versions */

int my_usleep(unsigned int i) {
    int k;
    for (int j = 0; j < i ; j++) {
	k = rand_r(&i);
    }
    return k;
}


#endif //_H_filesystem_TYPES_INCLUDED_H_
