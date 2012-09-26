#include "filesystem.h"
#include <math.h>
#include <limits.h>
#include <qthread/qtimer.h>
#include <unistd.h>

block b;
#define usecs (4000000)

int Concatenate::execute( const triple &t, filesystem_context & c) const
{
	
	int sourceInodeId1 = t[0];
	int sourceInodeId2 = t[1];
	int destinationInodeId = t[2];
	//printf("Concatenate job %d-%d-%d starting...\n", sourceInodeId1, sourceInodeId2, destinationInodeId);
	inode* s1;
	inode* s2;
	c.inodes.get(sourceInodeId1, s1);
	//printf("Concatenate job read: %d\n", sourceInodeId1);
	
	c.inodes.get(sourceInodeId2, s2);
	//printf("Concatenate job read: %d\n", sourceInodeId2);
	inode* d = new inode(s1->length+s2->length);
	//printf("new block\n");
	block* dindirect = new block();
	if (s1->length+s2->length>9)
		d->indirect_block =  -2-destinationInodeId; // negative id means it is a indirect block
	else
		free(dindirect);
		
	int* data = (int*) dindirect->data;

	int newNodeId = 1;
	block* b = c.temp;
	int startover = 1;
	int used_blocks = 0; // current position in inode data_blocks structure
	for(int i=0; i<d->length; i++) {
		if (i<DATA_BLOCKS_NO) {
			d->data_blocks[i] = (1+destinationInodeId)*1000000+newNodeId;
			if (s1->data_blocks[i] != EMPTY) {
				c.blocks.get(s1->data_blocks[i], b);
				//sleep here
				my_usleep(usecs);
				
				used_blocks = i+1;
			}
		}
		else {
			data[i-DATA_BLOCKS_NO] = (1+destinationInodeId)*1000000+newNodeId;

		}
		c.blocks.put( (1+destinationInodeId)*1000000+newNodeId, b);
		newNodeId++;
	}
	
	c.inodes.put(destinationInodeId, d);
	
	
	
	// concatenate block numbers from the inode of s1
	
	for(int i=0; i<DATA_BLOCKS_NO; i++)
		if (s1->data_blocks[i] != EMPTY) {
			c.blocks.get(s1->data_blocks[i], b);
			//sleep here
			my_usleep(usecs);
			c.blocks.put( (1+destinationInodeId)*1000000+startover++, b);
			used_blocks = i+1;
		}
	// concatenate the block numbers in the indirect block of s1
	int crtPos = 0; // current position in indirect block of the destination
	if (s1->indirect_block != EMPTY) {
		block* s1indirect;
		c.blocks.get(s1->indirect_block, s1indirect);
		
		//memcpy(dindirect->data, s1indirect->data, c.BLOCK_SIZE);

		
		while (crtPos<c.BLOCK_SIZE/(int)sizeof(int) && (((int*)(s1indirect->data))[crtPos] != EMPTY)) {
			//printf("aaa1 crtPos=%d el=%d\n", crtPos, ((int*)(s1indirect->data))[crtPos]);
			c.blocks.get( ((int*)(s1indirect->data))[crtPos], b);
			//printf("aaa2\n");
			
			data[crtPos] = (1+destinationInodeId)*1000000+startover;
			//sleep here
			my_usleep(usecs);
			c.blocks.put((1+destinationInodeId)*1000000+startover++, b);
			crtPos++;
		}
	}

	
	//printf("Used blocks %d\n", used_blocks);
	// concatenate the block numbers in the inode structure of s2
	for(int i=0; i<DATA_BLOCKS_NO; i++)
		if (s2->data_blocks[i] != EMPTY) {
			if (used_blocks>=DATA_BLOCKS_NO)
			{
				//printf("bbb0\n");
				c.blocks.get(s2->data_blocks[i], b);
				//printf("bbb1\n");
				data[crtPos++] = (1+destinationInodeId)*1000000+startover;
				//sleep here
				my_usleep(usecs);
				c.blocks.put((1+destinationInodeId)*1000000+startover++, b);
			}
			else
			{
				d->data_blocks[used_blocks++] = (1+destinationInodeId)*1000000+startover;
				//printf("eee0\n");
				c.blocks.get(s2->data_blocks[i], b);
				//printf("eee1\n");
				//sleep here
				my_usleep(usecs);
				c.blocks.put((1+destinationInodeId)*1000000+startover++, b);
			}
		}

	//printf("Used blocks after second concat %d\n", used_blocks);
	// concatenate the block numbers in the indirect block of s2
	if (s2->indirect_block != EMPTY) {
		block* s2indirect;
		c.blocks.get(s2->indirect_block, s2indirect);
		int crtS2Pos = 0;
		int* dataS2 = (int*) s2indirect->data;
		while (crtS2Pos<c.BLOCK_SIZE/(int)sizeof(int) && (dataS2[crtS2Pos] != EMPTY) && crtPos<c.BLOCK_SIZE/(int)sizeof(int)) {
			data[crtPos++] = (1+destinationInodeId)*1000000+startover;
			//printf("ccc0\n");
			c.blocks.get(dataS2[crtS2Pos++], b);
			//printf("ccc1\n");
			//sleep here
			my_usleep(usecs);
			c.blocks.put((1+destinationInodeId)*1000000+startover++, b);
		}
	}

	
	if (crtPos>0)
	{
		c.blocks.put(d->indirect_block, dindirect);
	}
	
	
	//printf("Concatenate job %d-%d-%d finished...\n", sourceInodeId1, sourceInodeId2, destinationInodeId);
	
	return CnC::CNC_Success;
}

aligned_t** Concatenate::get_dependences(const triple & t, filesystem_context & c, int & no ) const
{
	no = 2;
	aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
	c.inodes.wait_on(t[0], &read[0]);
	c.inodes.wait_on(t[1], &read[1]);
	return read;
}


/* start: strict preconditions steps */
int Concatenate_epilog::execute( const triple_epilog &t, filesystem_context & c) const
{
	return CnC::CNC_Success;
}

aligned_t** Concatenate_epilog::get_dependences(const triple_epilog & t, filesystem_context & c, int & no ) const
{
	no = 0; 
	return NULL;
}


int Concatenate_epilog2::execute( const triple_epilog2 &t, filesystem_context & c) const
{
	return CnC::CNC_Success;
}


aligned_t** Concatenate_epilog2::get_dependences(const triple_epilog2 & t, filesystem_context & c, int & no ) const
{
	no = 0;
	return NULL;
}
/* end: strict preconditions steps */


int block::BLOCK_SIZE = 0;

int blockId;

/* chooses a random number of blocks
 * builds the file
 * fills in the file identified by fileId
 * and adds the inode and blocks to the item collections
 */ 
void addRandomFile(filesystem_context* c, int fileId)
{
	
	int random = rand() % MAX_FILE_LENGTH;
	

	/*comment out this line for random file length*/
	random = MAX_FILE_LENGTH;
	inode* file = new inode(random);
	int no_direct_blocks = random;
	file->indirect_block = EMPTY; 
	if (random>DATA_BLOCKS_NO)  no_direct_blocks = DATA_BLOCKS_NO;
	for(int i=0; i<no_direct_blocks; i++)
	{
		file->data_blocks[i] = blockId;
		block* bl = &b;
		c->blocks.put(blockId++, bl);
	}
	if (random>DATA_BLOCKS_NO)
	{
		block* bl = new block();
		int* data = (int*) (bl->data);
		int crt = 0;
		for(int i = no_direct_blocks; i<random; i++)
		{
			data[crt++] = blockId;
			block* bl = new block();
			c->blocks.put(blockId++, bl);
		}
	}
	c->inodes.put(fileId, file);
}

void printFile(filesystem_context* c, int fileId)
{
	inode* file;
	c->inodes.get(fileId, file);
	printf("\nFile name: %d (length=%d)\n", fileId, file->length);
	printf("   Direct blocks: ");
	int i = 0;
	while( (i<DATA_BLOCKS_NO) && (file->data_blocks[i] != EMPTY) )
	{
		//printf("(i=%d)\n", i);
		printf(" %d", file->data_blocks[i]);
		block* bl;
		c->blocks.get(file->data_blocks[i], bl);
		i++;
	}
	printf("   Indirect block content: ");
	if (file->indirect_block != EMPTY)
	{
		block* ib;
		c->blocks.get(file->indirect_block, ib);
		int* data = (int*)ib->data;
		int i = 0;
		while(data[i] != EMPTY) {
			printf("%d ", data[i]);
			i++;
		}
		
	}
}

int main (int argc, char **argv)
{

	if (argc != 2)
	{
		printf("Usage:\n\t%s <number of concatenation jobs> \n", argv[0]);
		return 0;
	}
	int jobs = atoi(argv[1]);
	
	if (jobs < 0){
		    fprintf(stderr, "Number of concatenation jobs cannot be negative.\n");
			return -1;
	}
	

	block::BLOCK_SIZE = MAX_FILE_LENGTH*jobs*4;
    // Create an instance of the context class which defines the graph
    filesystem_context c( block::BLOCK_SIZE );
	c.temp = new block;
	srand(time(NULL));


    qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);



	for(int i=jobs; i<jobs + jobs ; i++)
		addRandomFile(&c, i);
	printf("ADDED FILES.\n");
	for( int i = 1; i < jobs; i++ ) {
		c.jobIds.put(triple(i*2, i*2+1, i));   
	}
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
