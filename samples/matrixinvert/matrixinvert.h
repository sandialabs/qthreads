#ifndef MatrixInvert_H_ALREADY_INCLUDED
#define MatrixInvert_H_ALREADY_INCLUDED


//#define DISABLE_GET_COUNTS

#include <cnc/cnc.h>
#include <qthread/qtimer.h>

// Forward declaration of the context class (also known as graph)
struct MatrixInvert_context;

// The step classes

struct compute_inverse
{
    int execute( const tile_tag & t, MatrixInvert_context & c ) const;
    aligned_t** get_dependences(const tile_tag & t, MatrixInvert_context & c, int & no ) const;
};

// The context class
struct MatrixInvert_context : public CnC::context< MatrixInvert_context >
{
    // Step collections
    CnC:: step_collection< compute_inverse > sc_matrixinvert_compute;
    
    // Item collections
    CnC::item_collection< tile_tag, tile > m_tiles;

    // Tag collections
    CnC::tag_collection< tile_tag > m_steps;

    // The context class constructor
    MatrixInvert_context()
        : CnC::context< MatrixInvert_context >(),
          //Initialize each step collection
          sc_matrixinvert_compute( *this ),
          // Initialize each item collection
          m_tiles( *this ),
          // Initialize each tag collection
          m_steps( *this )
    	  {
        	// Prescriptive relations
        	m_steps.prescribes( sc_matrixinvert_compute, *this );
    	  }
};

#endif // MatrixInvert_H_ALREADY_INCLUDED
