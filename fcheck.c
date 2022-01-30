// File Name: fcheck.c
// Author: Jagan Mohan Reddy Bijjam (JXM210003)
// Description: Checking the consistency of the given file system image


// Include Files
#include "types.h"
#include "fs.h"

// Include Libraries
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

// Some global variables which will be used across the functions of this file
// Most of these below variables are calculated in the main function and then functions use them for various checks
char* addr; 				// the starting address of the fs img that we have copied 
char* sbAddr; 				// address of the super block
char* inodeBlockStartAddr; 	// Address of the first inode block
char* dataBitmapStartAddr; 	// Address of the first Data Bitmap block
char* dataBlockStartAddr;	// Address of the first Data Block
uint noOfInodeBlocks; 		// Total number of inode blocks present
uint noOfDataBitmapBlocks;	// Total number of Data Bitmap blocks present
uint noOfDataBlocks;		// Total number of Data Blocks present

struct superblock* sb;		// struct to store the superblock info of the fs img given

#define BLOCK_SIZE (BSIZE)	// Size of the block


// Check 1
// Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). If not, print ERROR: bad inode.
void check1(){
	int i;

	// storing the first inode information in di
	struct dinode* di = (struct dinode *) inodeBlockStartAddr;

	// looping through each inode to check if it is proper or not
	for(i = 0; i < noOfInodeBlocks; i++, di++){
		if(di->type == 0 || di->type == T_DIR || di->type == T_DEV || di->type == T_FILE){
			continue;
		} else{
			fprintf(stderr, "ERROR: bad inode.\n");
			exit(1);
		}
	}

	return;
}

// Check 2
// For in-use inodes, each block address that is used by the inode is valid (points to a valid data block address within the image). If the direct block is used and is
// invalid, print ERROR: bad direct address in inode.; if the indirect block is in use and is invalid, print ERROR: bad indirect address in inode.
void check2(){
	int i, j;
	uint blockNumber;

	// storing the first inode information in di
	struct dinode* di = (struct dinode *) inodeBlockStartAddr;

	// looping through each inode to check its direct and indirect addresses
	for(i = 0; i < noOfInodeBlocks; i++, di++){

		// looping through all direct blocks of the selected inode
		for(j = 0; j < NDIRECT; j++){
			blockNumber = di->addrs[j];

			// block number should be in the range of the total number of blocks present in the given fs img
			if(blockNumber >= 0 && blockNumber < sb->size){
				continue;
			} else{
				fprintf(stderr, "ERROR: bad direct address in inode.\n");
				exit(1);
			}
		}

		// Getting the address of block with indirect addresses present
		uint indirectBlockNo = di->addrs[NDIRECT];
		if(indirectBlockNo == 0) continue;
		if(indirectBlockNo < 0 || indirectBlockNo >= sb->size){
			fprintf(stderr, "ERROR: bad indirect address in inode.\n");
			exit(1);
		}

		// Storing the first indirect entry information in indirectEntry
		uint* indirectEntry = (uint* ) (addr + indirectBlockNo*BLOCK_SIZE);

		// looping through each indirect entry and validating similar to above
		for(j = 0; j < NINDIRECT; j++, indirectEntry++){
			if(*indirectEntry >= 0 && *indirectEntry < sb->size){
				continue;
			} else{
				fprintf(stderr, "ERROR: bad indirect address in inode.\n");
				exit(1);
			}
		}
	}

	return;
}


// Check 3
// Root directory exists, its inode number is 1, and the parent of the root directory is itself. If not, print ERROR: root directory does not exist.
void check3(){

	// Storing the 0th inode information in rootInode
	struct dinode* rootInode = (struct dinode*)(inodeBlockStartAddr);
	// Incrementing it, to actually reach the rootInode at 1
	rootInode++;

	// Error if inode type is not directory
	if(rootInode->type != T_DIR){
		fprintf(stderr, "ERROR: root directory does not exist.\n");
		exit(1);
	}

	// Validating the . and .. functionality of root directory. 
	bool dotCheck = false, dotDotCheck = false;

	// Calculating the max number of dir entries in a single directory
	int maxDir = BLOCK_SIZE / (sizeof(struct dirent));

	int j;
	// looping through all the direct entries, to check for . and .. dirent's
	for(j = 0; j < NDIRECT; j++){

		// getting the block numbers pointed one by one from the root inode
		uint rootInodeBlockNumber = rootInode->addrs[j];
		if(rootInodeBlockNumber == 0) break;

		// getting the first dir entry at the location pointed by the direct address
		struct dirent* de = (struct dirent*) (addr + rootInodeBlockNumber*BLOCK_SIZE);

		int i;

		// looping through each dir entry to check if it is dot or dotdot
		for(i = 0; i < maxDir; i++, de++){

			// check for dot, and that it points to the root directory itself
			if(strcmp(".", de->name)==0){
				if(de->inum == 1) dotCheck = true;
			}

			// check for dotdot, and that it points to the root directory itself
			if(strcmp("..", de->name)==0){
				if(de->inum == 1) dotDotCheck = true;
			}

			// If both dot & dotdot found return without printing any error
			if(dotDotCheck && dotCheck) return;
		}
	}

	// print the error otherwise
	fprintf(stderr, "ERROR: root directory does not exist.\n");
	exit(1);
}

// Check 4
// Each directory contains . and .. entries, and the . entry points to the directory itself. If not, print ERROR: directory not properly formatted.
void check4(){
	// Initiating with the root inode and storing its number
	uint inodeNumber = 0;
	struct dinode* inode = (struct dinode*)(inodeBlockStartAddr);

	// root inode starts at 1, so incrementing accordingly
	inode++;inodeNumber++;

	int i, j, k;
	bool dotCheck, dotDotCheck;
	struct dirent* de;
	int maxDir = BLOCK_SIZE / (sizeof(struct dirent));

	// looping through each inode to know its contents
	for(i = 0; i < noOfInodeBlocks; i++, inode++, inodeNumber++){
		if(inode->type != 1) continue;
		dotCheck = false; dotDotCheck = false;

		// looping through each direct address to get the dir entries
		for(j = 0; j < NDIRECT; j++){
			uint inodeBlockNumber = inode->addrs[j];
			if(inodeBlockNumber == 0) break;
			de = (struct dirent*) (addr + inodeBlockNumber*BLOCK_SIZE);

			// looping through each block of dir entries to check for exitence of dot and dotdot
			for(k = 0; k < maxDir; k++, de++){

				// check for dot, and that it points to the root directory itself
				if(strcmp(".", de->name)==0){
					if(de->inum == inodeNumber) dotCheck = true;
					// fprintf(stderr, "dot found for %d inode\n", inodeNumber);
				}

				// check for dotdot
				if(strcmp("..", de->name) == 0){
					dotDotCheck = true;
					// fprintf(stderr, "dotDot found for %d inode\n", inodeNumber);
				}

				// if found then no need to search remaining
				if(dotCheck && dotDotCheck) break;
			}

			// if found for that inode, then break
			if(dotCheck && dotDotCheck) break;
		}

		// print error otherwise
		if(!dotCheck || !dotDotCheck){
			fprintf(stderr, "ERROR: directory not properly formatted.\n");
			exit(1);
		}
	}

	return;
}

// Check 5
// For in-use inodes, each block address in use is also marked in use in the bitmap. If not, print ERROR: address used by inode but marked free in bitmap.
void check5(){
	uint inodeNumber = 0;
	struct dinode* inode = (struct dinode*)(inodeBlockStartAddr);
	inode++;inodeNumber++;
	int i, j;

	// looping through each inode to know the block it is using
	for(i = 0; i < noOfInodeBlocks; i++, inode++, inodeNumber++){
		if(inode->type == 0) continue;

		// looping through each direct address to verify its bitmap counterpart
		for(j = 0; j < NDIRECT+1; j++){
			if(inode->addrs[j] == 0) break;

			uint offset = inode->addrs[j];

			// Checking if the bitmap shows that the block is truly occupied
			if(!((*(dataBitmapStartAddr + offset / 8) >> (offset % 8))&1)){				
				fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
				exit(1);
			}

		}

		// Same for indirect address pointers also to check for bitmap consistency
		if(inode->addrs[NDIRECT] == 0) continue;
		uint* indirectEntry = (uint* ) (addr + (inode->addrs[NDIRECT])*BLOCK_SIZE);

		// looping through each indirect entry for consistency check with bitmap
		for(j = 0; j < NINDIRECT; j++, indirectEntry++){
			uint offset = *indirectEntry;

			// Checking if the bitmap shows that the block is truly occupied
			if(!((*(dataBitmapStartAddr + offset / 8) >> (offset % 8))&1)){				
				fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
				exit(1);
			}
		}
		
	}

	return;
}


// Check 6
// For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect block somewhere. If not, print ERROR: bitmap marks block in use but it is not in use.
void check6(){

	// Creating a custom bitmap on our own for this consistency check
    int custom_bitmap[sb->nblocks];
    memset(custom_bitmap, 0, sb->nblocks*sizeof(int)); // initializing with zero values for the custom bitmap
    int temp, firstDataBlock = 1 + 1 + noOfInodeBlocks + noOfDataBitmapBlocks;

	struct dinode* inode = (struct dinode*)(inodeBlockStartAddr);
	uint i, j;

	// looping through each inode to get the data blocks in use
	for(i = 0; i < sb->ninodes; i++, inode++){
		if(inode->type == 0) continue;

		// looping through each direct address to get the blocks in use
		for(j = 0; j < NDIRECT; j++){
			temp = inode->addrs[j];
			if(temp == 0) continue;

			// storing information about the block in the custom bitmap
			custom_bitmap[temp - firstDataBlock] = 1;
		}

		// flagging the bit corresponding to the indirect address block
		temp = inode->addrs[NDIRECT];
		if(temp == 0) continue;
		custom_bitmap[temp - firstDataBlock] = 1;

		// looping through each indirect entry to get the blocks in use
		uint* indirectEntry = (uint* ) (addr + (temp)*BLOCK_SIZE);
		for(j = 0; j < NINDIRECT; j++, indirectEntry++){
			temp = *indirectEntry;
			if(temp == 0) continue;

			// storing information about the block in the custom bitmap
			custom_bitmap[temp - firstDataBlock] = 1;
		}
	}

	// comparing our custom bitmap array with the actual bitmap present
	// looping through all the data blocks
	for(i = 0; i < sb->nblocks; i++){
		j = (uint)(i + firstDataBlock);

		// Checking if the datablock info and the custombitmap info are consistent
		if(  ((*(dataBitmapStartAddr + j / 8) >> (j % 8))&1)   && custom_bitmap[i] == 0 ){
			fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
			exit(1);
		}
	}

	return;
}

// Check 7 and Check 8
// For in-use inodes, each direct address in use is only used once. If not, print ERROR: direct address used more than once.
// For in-use inodes, each indirect address in use is only used once. If not, print ERROR: indirect address used more than once.
void check7_8(){

	// using directBlocks array to store info about the direct addresses
	bool directBlocks[sb->nblocks];

	// initializing them with false. false means they haven't been marked as used yet.
    memset(directBlocks, false, sizeof(bool)* sb->nblocks);



    // using indirectBlocks array to store info about the direct addresses
    bool indirectBlocks[sb->nblocks];

    // initializing them with false. false means they haven't been marked as used yet.
    memset(indirectBlocks, false, sizeof(bool)* sb->nblocks);


    struct dinode* inode = (struct dinode*)(inodeBlockStartAddr);
    int temp, firstDataBlock = 1 + 1 + noOfInodeBlocks + noOfDataBitmapBlocks;
	uint i, j;

	// Making the custom direct and indirect address usage block
	// looping through each inode, to get its direct and indirect addresses
	for(i = 0; i < sb->ninodes; i++, inode++){
		if(inode->type == 0) continue;

		// looping through each of the direct addresses of the inode
		for(j = 0; j < NDIRECT; j++){
			temp = inode->addrs[j];
			if(temp == 0) continue;

			// if the direct block was already flagged as used, printing the error
			if(directBlocks[temp-firstDataBlock]){
				fprintf(stderr, "ERROR: direct address used more than once.\n");
				exit(1);
			}

			// else, marking the direct block address as used.
			directBlocks[temp-firstDataBlock] = true;
		}

		// checking the consistency of the direct address which points to the indirect addresses blocks
		temp = inode->addrs[NDIRECT];
		if(temp == 0) continue;

		// if the direct block was already flagged as used, printing the error
		if(directBlocks[temp-firstDataBlock]){
			fprintf(stderr, "ERROR: direct address used more than once.\n");
			exit(1);
		}

		// else, marking the direct block address as used.
		directBlocks[temp-firstDataBlock] = true;




		// looping through each of the indirect address entries
		uint* indirectEntry = (uint* ) (addr + (temp)*BLOCK_SIZE);
		for(j = 0; j < NINDIRECT; j++, indirectEntry++){
			temp = *indirectEntry;
			if(temp == 0) continue;

			// if the direct block was already flagged as used, printing the error
			if(indirectBlocks[temp-firstDataBlock]){
				fprintf(stderr, "ERROR: indirect address used more than once.\n");
				exit(1);
			}

			// else, marking the direct block address as used.
			indirectBlocks[temp-firstDataBlock] = true;
		}
	}

	return;
}

// Recursive helper function to map the whole directories and inodes
// Used for checks 9, 10, 11, and 12
void helper(struct dinode* inode, int* trackInodes){
	// checking if inode type is directory or not
	if(inode->type != T_DIR) return;

	int i, j, temp;
	struct dirent* de;
	struct dinode* passInode;

	// looping through each direct address of the inode, to map it further
	for(i = 0; i < NDIRECT; i++){
		if(inode->addrs[i] == 0) continue;

		// looping through all the directory entries present for mapping
		de = (struct dirent*)(addr + (inode->addrs[i]) * BLOCK_SIZE);
		for(j = 0; j < DPB; j++, de++){

			// checking if directory entry is valid, and that they are not dot & dotdot
			if(de->inum != 0 && strcmp(".", de->name)!=0 && strcmp("..", de->name)!=0){
				trackInodes[de->inum]++;
				passInode = ((struct dinode*)(inodeBlockStartAddr)) + de->inum;

				// If valid directory entry found, map it also using this recursive function
				helper(passInode, trackInodes);
			}
		}
	}

	if(inode->addrs[NDIRECT] == 0) return;
	uint* indirectEntry = (uint* )(addr + (inode->addrs[NDIRECT]) * BLOCK_SIZE);

	// looping through each indirect address of the inode, to map it further
	for(i = 0; i < NINDIRECT; i++, indirectEntry++){
		temp = *indirectEntry;
		if(temp == 0) continue;

		// looping through all the directory entries present for mapping in the indirect address
		de = (struct dirent*)(addr + temp * BLOCK_SIZE);
		for(j = 0; j < DPB; j++, de++){

			// checking if directory entry is valid, and that they are not dot & dotdot
			if(de->inum != 0 && strcmp(".", de->name)!=0 && strcmp("..", de->name)!=0){
				trackInodes[de->inum]++;
				passInode = ((struct dinode*)(inodeBlockStartAddr)) + de->inum;

				// If valid directory entry found, map it also using this recursive function
				helper(passInode, trackInodes);
			}
		}
	}

	return;
}	


// Checks 9, 10, 11, and 12
// For all inodes marked in use, each must be referred to in at least one directory. If not, print ERROR: inode marked use but not found in a directory.
// For each inode number that is referred to in a valid directory, it is actually marked in use. If not, print ERROR: inode referred to in directory but marked free.
// Reference counts (number of links) for regular files match the number of times file is referred to in directories (i.e., hard links work correctly). If not, print ERROR: bad reference count for file.
// No extra links allowed for directories (each directory only appears in one other directory). If not, print ERROR: directory appears more than once in file system.
void check9_10_11_12(){

	// check 9, 10, 11, and 12 need the whole directory mapping information for their consistent check
	// using trackInodes to map how many times each of them has been used
	int trackInodes[sb->ninodes];
    memset(trackInodes, 0, sizeof(int)* sb->ninodes); // initializing the inodes usage count with 0

    // get the root inode, to start the whole directory mapping
    struct dinode* rootInode = (struct dinode*) inodeBlockStartAddr;
    rootInode++;

    // set this inode usage value to one and call the recursive mapper function
    trackInodes[1] = 1;
    helper(rootInode, trackInodes);

    struct dinode* inode = rootInode;
    // struct dinode* inode = ++rootInode;
    int i;
    for(i = 1; i < sb->ninodes; i++, inode++){

    	// checking if inode in use, is actually used by a directory
    	if(inode->type != 0 && trackInodes[i] == 0){
    		fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
    		exit(1);
    	}

    	// checking if inode found in a directory, is actually in use
    	if(trackInodes[i] > 0 && inode->type == 0){
    		fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
    		exit(1);
    	}

    	// checking if the type is file, then reference count of links matches those in directory mapping
    	if(inode->type == T_FILE && trackInodes[i] != inode->nlink){
    		fprintf(stderr, "ERROR: bad reference count for file.\n");
    		exit(1);
    	}

    	// checking if the type is directory, then only one reference count exists (apart from dot and dotdot)
    	if(inode->type == T_DIR && trackInodes[i] > 1){
    		fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
    		exit(1);
    	}
    }

    return;
}

int main(int argc, char* argv[]){

	// Required initial variables
	int fsfd; 			// to store file descriptor of the fs img
	struct stat fStat;	// to store stat information of the fs img


	// fcheck accepts only one argument
	if(argc != 2 ){
		fprintf(stderr, "Usage: fcheck <file_system_image>\n");
		exit(1);
	} 

	// open the given fs img
	fsfd = open(argv[1], O_RDONLY);
	if(fsfd < 0){
		perror(argv[1]);
		exit(1);
	}

	// get stat on the fs img
	if(fstat(fsfd, &fStat) < 0){
		exit(1);
	}

	// copy the fs img to our own ram, and store its starting address in the addr
	addr = mmap(NULL, fStat.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
	if (addr == MAP_FAILED){
		perror("mmap failed");
		exit(1);
	}

	// store the super block info of the fs img into the global variable sb
	sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);

	// calculating the number of inode blocks present in the fs img
	noOfInodeBlocks = sb->ninodes / IPB + 1;

	// calculating the number of data bitmap blocks present in the fs img
	noOfDataBitmapBlocks = sb->size / (BLOCK_SIZE * 8) + 1;

	// getting the total number of data blocks present
	noOfDataBlocks =  sb->nblocks;

	// calculating the address of the first inode block
	inodeBlockStartAddr = addr + (BLOCK_SIZE * 2);

	// calculating the address of the first data bitmap block
	dataBitmapStartAddr = addr + (BLOCK_SIZE * (2 + noOfInodeBlocks));

	// calculating the address of the first data block
	dataBlockStartAddr  = addr + (BLOCK_SIZE * (2 + noOfInodeBlocks + noOfDataBitmapBlocks));

	// confirming the number of blocks calculated individually equals the total block present in the fs imgs
	assert(2 + noOfInodeBlocks + noOfDataBitmapBlocks + noOfDataBlocks == sb->size);

	// performing checks as per the project 4 description
	check1();
	check2();
	check3();
	check4();
	check5();
	check6();
	check7_8();
	check9_10_11_12();

	// EOP
	return 0;
}