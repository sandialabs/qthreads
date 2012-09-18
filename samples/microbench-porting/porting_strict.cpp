#include <stdio.h>
#include <cstdlib>
#include "porting_strict.h"
// chain_length>no_chains
int no_chains;
int no_dependences;
int chain_length;
    
int UNUSED = -9999999;
  
aligned_t** porting_step::get_dependences(const triple & tt, porting_context & c, int & no ) const
{
	no = 0;
	aligned_t** read;
	read = NULL;
	

	if (tt[1] != UNUSED)
	{
		no = 1;
		read = (aligned_t**) malloc(sizeof(int));
		c.m_porting_ids.wait_on( tt[1], &read[0] );
		//printf("Task will not start until %d is found.\n", tt[1]);
	}	
	
	return read;
}
int porting_step::execute( const triple & tag, porting_context & ctxt ) const
{
	
		
		int crt_chain = (int)(((double)tag[0])/(double)chain_length);
		//printf("%d/%d=%d\n", tag[0], chain_length, crt_chain);
		int crt_id = tag[0]%chain_length;
		//printf("Task (%d) crt_chain=%d crt_id=%d is starting (extra waits %d).\n", tag[0], crt_chain, crt_id , tag[2]);
		int t = tag[0]*no_dependences;
		if (crt_id+1 != chain_length) {
			// need to read input(s)
			
			int crt_dep = t;
			if (tag[1] != UNUSED)
				crt_dep = tag[1];
			//printf("Task %d waiting for dep %d\n", tag[0], crt_dep);
			ctxt.m_porting_ids.get( crt_dep, t);
			
			// tag[2] = number of remaining gets
			// tag[1] = value to wait for
			// tag[0] = id, as in the case of the flexible preconditions
			if (tag[2] != 0) {
				//printf("Spawning ported-subtask (%d-%d-%d)...\n", tag[0], t, tag[2]-1);
				ctxt.m_tags.put(*new triple(tag[0], t, tag[2]-1));
			}
		} else {
			//printf("Task (%d) does not need input.\n", tag);
		}
		

		
		// if I am the last subtask, I will spawn the next task in the chain
		// unless I am the last subtask of the last taks in the chain
		if (crt_id>0) {
			if ((tag[2] == 0)||(crt_id+1 == chain_length)) {
				ctxt.m_tags.put(*new triple(tag[0]-1, UNUSED, no_dependences-1) );
			}
		}	
		
		// produce the next output, if I am the first task of the chain
		// or the last subtask of any other task in the chain
		//int t = 0;
		if ((tag[2] == 0)||(crt_id+1 == chain_length)) {
			for(int i=0; i<no_dependences; i++) 
			{
				int crt_dep = i+(tag[0]-1)*no_dependences;
				//printf("Task %d producing dep %d\n", tag[0], crt_dep);
				ctxt.m_porting_ids.put( crt_dep, crt_dep + 1);
			}	
		}
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
    for( int i = 0; i < no_chains; ++i ) {
    	//printf("Spawning initial task for chain %d with id %d\n", i, (i+1)*chain_length-1);
    	ctxt.m_tags.put( *new triple((i+1)*chain_length-1, UNUSED, no_dependences-1) );
	}
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
