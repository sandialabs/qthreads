#include "routing.h"
#include <math.h>
#include <limits.h>
int topo;

int NetworkUpdate::execute( const int &it, routing_context & c) const
{
	//printf("NetworkUpdate iteration %d starting.\n", it);
	int* m;
	//topo ++;
	c.topology.get(it, m);
	int* nextM = (int*) malloc(c.N*c.N*sizeof(int));
	memcpy(nextM, m, c.N*c.N*sizeof(int));

	// TODO: Randomize changes to the topology matrix here

	c.topology.put(it+1, nextM, c.N*(c.N) + 1  );

	if (it<c.IT-1)
		c.nextIterationId.put(it+1);
	return CnC::CNC_Success;
}

aligned_t** NetworkUpdate::get_dependences(const int & it, routing_context & c, int & no ) const
{
	return NULL;
}


int TableUpdate::execute( const triple &t, routing_context & c) const
{
	//printf("TableUpdate iteration %d-%d-%d starting.\n", t[0], t[1], t[2]);
	int me = t[2];
	int subiteration = t[1];
	int iteration = t[0];
	
	int* topology;
	//printf("Getting topology %d\n", iteration);
	c.topology.get(iteration, topology);
	int* neighbors = &(topology[me*c.N]);
	topo ++;
	
	int* myRoutingTable;
	if (subiteration == 0) {
		// if it is the first subiteration, build my initial table myself
		int* myInitialTable =  (int*) malloc(c.N*sizeof(int));
		for(int i=0; i<c.N; i++)
			if (neighbors[i] == 1) myInitialTable[i] = 1;
			else myInitialTable[i] = INT_MAX;
		myRoutingTable = myInitialTable;
		c.tables.put(t, myInitialTable);
	} else {
		// get my last built table from the item collection
		//printf("Will-get my own tag... ( %d-%d-%d )\n", t[0], t[1], t[2]);
		c.tables.get( t, myRoutingTable);
		//printf("Got my own tag... ( %d-%d-%d )\n", t[0], t[1], t[2]);
	}

	int* newRountingTable = (int*) malloc(c.N*sizeof(int));
	memcpy(newRountingTable, myRoutingTable, c.N*sizeof(int));
	myRoutingTable = newRountingTable;
	
	// for every neighbour
	// get its table and update my table
	// with the shortest path
	int nCount = 0;
	for(int i=0; i<c.N; i++) {
		if ((neighbors[i]==1)&&(me!=i)) {
			nCount ++;
			int* nTable;
			//printf("Will-get tag... ( %d-%d-%d )\n", iteration, subiteration, i);
			c.tables.get(triple(iteration, subiteration, i), nTable);
			for(int nTableEntry =0; nTableEntry< c.N; nTableEntry++) {
				if (myRoutingTable[nTableEntry]>nTable[nTableEntry]+1)
					myRoutingTable[nTableEntry] = nTable[nTableEntry]+1;
			}
		}
	}	

	c.tables.put(triple(iteration, subiteration+1, me), myRoutingTable, nCount + 1);
	//c.tables.put(triple(iteration, subiteration+1, me), myRoutingTable);
	if (subiteration<c.N-1)
		c.nodeUpdateId.put(triple(iteration, subiteration+1, me));
	else if (iteration<c.IT-1)
		c.nodeUpdateId.put(triple(iteration+1, 0, me));
		
	//printf("TableUpdate iteration %d-%d-%d end.\n", t[0], t[1], t[2]);
	return CnC::CNC_Success;
}

aligned_t** TableUpdate::get_dependences(const triple & t, routing_context & c, int & no ) const
{
	no = 1;
	aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
	c.topology.wait_on(t[0], &read[0]);
	return read;

}


int* getInitialTopology(int nodes)
{
	int* v = (int*) malloc(nodes*nodes*sizeof(int));

	for(int i=0; i<nodes; i++)
		for(int j=0; j<=i; j++)
			if (i!=j) {
				v[i*nodes+j] = 1;
				v[j*nodes+i] = 1;
			} else {
				v[i*nodes+j] = 0;
			}
	
	return v;
}

int main (int argc, char **argv)
{

	if (argc != 3)
	{
		printf("Usage:\n\t%s <number of nodes> <number of iterations> \n", argv[0]);
		return 0;
	}
	int nodes = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	
	if (nodes < 1){
		    fprintf(stderr, "Number of nodes in network cannot be negative.\n");
			return -1;
	}
	
	if (iterations <= 0){
		    fprintf(stderr, "Number of iterations imulated cannot be less than or equal to 0.\n");
			return -1;
	}
    

    // Create an instance of the context class which defines the graph
    routing_context c( nodes, iterations );

	int i = 0;
	int* topology = getInitialTopology(nodes);
	c.topology.put(i, topology);
    c.nextIterationId.put(i);
	for( int i = 0; i < nodes; ++i ) {
		c.nodeUpdateId.put(triple(0, 0, i));   
	}

    // Wait for all steps to finish
    c.wait();
	printf("Topo=%d\n", topo);
	
	return 0;
}
