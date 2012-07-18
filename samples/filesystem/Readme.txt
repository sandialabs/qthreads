This benchmark simulates concatenation operations in a file system based on inodes and plain data blocks.

The files are represented by inode structures (one inode per file ) that contain an array of ints
that are indexes for data bloxks. If files are larger and do nt fit in the number of data blocks
that can be indexed by the array, an additional block is used (inode.indirect_block) that
contains indices of the following data blocks.

Limitations:
- Files are a fix number of blocks (no partial blocks allowed). 
- No verification of correctness is performed.
