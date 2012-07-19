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
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CNC_PRECOND
#define CNC_PRECOND_ONLY

#include <cnc/cnc.h>
#include <qthread/qtimer.h>

struct my_context;

struct FindPrimes
{
    int execute( int n, my_context & c ) const;
    aligned_t** get_dependences(const int & t, my_context & c, int & no ) const;
};


struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< FindPrimes > 				m_steps;
    CnC::tag_collection<int>                        m_tags;
    CnC::item_collection<int, int>    				m_primes;
    my_context() 
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_tags( *this ),
          m_primes( *this )
    {
        m_tags.prescribes( m_steps, *this );
    }
};

aligned_t** FindPrimes::get_dependences(const int & t, my_context & c, int & no ) const
{
	return NULL;
}

int FindPrimes::execute( int n, my_context & c ) const
{
    int factor = 3;
    while ( n % factor ) factor += 2;
    if (factor == n) c.m_primes.put(n, n);

    return CnC::CNC_Success;
}


int main(int argc, char* argv[])
{
    //bool verbose = false;
    int n = 0;
    //int number_of_primes = 0;

    if (argc >= 2) 
    {
        n = atoi(argv[1]);
    }
    else if (argc == 3 && 0 == strcmp("-v", argv[1]))
    {
        n = atoi(argv[2]);
        //verbose = true;
    }
    else
    {
        fprintf(stderr,"Usage: primes [-v] n\n");
        return -1;
    }

    my_context c;

    printf("Determining primes from 3-%d...\n",n);
	qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);

    for (int number = 3; number < n; number += 2)
        {
            c.m_tags.put(number);
        }

    c.wait();

	qtimer_stop(timer);
    total_time = qtimer_secs(timer);
    
    printf("Time(s): %.3f\n", total_time);
    qtimer_destroy(timer);

    //Size not implemented yet;
    //number_of_primes = (int)c.m_primes.size() + 1;

	CnC::item_collection<int, int>::const_iterator cii_b = c.m_primes.begin();
	CnC::item_collection<int, int>::const_iterator cii_e = c.m_primes.end();
	int no_primes = 0;
	for (CnC::item_collection<int, int>::const_iterator cii = cii_b; cii != cii_e; cii++) 
	{
		no_primes++;
		//printf("%d\n", (*cii).first); 
	}
    if (argc > 2) 
    {
        int expected_primes = atoi(argv[2]);
        assert(no_primes == expected_primes);
        printf("OK.\n");
    }   
        
        
    
}

