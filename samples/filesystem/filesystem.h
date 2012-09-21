#ifndef filesystem_H_ALREADY_INCLUDED
#define filesystem_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <vector>
#include <memory>



// define the maximum length of randomly generated files, in blocks
#define MAX_FILE_LENGTH (1)
// define the number entries that can be stored in an inode 
#define DATA_BLOCKS_NO (9)
#define EMPTY (-1)

class triple;
class triple_epilog;
class triple_epilog2;
class inode {
public:
	int data_blocks[DATA_BLOCKS_NO];
	int indirect_block;
	int length;
	inode() {
		//printf("Created inode\n");
		for(int i=0; i<DATA_BLOCKS_NO; i++) data_blocks[i] = EMPTY;
		indirect_block = EMPTY;
		length = 0;
	}

	inode(int len) {
		//printf("Created inode\n");
		for(int i=0; i<DATA_BLOCKS_NO; i++) data_blocks[i] = EMPTY;
		indirect_block = EMPTY;
		length = len;
	}
	
	inode( const inode & i) {
		//printf("Created inode\n");
		memcpy(this->data_blocks, i.data_blocks, DATA_BLOCKS_NO*sizeof(int));
		this->indirect_block = i.indirect_block;
		this->length = i.length;
	}
};

class block {
	
public:
	static int BLOCK_SIZE;
	void* data;

	block() {
		//printf("Allocated block\n");
		data = malloc(BLOCK_SIZE*sizeof(int));
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

struct Concatenate_epilog
{
    int execute( const triple_epilog &, filesystem_context & ) const;
    aligned_t** get_dependences(const triple_epilog & t, filesystem_context & c, int & no ) const;
};

struct Concatenate_epilog2
{
    int execute( const triple_epilog2 &, filesystem_context & ) const;
    aligned_t** get_dependences(const triple_epilog2 & t, filesystem_context & c, int & no ) const;
};


// The context class
struct filesystem_context : public CnC::context< filesystem_context >
{
    // Step collections
    CnC::step_collection< Concatenate > concatenate; 
	CnC::step_collection< Concatenate_epilog > concatenate_epilog; 
    CnC::step_collection< Concatenate_epilog2 > concatenate_epilog2; 
    
    // Item collections
    CnC::item_collection< int, inode* > inodes; 	
    CnC::item_collection< int, block* > blocks; 	

    // Tag collections
    CnC::tag_collection< triple > jobIds;	
	CnC::tag_collection< triple_epilog > jobIds_epilog;		
	CnC::tag_collection< triple_epilog2 > jobIds_epilog2;			
	
	
    int BLOCK_SIZE; 
    // The context class constructor
    filesystem_context( int block_size = 256)
        : CnC::context< filesystem_context >(),
		  // Initialize each step collection
          concatenate( *this ),
		  concatenate_epilog( *this ),
		  concatenate_epilog2( *this ),
          // Initialize each item collection
          inodes( *this ),
          blocks( *this ),
          // Initialize each tag collection
          jobIds( *this ),
		  jobIds_epilog( *this ),
		  jobIds_epilog2( *this ),
          // Member initialization
          BLOCK_SIZE(block_size)
    {
        // Prescriptive relations
        jobIds.prescribes( concatenate, *this );
		jobIds_epilog.prescribes( concatenate_epilog, *this );
		jobIds_epilog2.prescribes( concatenate_epilog2, *this );
    }

    block* temp;
};

#endif // filesystem_H_ALREADY_INCLUDED
#include "filesystem_types.h"
