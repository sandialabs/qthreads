#include "reduction.h"


int Update::execute(const pair & t, reduction_context & c ) const
{
	Tile* tile = new Tile();
    //printf("Starting update %d-%d...\n", t.first, t.second);
    printf("Reading %d-%d\n", t.first, t.second);
    c.tile_data.get(t, *tile);
    printf("Producing tile %d of iteration %d\n", t.first, t.second+1);
    my_usleep(1);
    c.tile_data.put(*new pair(t.first, t.second+1), *new Tile(t.first, t.second+1), 2);
    return CnC::CNC_Success;
}

aligned_t** Update::get_dependences(const pair & t, reduction_context & c, int & no ) const
{
	no = 1;
	aligned_t** read = (aligned_t**)malloc(no*sizeof(int)); 
	printf("Waiting on %d-%d\n", t.first, t.second);
    c.tile_data.wait_on(t, &(read[0]));
    return (aligned_t**)read;
}

int Reduce::execute(const int & t, reduction_context & c ) const
{
    int max = c.NO_TILES/2;
    #ifdef CNC_PRECOND_ONLY
    	max = c.NO_TILES;
    #endif
    
    for(int k = 0; k < max; k++) {
    	Tile *crt = new Tile();
    	
        c.tile_data.get(pair(k, t), *crt);
        my_usleep(1);
    }
    // DO NOT add getcount for this put
	// because then strict would show memory leaks
    c.reduced_data.put(t, 1);
    return CnC::CNC_Success;
}

aligned_t** Reduce::get_dependences(const int & t, reduction_context & c, int & no ) const
{
	no = c.NO_TILES;
	aligned_t** read; 
	#ifdef CNC_PRECOND_ONLY
		// strict runtime needs to wait on everything	
		read = (aligned_t**)malloc(no*sizeof(int));
	
		for(int k = 0; k < c.NO_TILES; k++) {
    		Tile crt;
        	c.tile_data.wait_on(pair(k, t), &(read[k]));
    	}
    #else
    	// flexible runtime can pick and choose
    	no = 1;
    	read = (aligned_t**)malloc(no*sizeof(int));
    	c.tile_data.wait_on(pair(0, t), &(read[0]));
    #endif
    
    return read;
}

int NextIter::execute(const int & t, reduction_context & c ) const
{
	//printf("NexIIter %d runinngin\n", t);
	int reduced_value;
    if (t>0) 
    {	
    	//printf("NextIter waiting for reduced data %d\n", t-1);
    	c.reduced_data.get(t-1, reduced_value);
    	
    }
    	
    if (t<c.NO_ITER) {
    	c.reduce_tags.put(t);
    	c.next_iter_tags.put(t+1);
    	//printf("Prescribed next iteration start (%d)...\n", t+1);
    	
    	
    	for(int i=0; i<c.NO_TILES; i++)
			c.update_tags.put(pair(i, t));
    }
    
    return CnC::CNC_Success;
}

aligned_t** NextIter::get_dependences(const int & t, reduction_context & c, int & no ) const
{
	no = 0;
    if (t>0) 
    {	
    	no = 1;
    	int* read = (int*)malloc(1 * sizeof(int));
    	c.reduced_data.wait_on(t-1, (aligned_t**)&(read[0]));
    	return (aligned_t**)read;	
    }
    return NULL;
}


int main (int argc, char **argv)
{

	if (argc != 4)
	{
		printf("Usage:\n\t%s <number of tiles> <tile size> <number of iterations\n", argv[0]);
		return 0;
	}
	int tiles = atoi(argv[1]);
	int tile_size = atoi(argv[2]);
	int no_iter = atoi(argv[3]);
	if (tiles < 2){
		    fprintf(stderr, "Number of tiles cannot be less than 2.\n");
			return -1;
	}
	TILE_SIZE = tile_size;
    // Create an instance of the context class which defines the graph
    reduction_context c( tiles, tile_size , no_iter);
	
	
	
	srand(time(NULL));
	qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);

	for( int i = 0; i < tiles; i++ ) {
		c.tile_data.put(*new pair(i, 0), * new Tile(), 2);   
	}
	c.next_iter_tags.put(0);
    // Wait for all steps to finish
    c.wait();

	qtimer_stop(timer);
    total_time = qtimer_secs(timer);
    printf("Time(s): %.3f\n", total_time);
    qtimer_destroy(timer);

    
	/*
	for( int i = 1; i < jobs + jobs; i++ )
		printFile(&c, i);
	*/
	printf("Jobs finished.\n");
	//printFile(&c, 1);
	return 0;
}
