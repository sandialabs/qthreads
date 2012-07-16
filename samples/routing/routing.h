#ifndef routing_H_ALREADY_INCLUDED
#define routing_H_ALREADY_INCLUDED

#define CNC_PRECOND
//#define CNC_PRECOND_ONLY

#include <cnc/cnc.h>
#include <vector>
#include <memory>

#include "routing_types.h"

// Forward declaration of the context class (also known as graph)
struct routing_context;

// The step classes
struct NetworkUpdate
{
    int execute( const int &, routing_context & ) const;
    aligned_t** get_dependences(const int & t, routing_context & c, int & no ) const;
};

struct TableUpdate
{
    int execute( const triple &, routing_context & ) const;
    aligned_t** get_dependences(const triple & t, routing_context & c, int & no ) const;
};



// The context class
struct routing_context : public CnC::context< routing_context >
{
    // Step collections
    CnC::step_collection< NetworkUpdate > networkUpdate; // iteration = new set of table updates after a topology change
    CnC::step_collection< TableUpdate > tableUpdate; // tableUpdate = another step towards convergence, updates tables

    // Item collections
    CnC::item_collection< int, int* > topology; 	// tag: iterationId; item value: adiacency matrix for the network at iteration iterationId
    CnC::item_collection< triple, int* >  tables; 	// tag: (iteration, subiteration, node);
													// item value: array with v[nodeId] = number of hops required to reach nodeId

    // Tag collections
    CnC::tag_collection< int > nextIterationId;		// iteration = new run of convergence algorithm (set of table updates) after a topology change
	CnC::tag_collection< triple > nodeUpdateId;		// triple (iteration, subiteration, nodeId)

	
    int N; 		// number of nodes in the network 
    int IT;		// number of iteration simulated
    // The context class constructor
    routing_context( int nodes = 0, int iterations = 1)
        : CnC::context< routing_context >(),
		  // Initialize each step collection
          networkUpdate( *this ),
          tableUpdate( *this ),
          // Initialize each item collection
          topology( *this ),
          tables( *this ),
          // Initialize each tag collection
          nextIterationId( *this ),
          nodeUpdateId( *this ),
          // Member initialization
          N(nodes),
          IT(iterations) 
    {
		
        // Prescriptive relations
        nextIterationId.prescribes( networkUpdate, *this );
        nodeUpdateId.prescribes( tableUpdate, *this );
    }
};

#endif // routing_H_ALREADY_INCLUDED
