#ifndef filesystem_H_ALREADY_INCLUDED
#define filesystem_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <vector>
#include <memory>

#include "filesystem_types.h"

// define the maximum length of randomly generated files, in blocks
#define MAX_FILE_LENGTH (1)
// define the number entries that can be stored in an inode 
#define DATA_BLOCKS_NO (9)
#define EMPTY (-1)

class inode {
public:
	int data_blocks[DATA_BLOCKS_NO];
	int indirect_block;
	
	inode() {
		for(int i=0; i<DATA_BLOCKS_NO; i++) data_blocks[i] = EMPTY;
		indirect_block = EMPTY;
	}
	
	inode( const inode & i) {
		memcpy(this->data_blocks, i.data_blocks, DATA_BLOCKS_NO*sizeof(int));
		this->indirect_block = i.indirect_block;
	}
};

class block {
	
public:
	static int BLOCK_SIZE;
	void* data;

	block() {
		data = malloc(BLOCK_SIZE);
		for(int i=0; i< (BLOCK_SIZE/(int)sizeof(int)); i++)
			((int*)data)[i] = EMPTY;
	}
};



// Forward declaration of the context class (also known as graph)
struct filesystem_context;

// The step classes
struct Concatenate
{
    int execute( const triple &, filesystem_context & ) const;
    aligned_t** get_dependences(const triple & t, filesystem_context & c, int & no ) const;
};


// The context class
struct filesystem_context : public CnC::context< filesystem_context >
{
    // Step collections
    CnC::step_collection< Concatenate > concatenate; 
    
    // Item collections
    CnC::item_collection< int, inode* > inodes; 	
    CnC::item_collection< int, block* > blocks; 	

    // Tag collections
    CnC::tag_collection< triple > jobIds;		
	
	
    int BLOCK_SIZE; 
    // The context class constructor
    filesystem_context( int block_size = 256)
        : CnC::context< filesystem_context >(),
		  // Initialize each step collection
          concatenate( *this ),
          // Initialize each item collection
          inodes( *this ),
          blocks( *this ),
          // Initialize each tag collection
          jobIds( *this ),
          // Member initialization
          BLOCK_SIZE(block_size)
    {
        // Prescriptive relations
        jobIds.prescribes( concatenate, *this );
    }
};

#endif // filesystem_H_ALREADY_INCLUDED
