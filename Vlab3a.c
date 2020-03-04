#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>
#include "ext2_fs.h"

struct ext2_super_block super;
struct ext2_group_desc gd;
int fd;
int blockSize;
int numGroups;
long bytesPerGroup;


void superSummary(){
	pread(fd, &super, sizeof(super), blockSize);

	fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", 
		super.s_blocks_count,super.s_inodes_count, blockSize,
		 super.s_inode_size, super.s_blocks_per_group, 
		 super.s_inodes_per_group, super.s_first_ino);
}

void groupSummary(int gIndex){
	int gMult;
	if(gIndex == 0)
		gMult = 2;
	else
		gMult = 1;
	int blocksPerGroup = super.s_blocks_per_group;
	int inodesPerGroup = super.s_inodes_per_group;
	if(gIndex == numGroups - 1){
		if(super.s_blocks_count != super.s_blocks_per_group)
			blocksPerGroup = super.s_blocks_count % super.s_blocks_per_group;
		if(super.s_inodes_count != super.s_inodes_per_group)
			inodesPerGroup = super.s_inodes_count % super.s_inodes_per_group;
	}

	pread(fd, &gd, sizeof(gd), blockSize*gMult + bytesPerGroup*gIndex);
	fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
		gIndex, blocksPerGroup, inodesPerGroup, gd.bg_free_blocks_count,
		gd.bg_free_inodes_count, gd.bg_block_bitmap, gd.bg_inode_bitmap,
		gd.bg_inode_table);
}

void freeBlockSummary(int blockIndex){
	char freeBlock[blockSize];
	pread(fd, &freeBlock, blockSize, blockSize*blockIndex);
}

int main(int argc, char* argv[]){

	blockSize = EXT2_MIN_BLOCK_SIZE << super.s_log_block_size;

	if(argc != 2){
		fprintf(stderr, "Must have 1 argument img\n");
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	superSummary();
	bytesPerGroup = super.s_blocks_count*super.s_blocks_per_group;
	numGroups = (int)ceil((double)super.s_blocks_count/(double)super.s_blocks_per_group);
	
	for(int i; i < numGroups; i++){
		groupSummary(i);
		freeBlockSummary(gd.bg_block_bitmap);
	}
}