#include <cassert>
#include <cstdio>
#include <cmath>
#include <cnc/cnc.h>
#include <iostream>
#include <cassert>
#include <memory>

#include <qthread/qtimer.h>
#define usecs (4000)

int TILE_SIZE = 100;

int t;

int my_usleep(int i) {
	
	for(int i=0; i<2000000; i++)
		t += 1.12*t;
	//printf("t%d", t);
	return t;
}

typedef std::pair<int,int> pair;
template <>
class cnc_tag_hash_compare< pair >
{
public:
    size_t hash( const pair & t ) const
    {
        return t.first + t.second;
    }
    bool equal( const pair & a, const pair & b) const { 
	return a.first == b.first && a.second == b.second; 
    }
};



class Tile
{
public:
	double* m_array;
	Tile()/*, m_full( true )*/
    {
		
    		m_array = new double[TILE_SIZE*TILE_SIZE];
    		for(int i=0; i<TILE_SIZE*TILE_SIZE; i++) {
    			m_array[i] = 0.0;
    		}
    }
    
    Tile(int f, int s)/*, m_full( true )*/
    {
		
    		m_array = new double[TILE_SIZE*TILE_SIZE];
    		//printf("Allocating %d-%d at %x (in struct %x)\n", f, s, m_array, this);
    		for(int i=0; i<TILE_SIZE*TILE_SIZE; i++) {
    			m_array[i] = 0.0;
    		}
    }
    
    
    ~Tile() {
    	//printf("Deallocationg %x from struct %x\n", m_array, this);
		delete m_array;
    }
};

// Forward declaration of the context class (also known as graph)
struct reduction_context;

// The step classes
struct Update
{
    int execute( const pair &, reduction_context & ) const;
    aligned_t** get_dependences(const pair & t, reduction_context & c, int & no ) const;
};

struct Reduce
{
    int execute( const int &, reduction_context & ) const;
    aligned_t** get_dependences(const int & t, reduction_context & c, int & no ) const;
};

struct NextIter
{
    int execute( const int &, reduction_context & ) const;
    aligned_t** get_dependences(const int & t, reduction_context & c, int & no ) const;
};



// The context class
struct reduction_context : public CnC::context< reduction_context >
{
    // Step collections
    CnC::step_collection< Update > update;
	CnC::step_collection< Reduce > reduce;
	CnC::step_collection< NextIter > next;
    // Item collections
    CnC::item_collection< pair, Tile > tile_data;
    CnC::item_collection< int, int >  reduced_data;

    // Tag collections
    CnC::tag_collection< int > reduce_tags;
    CnC::tag_collection< pair > update_tags;
	CnC::tag_collection< int > next_iter_tags;
    int NO_TILES;
    int TILE_SIZE;
    int NO_ITER;
    // The context class constructor
    reduction_context( int tiles = 2, int tile_size = 2, int no_iter = 20)
        : CnC::context< reduction_context >(),
	  // Initialize each step collection
          update( *this ),
          reduce( *this ),
          next( *this ),
          // Initialize each item collection
          tile_data( *this ),
          reduced_data( *this ),
          // Initialize each tag collection
          reduce_tags( *this ),
          update_tags( *this ),
          next_iter_tags( *this ),
          NO_TILES(tiles),
          TILE_SIZE(tile_size),
          NO_ITER(no_iter)
    {
        // Prescriptive relations
    	update_tags.prescribes( update, *this );
    	reduce_tags.prescribes( reduce, *this );
    	next_iter_tags.prescribes( next, *this );
    }
};
