#include "filesystem.h"
#include <math.h>
#include <limits.h>
#include <qthread/qtimer.h>

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
	
	inode* d = new inode();
	block* dindirect = new block();
	int* data = (int*) dindirect->data;
	
	// concatenate block numbers from the inode of s1
	int used_blocks = 0; // current position in inode data_blocks structure
	for(int i=0; i<DATA_BLOCKS_NO; i++)
		if (s1->data_blocks[i] != EMPTY) {
			d->data_blocks[i] = s1->data_blocks[i];
			used_blocks = i+1;
		}
	
	// concatenate the block numbers in the indirect block of s1
	int crtPos = 0; // current position in indirect block of the destination
	if (s1->indirect_block != EMPTY) {
		block* s1indirect;
		c.blocks.get(s1->indirect_block, s1indirect);
		memcpy(dindirect->data, s1indirect->data, c.BLOCK_SIZE);
		
		while (crtPos<c.BLOCK_SIZE/(int)sizeof(int) && (data[crtPos] != EMPTY))
			crtPos++;
	} 
	//printf("Used blocks %d\n", used_blocks);
	// concatenate the block numbers in the inode structure of s2
	for(int i=0; i<DATA_BLOCKS_NO; i++)
		if (s2->data_blocks[i] != EMPTY) {
			if (used_blocks>=DATA_BLOCKS_NO)
			{
				data[crtPos++] = s2->data_blocks[i];
			}
			else
			{
				d->data_blocks[used_blocks++] = s2->data_blocks[i];
			}
		}
	//printf("Used blocks after second concat %d\n", used_blocks);
	// concatenate the block numbers in the indirect block of s2
	if (s2->indirect_block != EMPTY) {
		block* s2indirect;
		c.blocks.get(s2->indirect_block, s2indirect);
		int crtS2Pos = 0;
		int* dataS2 = (int*) s2indirect->data;
		while (crtS2Pos<c.BLOCK_SIZE/(int)sizeof(int) && (dataS2[crtS2Pos] != EMPTY) && crtPos<c.BLOCK_SIZE/(int)sizeof(int))
			data[crtPos++] = dataS2[crtS2Pos++];
	}

	
	if (crtPos>0)
	{
		d->indirect_block = -2-destinationInodeId; // negative id means it is a indirect block
													// the -2 
		c.blocks.put(d->indirect_block, dindirect);
	}
	
	c.inodes.put(destinationInodeId, d);
	//printf("Concatenate job %d-%d-%d finished...\n", sourceInodeId1, sourceInodeId2, destinationInodeId);
	
	return CnC::CNC_Success;
}

aligned_t** Concatenate::get_dependences(const triple & t, filesystem_context & c, int & no ) const
{
	no = 2;
	aligned_t** read = (aligned_t**) malloc(no * sizeof(aligned_t*));
	c.inodes.wait_on(t[0], &read[0]);
	c.inodes.wait_on(t[1], &read[0]);
	return read;
}

int block::BLOCK_SIZE = 0;

int blockId;

/* chooses a random number of blocks
 * builds the file
 * fills in the file identified by fileId
 * and adds the inode and blocks to the item collections
 */ 
void addRandomFile(filesystem_context* c, int fileId)
{
	inode* file = new inode();
	int random = rand() % MAX_FILE_LENGTH;


	/*comment out this line for random file length*/
	random = MAX_FILE_LENGTH;
	
	int no_direct_blocks = random;
	file->indirect_block = EMPTY; 
	if (random>DATA_BLOCKS_NO)  no_direct_blocks = DATA_BLOCKS_NO;
	for(int i=0; i<no_direct_blocks; i++)
	{
		file->data_blocks[i] = blockId;
		block* bl = new block();
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
	printf("\nFile name: %d\n", fileId);
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

	if (argc != 3)
	{
		printf("Usage:\n\t%s <number of concatenation jobs> <block size> \n", argv[0]);
		return 0;
	}
	int jobs = atoi(argv[1]);
	
	if (jobs < 0){
		    fprintf(stderr, "Number of concatenation jobs cannot be negative.\n");
			return -1;
	}
	
    int block_size = atoi(argv[2]);
    if (block_size <= 0){
		    fprintf(stderr, "Block size cannot be negative.\n");
			return -1;
	}

	block::BLOCK_SIZE = block_size;
    // Create an instance of the context class which defines the graph
    filesystem_context c( block_size );

	srand(time(NULL));


    qtimer_t timer;
    double   total_time = 0.0;
    timer = qtimer_create();
    qtimer_start(timer);


	for( int i = 1; i < jobs; i++ ) {
		c.jobIds.put(triple(i*2, i*2+1, i));   
	}
	for(int i=jobs; i<jobs + jobs ; i++)
		addRandomFile(&c, i);

    // Wait for all steps to finish
    c.wait();

	qtimer_stop(timer);
    total_time = qtimer_secs(timer);
    printf("Time(s): %.3f\n", total_time);
    qtimer_destroy(timer);

    /*
	printf("Jobs finished.\n");
	for( int i = 1; i < jobs + jobs; i++ )
		printFile(&c, i);
	*/
	return 0;
}
