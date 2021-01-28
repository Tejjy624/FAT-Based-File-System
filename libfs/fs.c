#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "disk.h"
#include "fs.h"

typedef struct __attribute__((__packed__)) superblock {
	char signature[8];
	uint16_t total_blocks;
	uint16_t root_index;
	uint16_t data_index;
	uint16_t data_blocks;
	uint8_t FAT_blocks;
	char padding[4079];
} superblock;

typedef struct __attribute__((__packed__)) file {
	char filename[FS_FILENAME_LEN]; //16
	uint32_t size;
	uint16_t firstblock;
	char padding[10];
} file;

// typedef struct __attribute__((__packed__)) fat {
// 	uint16_t table;
// } fat;

typedef struct __attribute__((__packed__)) descriptor {
	char filename[FS_FILENAME_LEN]; //16
	size_t offset;
	int position; //index in root array
	//possibly a pointer to the file
} descriptor;

typedef char block[BLOCK_SIZE]; //may not be packed, char may be wrong type!!!

superblock* SB;
file RT[FS_FILE_MAX_COUNT]; //128
// fat* FT;
uint16_t* FAT;
block* DATA; 
descriptor FD[FS_OPEN_MAX_COUNT]; //32

int min(int x, int y) 
{ 
	if (x < y) {
		return x;
	}
	return y;
}

int free_data(int block)
{
	FAT[block] = 0;
	for (int i = 0; i < BLOCK_SIZE; i++) {
		DATA[block][i] = 0;
	}
	return 0;
}

int fs_mount(const char *diskname)
{
	//check if disk can be opened
	if (block_disk_open(diskname) == -1) {
		return -1;
	}
	SB = malloc(sizeof(superblock));
	
	//read block into struct
	if (block_read(0, SB) == -1) {
		return -1;
	}
	
	//check for signature
	if (strncmp(SB->signature, "ECS150FS", 8) != 0) {
		return -1;
	}
	
	//check for total block count
	if (SB->total_blocks != block_disk_count()) {
		return -1;
	}
	
	//allocate FAT blocks
	FAT = malloc(BLOCK_SIZE * SB->FAT_blocks);
	
	//read FAT blocks
	for (int i = 0; i < SB->FAT_blocks; i++) {
		block_read(i+1, FAT + (i*BLOCK_SIZE));
	}
	
	//allocate root, array of files
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		// RT[i] = malloc(sizeof(file));
		RT[i].filename[0] = '\0';
	}

	//read root directory
	block_read(SB->root_index, RT);

	//allocate data block array
	DATA = malloc(BLOCK_SIZE * SB->data_blocks);
	
	//read data blocks
	for (int i = 0; i < SB->data_blocks; i++) {
		block_read(i+SB->data_index, DATA[i]);
	}

	//allocate file descriptor array
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		// FD[i] = malloc(sizeof(descriptor));
		FD[i].filename[0] = '\0';
	}
	
	return 0;
}

int fs_umount(void)
{
	//write any changes before unmounting
	block_write(0, SB);
	block_write(SB->root_index, RT);

	//write FAT blocks
	for (int i = 0; i < SB->FAT_blocks; i++) {
		block_write(i+1, FAT + (i*BLOCK_SIZE));
	}

	//write data blocks
	for (int i = 0; i < SB->data_blocks; i++) {
		block_write(i+SB->data_index, DATA[i]);
	}

	
	//check for open file descriptors
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (FD[i].filename[0] != '\0') {
			return -1;
		}
	}

	//free all structs
	free(FAT);
	free(SB);
	free(DATA);

	// //free files in root
	// for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
	// 	free(RT[i]);
	// }

	// //free file descriptor array
	// for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
	// 	free(FD[i]);
	// }
	
	//close disk
	if (block_disk_close() != 0) {
		return -1;
	}
	return 0;
}

int fs_info(void)
{
	//no underlying virtual disk was opened
	if (block_disk_count() == -1) {
		return -1;
	}

	//display information about currently mounted file system
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", SB->total_blocks);
	printf("fat_blk_count=%d\n", SB->FAT_blocks);
	printf("rdir_blk=%d\n", SB->root_index);
	printf("data_blk=%d\n", SB->data_index);
	printf("data_blk_count=%d\n", SB->data_blocks);
	int countfat = 0;
	for (int i = 0; i < SB->data_blocks; i++) {
		if(FAT[i] == 0) {
			countfat++;
		}
	}

	int countrdir = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (RT[i].filename[0] == '\0') {
			countrdir++;
		}
	}
	printf("fat_free_ratio=%d/%d\n", countfat, SB->data_blocks);
	printf("rdir_free_ratio=%d/%d\n", countrdir, FS_FILE_MAX_COUNT);
	
	return 0;
}

int fs_create(const char *filename)
{
	//check if filename is too long
	if (strlen(filename)+1 > FS_FILENAME_LEN) { 
		return -1;
	}

	//check if the file already exists (may occur after empty space)
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(RT[i].filename, filename) == 0) {
			return -1;
		}
	}
	
	//find an empty entry
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (RT[i].filename[0] == '\0') {
			strcpy(RT[i].filename, filename);
			RT[i].size = 0;
			RT[i].firstblock = 0xFFFF; //FAT_EOC

			//created file successfully
			return 0;
		}
	}
	
	//no empty space found, root directory is full
	return -1;
}

int fs_delete(const char *filename)
{
	//will not delete if file is currently open
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (strcmp(FD[i].filename, filename) == 0) {
			return -1;
		}
	}

	//find the correct file and delete
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(RT[i].filename, filename) == 0) {
			strcpy(RT[i].filename, "\0");
			//do we need to reset size and index?

			//data blocks containing the file’s contents must be freed in the FAT
			int currblock = RT[i].firstblock;
			//if file is empty and has no FAT entry
			if (currblock == 0xFFFF) {
				return 0;
			}
			int nextblock;
			while (1) {
				nextblock = FAT[currblock];
				free_data(currblock);

				if (nextblock == 0xFFFF) {
					break;
				}
				currblock = nextblock;
			}

			//deleted file successfully
			return 0;
		}
	}

	//no file with filename found
	return -1;
}

int fs_ls(void)
{
	//no underlying virtual disk was opened
	if (block_disk_count() == -1) {
		return -1;
	}

	printf("FS Ls:\n");
	//print all valid files in the root directory
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (RT[i].filename[0] != '\0') {
			printf("file: %s, ", RT[i].filename);
			printf("size: %d, ", RT[i].size);
			printf("data_blk: %d\n", RT[i].firstblock);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	int position = -1;
	//iterate through root directory
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(RT[i].filename, filename) == 0) {
			//a file with name filename has been found
			position = i;
			break;
		}
	}
	//no file named filename to open
	if (position == -1) {
		return -1;
	}

	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		//the same file can be open multiple times and will return different FDs

		if (FD[i].filename[0] == '\0') {
			strcpy(FD[i].filename, filename);
			FD[i].offset = 0;
			FD[i].position = position;

			//return index in the file descriptor array
			return i;
		}
	}

	//there are already FS_OPEN_MAX_COUNT files open
	return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */

	//fd is out of bounds or not currently open
	if (fd < 0 || fd >= FS_FILE_MAX_COUNT || FD[fd].filename[0] == '\0') {
		return -1;
	}

	// strcpy(FD[fd].filename, "\0", sizeof(FS_FILENAME_LEN));
	FD[fd].filename[0] = '\0';
	FD[fd].offset = 0;
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */

	//fd is out of bounds or not currently open
	if (fd < 0 || fd >= FS_FILE_MAX_COUNT || FD[fd].filename[0] == '\0') {
		return -1;
	}

	//return the current size of file
	return RT[FD[fd].position].size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */

	//fd is out of bounds or not currently open
	if (fd < 0 || fd >= FS_FILE_MAX_COUNT || FD[fd].filename[0] == '\0') {
		return -1;
	}

	// if offset is larger than current file size
	if (offset > RT[FD[fd].position].size) {
		return -1;
	}

	FD[fd].offset = offset;
	return 0;
}

int block_index(int fd)
{
	int curroffset = FD[fd].offset;
	int currblock = RT[FD[fd].position].firstblock;

	while (curroffset > BLOCK_SIZE) {
		curroffset -= BLOCK_SIZE;

		if (FAT[currblock] == 0xFFFF) {
			break;
		}
		currblock = FAT[currblock];
	}

	if (curroffset > BLOCK_SIZE) {
		//offset is somehow beyond file's last block?
		printf("Oh no...");
		return -1;
	}

	//return index of the data block cooresponding to file's offset
	return currblock;
}

int new_block(int fd)
{
	//locate the end of the file's data block chain
	int currblock = RT[FD[fd].position].firstblock;
	while (FAT[currblock] != 0xFFFF) {
		currblock = FAT[currblock];

		if (FAT[currblock] == 0) {
			//this may occur if we do not make last entry for file FAT_EOC
			printf("Error in FAT");
			// return -1;
		}
	}

	//TODO: might need to check boundary conditions for above while loop
	if (FAT[currblock] != 0xFFFF) {
		printf("Error in FAT:");
		// return -1;
	} 
	
	//look for empty space in FAT to put new block
	for (int i = currblock; i < SB->data_blocks; i++) {
		//TODO: double check that these are initialized to 0
		if (FAT[i] == 0) {
			FAT[currblock] = i;
			FAT[i] = 0xFFFF;
			return 0;
		}
	}

	//nowhere to put new block
	return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
	//fd is out of bounds or not currently open
	if (fd < 0 || fd >= FS_FILE_MAX_COUNT || FD[fd].filename[0] == '\0') {
		return -1;
	}
	int bytes_written = 0;
	int write_left = count; //remaining data to write
	// int file_left = fs_stat(fd) - FD[fd].offset; //size of current file - offset
	int block_count = 0; //file's current block that we are in
	int currblock = block_index(fd);
	int start_write = FD[fd].offset % BLOCK_SIZE; //position of offset in current block
	int first_size = BLOCK_SIZE-start_write; 
	int is_first_write = 1; //first read
	int full_disk = 1;

	//find first block for empty file
	if (currblock == 0xFFFF) {
		//no write occurs
		if (count == 0) {
			return 0;
		}

		//look for empty space in FAT to put new block
		for (int i = 1; i < SB->data_blocks; i++) {
			if (FAT[i] == 0) {
				FAT[i] = 0xFFFF;
				currblock = i;
				RT[FD[fd].position].firstblock = i;
				full_disk = 0;
				break;
			}
		}
		//full disk, no empty blocks!
		if (full_disk) {
			return 0;
		}
	}

	while(1) {
		//bytes to write are within the first block
		if (is_first_write) {
			if (write_left <= first_size) {
				memcpy(DATA[currblock]+start_write, buf, write_left);
				bytes_written += write_left;
				//we have now written up to count bytes
				break; 
			}
			
			memcpy(DATA[currblock]+start_write, buf, first_size);
			bytes_written += first_size;
			write_left -= first_size;
			block_count++;
			if (FAT[currblock] == 0xFFFF) {
				if (new_block(fd)) {
					//ran out of space to write to
					break;
				}
			}
			currblock = FAT[currblock];
			is_first_write = 0;
			continue;
		}

		if (write_left <= BLOCK_SIZE) {
			memcpy(DATA[currblock], buf + bytes_written, write_left);
			bytes_written += write_left;
			break; 
		}

		memcpy(DATA[currblock], buf + bytes_written, BLOCK_SIZE);
		bytes_written += BLOCK_SIZE;
		write_left -= BLOCK_SIZE;
		block_count++;
		if (FAT[currblock] == 0xFFFF) {
			if (new_block(fd)) {
				//ran out of space to write to
				break;
			}
		}
		currblock = FAT[currblock];
	}

	if (bytes_written+FD[fd].offset > RT[FD[fd].position].size) {
		RT[FD[fd].position].size = bytes_written+FD[fd].offset;
	}
	FD[fd].offset += bytes_written;
	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
    //fd is out of bounds or not currently open
	if (fd < 0 || fd >= FS_FILE_MAX_COUNT || FD[fd].filename[0] == '\0') {
		return -1;
	}

	char *bounce = malloc(BLOCK_SIZE);
	int bytes_read = 0;
	int read_left = count; //remaining data to read
	int file_left = fs_stat(fd) - FD[fd].offset; //size of current file - offset

	int block_count = 0; //file's current block that we are in
	int currblock = block_index(fd);

	int start_read = FD[fd].offset % BLOCK_SIZE; //position of offset in current block
	int first_size = BLOCK_SIZE-start_read;
	int is_first_read = 1; //first read
	int first_edge; //count or end of file boundary, whichever comes first
	
	while (1) {
		memcpy(bounce, DATA[currblock], BLOCK_SIZE);
		if (is_first_read) {
			first_edge = min(read_left, file_left);
			//offset and end of file or count on same block
			if (first_edge <= first_size) {
				memcpy(buf, bounce + start_read, first_edge);
				bytes_read += first_edge;
				//we have now read up to count bytes or end of file
				break; 
			}

			memcpy(buf, bounce+start_read, first_size);
			bytes_read += first_size;

			read_left -= first_size;
			file_left -= first_size;
			block_count++;

			if (FAT[currblock] == 0xFFFF) {
				break;
			}
			currblock = FAT[currblock];
			is_first_read = 0;
			continue;
		}

		first_edge = min(read_left, file_left);
		//offset and end of file or count on same block
		if (first_edge <= BLOCK_SIZE) {
			memcpy(buf + first_size + (block_count-1)*BLOCK_SIZE, bounce, first_edge);
			bytes_read += first_edge;
			//we have now read up to count bytes or end of file
			break; 
		}

		//read a full block into buffer and continue	
		memcpy(buf + first_size + (block_count-1)*BLOCK_SIZE, bounce, BLOCK_SIZE);
		bytes_read += BLOCK_SIZE;
		
		read_left -= BLOCK_SIZE;
		file_left -= BLOCK_SIZE;
		block_count++;

		if (FAT[currblock] == 0xFFFF) {
			break;
		}
		currblock = FAT[currblock];
	}

	//increment offset
	FD[fd].offset += bytes_read;
    //return the number of bytes actually read
	return bytes_read;
}

