#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include "ext2_fs.h"

struct ext2_super_block super;
struct ext2_group_desc gd;
int fd;
int blockSize;
int numGroups;
long bytesPerGroup;

char* timeGetter(time_t curr)
{
  char* buff = malloc(sizeof(char)*32);
  time_t rawtime = curr;
  struct tm* raw = gmtime(&rawtime);
  strftime(buff, 32, "%m/%d/%y %H:%M:%S", raw);
  return buff;
}

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

void bitMapSummary(int blockIndex){
	char bitMap[blockSize];
	pread(fd, &bitMap, blockSize, blockSize*blockIndex);

	for(int i = 0; i < blockSize; i++){
		for(int j = 0; j < 8; j++){
			if(((bitMap[i] >> j) & 1) == 0){
				fprintf(stdout, "BFREE,%d\n", i*8+j);
			}
		}
	}
}

void dirSummary(int inodeNum, struct ext2_inode *inode){

	struct ext2_dir_entry dir;
	int offset;
	int logicalOffset = 0;
	for(int i = 0; i < 12; i++){
		if(inode->i_block[i] != 0){
			while(logicalOffset < blockSize){
				offset = inode->i_block[i]*blockSize + logicalOffset;
				pread(fd, &dir, sizeof(dir), offset);
				if(dir.inode != 0){
					fprintf(stdout,"DIRENT,%d,%d,%d,%d,%d,'%s'\n",
					inodeNum, logicalOffset, dir.inode, dir.rec_len, 
					dir.name_len, dir.name);
				}
				logicalOffset += dir.rec_len;
			}
		}
	}
}

void InodeSummary(int blockNum)
{

  int InodeTableSize = super.s_inodes_per_group;
  int iNum = blockNum * InodeTableSize + 1;
  struct ext2_inode inodeTable[super.s_inodes_per_group];
  pread(fd, &inodeTable , sizeof(inodeTable) , blockSize * blockNum);
  char fileType = '?';
  int i = 0;
  for(i = 0; i < InodeTableSize; i++)
    {
      if(inodeTable[i].i_mode != 0 && inodeTable[i].i_links_count != 0)
		{
		  if(inodeTable[i].i_mode & S_IFREG)
		    fileType = 'f';
		  else if(inodeTable[i].i_mode & S_IFDIR)
		    fileType = 'd';
		  else if(inodeTable[i].i_mode & S_IFLNK)
		    fileType = 's';
		  char* aTime;
		  char* mTime;
		  char* cTime;
		  aTime = timeGetter( inodeTable[i].i_atime);
		  mTime = timeGetter(inodeTable[i].i_mtime);
		  cTime = timeGetter( inodeTable[i].i_ctime);
		  
		  fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d",iNum + i,fileType, inodeTable[i].i_mode & 0xFFF, inodeTable[i].i_uid, inodeTable[i].i_gid, inodeTable[i].i_links_count, cTime, mTime, aTime, inodeTable[i].i_size, inodeTable[i].i_blocks);
		  int j = 0;
		  for (j = 0; j < 15; j++)
		    {
		      fprintf(stdout, ",%u", inodeTable[i].i_block[j]);
		    }
		  fprintf(stdout, "\n");

		  if(fileType == 'd')
    		dirSummary(iNum+i, &inodeTable[i]);
		}
    }

}

void InodeBitMapSummary(int index)
{
  char IBitMap[blockSize];
  int numBytes = super.s_inodes_per_group / 8;
  //int off = 1024 + (j - 1) * blockSize;
  unsigned int follow = index * super.s_inodes_per_group + 1; 
  pread(fd, &IBitMap, numBytes, blockSize*index);

  for(int i = 0; i < numBytes; i++)
    {
      for(int j = 0; j < 8; j++)
	{
	  if(((IBitMap[i]  >> j) & 1) == 0)
	    {
	      fprintf(stdout, "IFREE,%d\n",follow); 
	    }
		follow++;
	}    
      
    }
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
	
	for(int i = 0; i < numGroups; i++){
		groupSummary(i);
		bitMapSummary(gd.bg_block_bitmap);
		InodeBitMapSummary(gd.bg_inode_bitmap);
		InodeSummary(gd.bg_inode_table);
	}
}
