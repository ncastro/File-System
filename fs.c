
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 6

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int sup = 0;
int *map; 

int fs_format()
{
	if(sup != 0) {
		printf("Format is not possible, disk already mounted\n");
		return 0;
	}
	else {
		union fs_block block;
		disk_read(0,block.data);

		block.super.magic = FS_MAGIC;
		block.super.nblocks = disk_size();
		block.super.ninodeblocks = ceil(disk_size()*0.1);
		block.super.ninodes = block.super.ninodeblocks*(DISK_BLOCK_SIZE/32);

		int i, j, l;
		int ninodeblocks = block.super.ninodeblocks;
		for(i = 1; i<=ninodeblocks;i++){
			disk_read(i,block.data);
			for(j=0;j<INODES_PER_BLOCK;j++){
				block.inode[j].isvalid = 0;
				block.inode[j].size = 0;
				for(l=0;l<POINTERS_PER_INODE;l++){
					block.inode[j].direct[l] = 0;
				}
			}
			disk_write( i,block.data);
		}
		return 1;
	} 
	
}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	if(sup != 0){

		printf("superblock:\n");
		printf("magic number is valid\n");
		printf("    %d blocks\n",block.super.nblocks);
		printf("    %d inode blocks\n",block.super.ninodeblocks);
		printf("    %d inodes\n",block.super.ninodes);

		int i, j, l;
		int ninodeblocks = block.super.ninodeblocks; 

		for(i = 1; i <= ninodeblocks; i++){
			disk_read(i,block.data);
			for(j=0;j<INODES_PER_BLOCK;j++){
				if(block.inode[j].isvalid == 1){
					printf("inode %d\n",j );
					printf("        size: %d bytes\n",block.inode[j].size );
					for(l=0;l<POINTERS_PER_INODE;l++){
						if(block.inode[j].direct[l] != 0) {
							printf("        blocks: \n");
							printf("               %d\n",block.inode[j].direct[l] );
						}		
					}
				}
			}		
		}
	}
	else
		printf("Disk not mounted\n");
}

int fs_mount()
{
	if(sup != 0){
		printf("Disk is already mounted\n");
		return 0;
	}
	else {
		union fs_block block;

		disk_read(0,block.data);

		if(block.super.magic != FS_MAGIC) {
			printf("The file system is not valid\n");
			return 0;
		}
		else if(block.super.nblocks == disk_size()) { 
			block.super.magic = FS_MAGIC;
			sup = 1;
			map = malloc((block.super.ninodes*6)*sizeof(int));
			int u;
			for(u =0; u<(block.super.ninodes*6);u++){
				map[u] = 0;
			}

			int i, j, l;
			int ninodeblocks = block.super.ninodeblocks;

			for(i = 1; i<=ninodeblocks;i++){
				disk_read(i,block.data);
				for(j=0;j<INODES_PER_BLOCK;j++){
					if(block.inode[j].isvalid == 1){
						for(l=0;l<POINTERS_PER_INODE;l++){
							if(block.inode[j].direct[l] != 0)
								map[block.inode[j].direct[l]] = 1;
						}
						
					}
				}
			}	
		}
		else {
			printf("Nblocks not valid\n");
			return 0;
		}
	}
		
	return 1;		
}

int fs_create()
{

	if(sup == 0){
		printf("Disk not mounted\n");
		return -1;
	}
	else{
		union fs_block block;
		disk_read(0,block.data);

		int i, j, l;
		int ninodeblocks = block.super.ninodeblocks; 

		for(i = 1; i<=ninodeblocks;i++){
			disk_read(i,block.data);
			for(j=0;j<INODES_PER_BLOCK;j++){
				if(block.inode[j].isvalid == 0){
					block.inode[j].isvalid = 1; 
					for(l=0;l<POINTERS_PER_INODE;l++){
						block.inode[j].direct[l] = 0;	
					}
					map[j] = 1;
					disk_write( i,block.data);
					return j;
				}
			}		
		}
	}
	return -1;
}

int inodeToBlock (int inode) {
	union fs_block block;
	disk_read(0,block.data);

	if(inode > block.super.ninodes)
		return -1;
	else
		return inode/INODES_PER_BLOCK+1;
}

int inodeToOffset(int inode) {
	union fs_block block;
	disk_read(0,block.data);

	if(inode > block.super.ninodes)
		return -1;
	else
		return inode % INODES_PER_BLOCK;
}

int inode_load(int number, struct fs_inode *inode) {
	union fs_block b;
	int block = inodeToBlock(number);

	if(block<0){
		return -1;
	}
	else{
		disk_read(block, b.data);
		int entry =  inodeToOffset(number);

		if(entry<0) {
			return -1;
		}
		*inode = b.inode[entry];
	}
	return 0;
}

int inode_save(int number, struct fs_inode *inode) {
	union fs_block b;
	int block = inodeToBlock(number);

	if(block<0){
		return -1;
	}
	else{
		disk_read(block, b.data);
		int entry =  inodeToOffset(number);

		if(entry<0) {
			return -1;
		}
		disk_read(block, b.data);
		b.inode[entry] = *inode;
		disk_write(block,b.data);
	}
	return 0;
}

int fs_delete( int number )
{
	if(sup !=0){
		struct fs_inode d;
		if(inode_load(number, &d)<0) {
		return 0;
		}
		else {
			if(d.isvalid == 0) {
				printf("Inode não valido\n");
				return 0;
			}
			else{
				d.isvalid = 0;
				inode_save(number, &d);
				int i = 0;

				while((i< INODES_PER_BLOCK) && (d.direct[i] !=0)) {
					map[d.direct[i]] = 1;
					i++;
				}
				return 1;
			}
		}
	}
	else
		printf("Disk not mounted\n"); return 0;

	
}

int fs_getsize( int inumber )
{
	if(sup !=0){

		struct fs_inode d;
		if(inode_load(inumber, &d)<0) {
			return -1;
		}
		else 
			return d.size;
	}
	else
		printf("Disk not mounted\n"); return -1;
	
}

void copy(char *a, char*b, int c){
	int i;
	for(i = 0;i<c;i++){
		b[i]=a[i];
	}
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if(sup != 0) {

		struct fs_inode f;
		inode_load(inumber,&f);
		if(f.isvalid == 0) {
				printf("Inode não valido\n");
				return -1;
		}
		else {
			int read = length;

			if(f.size <offset) {
				return -1;
			}

			else if(offset + length > f.size) {
				read = f.size - offset;
			}

			char block[DISK_BLOCK_SIZE];
			int n = 0;
			int currentOffset = offset;
			char *secAdress;
			char *dstAdress;
			int offsetIntoBlock;
			int nBytesCopy;

			while (n>read) {
				disk_read(f.direct[currentOffset/DISK_BLOCK_SIZE],block);
				offsetIntoBlock = currentOffset % DISK_BLOCK_SIZE;
				secAdress = block + currentOffset;
				dstAdress = data + currentOffset;
				nBytesCopy = DISK_BLOCK_SIZE - offsetIntoBlock;

				if(nBytesCopy>read) {
					nBytesCopy = read;
				}
				copy(secAdress, dstAdress, nBytesCopy);
				read -= nBytesCopy;
				currentOffset += nBytesCopy; 
			}
			return 1;
		}
	}
	else
		printf("Disk not mounted\n"); return -1;
	
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
