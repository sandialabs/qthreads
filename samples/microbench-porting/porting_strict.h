#ifndef fib_H_ALREADY_INCLUDED
#define fib_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <iostream>
#include <qthread/qtimer.h>
//#include <cnc/debug.h>

using namespace std;


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



// Forward declaration of the context class (also known as graph)
struct porting_context;

// The step classes

struct porting_step
{
    int execute( const triple & t, porting_context & c ) const;
    aligned_t** get_dependences(const triple & t, porting_context & c, int & no ) const;
};

// The context class
struct porting_context : public CnC::context< porting_context >
{
    // step collections
    CnC::step_collection< porting_step > m_steps;
    // Item collections
    CnC::item_collection< int, int > m_porting_ids;
    // Tag collections
    CnC::tag_collection< triple > m_tags;

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
