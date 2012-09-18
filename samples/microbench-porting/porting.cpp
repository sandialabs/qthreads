#include <stdio.h>
#include <cstdlib>
#include "porting.h"
// chain_length>no_chains
int no_chains;
int no_dependences;
int chain_length;
    
aligned_t** porting_step::get_dependences(const int & tt, porting_context & c, int & no ) const
{
	no = 0;
}
int porting_step::execute( const int & tag, porting_context & ctxt ) const
{
	int crt_chain = (int)(tag/chain_length);
	int crt_id = tag%chain_length;
	//printf("Task (%d) crt_chain=%d crt_id=%d is starting.\n", tag, crt_chain, crt_id );
	if (crt_id+1 != chain_length) {
		// need to read input(s)
		int t = tag*no_dependences;
		for(int i=0; i<no_dependences; i++) {
			int crt_dep = t;
			//printf("Task %d waiting for dep %d\n", tag, crt_dep);
			// HERE IS WHY WE NEEED FRLEXIBLE PRECONDITIONS:
			// crt_dep is actually the value gotten in the previous iteration (t)
			ctxt.m_porting_ids.get( crt_dep, t);
		}
	} else {
		//printf("Task (%d) does not need input.\n", tag);
	}
	
	// test if I reached the end of the chain
	// or if I need to spawn the next link
	if (crt_id>0) {
		ctxt.m_tags.put(tag-1);
	}	
	
	// produce the next output
	//int t = 0;
	for(int i=0; i<no_dependences; i++) 
	{
		int crt_dep = i+(tag-1)*no_dependences;
		//printf("Task %d producing dep %d\n", tag, crt_dep);
		ctxt.m_porting_ids.put( crt_dep, crt_dep + 1);
	}	
	
	/*
    switch( tag ) {
        case 0 : ctxt.m_fibs.put( tag, 0 ); break;
        case 1 : ctxt.m_fibs.put( tag, 1 ); break;
        default : 
            // get previous 2 results
            fib_type f_1; ctxt.m_fibs.get( tag - 1, f_1 );
            fib_type f_2; ctxt.m_fibs.get( tag - 2, f_2 );
            // put our result
            ctxt.m_fibs.put( tag, f_1 + f_2 );
    }
    
    */
    return CnC::CNC_Success;
}

int main( int argc, char* argv[] )
{
    no_chains = 1;
    no_dependences = 2;
    chain_length = 100;
    // eval command line args
    if( argc < 3 ) {
        printf("usage: <number of chains> <number of dataflow dependences per taks> <chain length>\n");
    } else {
    	no_chains = atol( argv[1] );
    	printf("no_chains = %d\n", no_chains);
    	no_dependences = atoi( argv[2] );
    	printf("no_dependences = %d\n", no_dependences);
    	chain_length = atoi ( argv[3] );
    	printf("chain_length = %d\n", chain_length);
	}
    // create context
    porting_context ctxt;
	qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);
    // put tags to initiate evaluation
    for( int i = 0; i < no_chains; ++i ) ctxt.m_tags.put( (i+1)*chain_length-1 );

    // wait for completion
    ctxt.wait();
     
	qtimer_stop(timer);
    total_time = qtimer_secs(timer);
    printf("%.3f\n", total_time);
    qtimer_destroy(timer);
    
    // get result
    for( int i = 0; i < no_chains; ++i ) 
    {
		int res2;
		int el = (i)*chain_length*no_dependences-1;
		ctxt.m_porting_ids.get( el, res2 );
	
		// print result
		std::cout << "porting_ids (" << el << "): " << res2 << std::endl;
	}
	
    return 0;
}
