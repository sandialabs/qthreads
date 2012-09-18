#ifndef fib_H_ALREADY_INCLUDED
#define fib_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <iostream>
#include <qthread/qtimer.h>
//#include <cnc/debug.h>

using namespace std;

// Forward declaration of the context class (also known as graph)
struct porting_context;

// The step classes

struct porting_step
{
    int execute( const int & t, porting_context & c ) const;
    aligned_t** get_dependences(const int & t, porting_context & c, int & no ) const;
};

// The context class
struct porting_context : public CnC::context< porting_context >
{
    // step collections
    CnC::step_collection< porting_step > m_steps;
    // Item collections
    CnC::item_collection< int, int > m_porting_ids;
    // Tag collections
    CnC::tag_collection< int > m_tags;

    // The context class constructor
    porting_context()
        : CnC::context< porting_context >(),
          // Initialize each step collection
          m_steps( *this ),
          // Initialize each item collection
          m_porting_ids( *this ),
          // Initialize each tag collection
          m_tags( *this )
    {
        // Prescriptive relations
        m_tags.prescribes( m_steps, *this );

    }
};

#endif // fib_H_ALREADY_INCLUDED
