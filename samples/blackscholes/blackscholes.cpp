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

//********************************************************************************
// Copyright (c) 2006-2007 Princeton University
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Princeton University nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//********************************************************************************

#include <cstdio>
#include <math.h>

#if _WIN32||_WIN64
#include <windows.h>
#endif


//Precision to use for calculations
#define fptype float
#define ERR_CHK
#define NUM_RUNS 100

typedef struct OptionData_ {
	fptype s;			// spot price
	fptype strike;		// strike price
	fptype r;			// risk-free interest rate
	fptype divq;		// dividend rate
	fptype v;			// volatility
	fptype t;			// time to maturity or option expiration in years 
						//     (1yr = 1.0, 6mos = 0.5, 3mos = 0.25, ..., etc)  
	char   OptionType;	// Option type.  "P"=PUT, "C"=CALL
	fptype divs;		// dividend vals (not used in this test)
	fptype DGrefval;	// DerivaGem Reference Value
} OptionData;


#include "blackscholes.h"

//++  modified by xiongww

#ifndef __GNUC__
//disable float to double conversion warning
#pragma warning( disable : 4305 )
#pragma warning( disable : 4244 )
#endif
OptionData data_init[] = {
	#include "optionData.txt"
};

//--

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Cumulative Normal Distribution Function
// See Hull, Section 11.8, P.243-244
#define inv_sqrt_2xPI 0.39894228040143270286

fptype CNDF ( fptype InputX ) 
{
	int sign;

	fptype OutputX;
	fptype xInput;
	fptype xNPrimeofX;
	fptype expValues;
	fptype xK2;
	fptype xK2_2, xK2_3;
	fptype xK2_4, xK2_5;
	fptype xLocal, xLocal_1;
	fptype xLocal_2, xLocal_3;

	// Check for negative value of InputX
	if (InputX < 0.0) {
		InputX = -InputX;
		sign = 1;
	} else 
		sign = 0;

	xInput = InputX;
 
    // Compute NPrimeX term common to both four & six decimal accuracy calcs
	expValues = exp(-0.5f * InputX * InputX);
	xNPrimeofX = expValues;
	xNPrimeofX = xNPrimeofX * inv_sqrt_2xPI;

	xK2 = 0.2316419 * xInput;
	xK2 = 1.0 + xK2;
	xK2 = 1.0 / xK2;
	xK2_2 = xK2 * xK2;
	xK2_3 = xK2_2 * xK2;
	xK2_4 = xK2_3 * xK2;
	xK2_5 = xK2_4 * xK2;
    
	xLocal_1 = xK2 * 0.319381530;
	xLocal_2 = xK2_2 * (-0.356563782);
	xLocal_3 = xK2_3 * 1.781477937;
	xLocal_2 = xLocal_2 + xLocal_3;
	xLocal_3 = xK2_4 * (-1.821255978);
	xLocal_2 = xLocal_2 + xLocal_3;
	xLocal_3 = xK2_5 * 1.330274429;
	xLocal_2 = xLocal_2 + xLocal_3;

	xLocal_1 = xLocal_2 + xLocal_1;
	xLocal   = xLocal_1 * xNPrimeofX;
	xLocal   = 1.0 - xLocal;

	OutputX  = xLocal;
    
	if (sign) {
		OutputX = 1.0 - OutputX;
	}
    
	return OutputX;
} 


//////////////////////////////////////////////////////////////////////////////////////
fptype BlkSchlsEqEuroNoDiv( fptype sptprice,
							fptype strike, fptype rate, fptype volatility,
							fptype time, int otype, float timet )    
{
	fptype OptionPrice;

	// local private working variables for the calculation
	//fptype xStockPrice;
	//fptype xStrikePrice;
	fptype xRiskFreeRate;
	fptype xVolatility;
	fptype xTime;
	fptype xSqrtTime;

	fptype logValues;
	fptype xLogTerm;
	fptype xD1; 
	fptype xD2;
	fptype xPowerTerm;
	fptype xDen;
	fptype d1;
	fptype d2;
	fptype FutureValueX;
	fptype NofXd1;
	fptype NofXd2;
	fptype NegNofXd1;
	fptype NegNofXd2;    
    
	//xStockPrice = sptprice;
	//xStrikePrice = strike;
	xRiskFreeRate = rate;
	xVolatility = volatility;

	xTime = time;
	xSqrtTime = sqrt(xTime);

	logValues = log( sptprice / strike );
        
	xLogTerm = logValues;
        
	xPowerTerm = xVolatility * xVolatility;
	xPowerTerm = xPowerTerm * 0.5;
	xDen = xVolatility * xSqrtTime;

    xD1 = xRiskFreeRate + xPowerTerm;
	xD1 = xD1 * xTime;
	xD1 = xD1 + xLogTerm;
	xD1 = xD1 / xDen;
	xD2 = xD1 -  xDen;

	d1 = xD1;
	d2 = xD2;
    
	NofXd1 = CNDF( d1 );
	NofXd2 = CNDF( d2 );

	FutureValueX = strike * ( exp( -(rate)*(time) ) );        
	if (otype == 0) {            
		OptionPrice = (sptprice * NofXd1) - (FutureValueX * NofXd2);
	} else { 
		NegNofXd1 = (1.0 - NofXd1);
		NegNofXd2 = (1.0 - NofXd2);
		OptionPrice = (FutureValueX * NegNofXd2) - (sptprice * NegNofXd1);
	}
    
	return OptionPrice;
}

//////////////////////////////////////////////////////////////////////////////////////
aligned_t** Compute::get_dependences(const int & t, blackscholes_context & c, int & no ) const
{
	no = 1;
	aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
	c.opt_data.wait_on(t, &read[0]);
	return read;
}



int Compute::execute(const int & tag, blackscholes_context& c ) const
{
    option_vector_type opt_vec;
    c.opt_data.get( tag, opt_vec );
    price_vector_type prices = std::make_shared< std::vector< fptype > >( opt_vec->size() );
    auto pi = prices->begin();

    for( auto i = opt_vec->begin(); i != opt_vec->end(); ++i, ++pi ) {
        for( int j = 0; j < NUM_RUNS; j++ ) {
            (*pi) = BlkSchlsEqEuroNoDiv( i->s, i->strike, i->r, i->v, i->t, i->OptionType == 'P' ? 1 : 0, 0 );
#ifdef ERR_CHK 
            fptype priceDelta = i->DGrefval - (*pi);
            if(fabs(priceDelta) >= 1e-4){
				printf("Error on %d:%d. Computed=%.5f, Ref=%.5f, Delta=%.5f\n",
                       tag, j, *pi, i->DGrefval, priceDelta );
            }
#endif
        }
    }
	c.prices.put( tag, prices );
    return CnC::CNC_Success;
}

int main (int argc, char **argv)
{

	if (argc < 3 || argc > 4)
	{
		printf("Usage:\n\t%s <number of options> <block size> [<nthreads>]\n", argv[0]);
		return 0;
	}
	int numOptions = atoi(argv[1]);
	if (numOptions <= 0){
		    fprintf(stderr, "Error specifying number of options\n");
			return -1;
	}
    int granularity = atoi(argv[2]);
	if (granularity <= 0){
		    fprintf(stderr, "Error specifying granularity\n");
			return -1;
	}
    int nThreads;
	if (argc > 3){
		nThreads = atoi(argv[3]);
	    if (nThreads <= 0){  // Looking for a positive number
		        fprintf(stderr, "Error specifying number of threads\n");
			    return -1;
	    }
    } else {
        nThreads = -1;  // Not specified
    }
	printf("Number of Options: %d\n", numOptions);
	printf("Number of Runs: %d\n", NUM_RUNS);
	printf("Granularity: %d\n", granularity);

    // Set the number of threads for the execution of the CnC graph
    if (nThreads > 0) {  // If specified by user
        //CnC::debug::set_num_threads(nThreads);
    }

    // Create an instance of the context class which defines the graph
    blackscholes_context c( numOptions / granularity + 1 );

    // How many elements do we have in our hard-coded datafile?
	int initOptionNum =  ( (sizeof(data_init)) / sizeof(OptionData) );
    int tag = 0;
    option_vector_type opt_vec = std::make_shared< std::vector< OptionData > >( granularity );
    // Repeat that hardcoded file several times:
	for( int i = 0; i < numOptions; ++i ) {
        int o = i % granularity;
        (*opt_vec)[o] = data_init[i % initOptionNum];
        if( o == granularity - 1 ) {
            c.opt_data.put( tag, opt_vec );
            c.tags.put( tag );
            ++tag;
            opt_vec = std::make_shared< std::vector< OptionData > >( granularity );
        }
	}

    // Wait for all steps to finish
    c.wait();

	return 0;
}
