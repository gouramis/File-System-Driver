#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "disk.h"
#include "fs.h"
#define BLOCK_SIZE 4096
#define FAT_EOC 0xFFFF
// -- Structs -- //
/* Here are the structures for the blocks to hold metadata */
struct __attribute__((packed)) superblock {
	uint8_t signature[8];
	uint16_t total_blocks_on_disk;
	uint16_t root_dir_index;
	uint16_t data_block_start_index;
	uint16_t amount_of_data_blocks;
	uint8_t num_of_blocks_for_FAT;
	uint8_t padding[4079];
};
typedef struct __attribute__((packed)) root_file_entry {
	uint8_t filename[16];
	uint32_t filesize;
	uint16_t first_data_block_index;
	uint8_t padding[10];
} file_entry;
struct __attribute__((packed)) root_dir {
	file_entry dir[128]; //this is the array of 128 entries holding the files
};
struct filesystem {
	struct superblock *fs_superblock; // This is the superblock
	uint16_t *fs_FAT; // This is the FAT
	struct root_dir *fs_root_dir;  // This is the root directory
};
typedef struct filedescriptor {
	// Open flag, set 1 if fd is open 0 if closed
	uint8_t open;
	// Current offset
	size_t file_offset;
	// Root directory index
	int root_index;
	// Name of the file
	uint8_t filename[16];
} filedes;
// -- Static Vars -- //
static struct filesystem *fs;
static filedes filedes_table[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
	// Is the disk already mounted/open?
	if(block_disk_open(diskname) == -1) return -1;
	// Allocate new superblock struct to hold metainformation
	struct superblock *new_superblock = malloc(sizeof(struct superblock));
	// Grab the superblock at index 0 of disk
	block_read(0, new_superblock);
	// Check the signature of the disk
	char correct_fs_sig[9] = "ECS150FS";
	char disk_sig[9];
	for(int i = 0; i < 8; i++) disk_sig[i] = new_superblock->signature[i];
	if(strcmp(disk_sig, correct_fs_sig)) return -1; // Invalid disk signature
	// Allocate enough space for the FAT array
	int fat_size = (new_superblock->num_of_blocks_for_FAT * BLOCK_SIZE);
	uint16_t *new_fat = malloc(fat_size);
	// Read the FAT data in
	for (int i = 1; i <= new_superblock->num_of_blocks_for_FAT; i++){
		// We have 4096/2 fs_fat entries per block
		// Make a 4096 byte buffer and fill it with read data
		uint16_t buf[BLOCK_SIZE/2];
		block_read(i, &buf);
		int offset = (i-1)*(BLOCK_SIZE/2);
		for (int j = 0; j < BLOCK_SIZE/2; j++) {
			new_fat[j+offset] = buf[j];
		}

	}
	// Allocate a new root directory
	struct root_dir *new_root_dir = malloc(sizeof(struct root_dir));
	// Get root directory index
	int root_start = new_superblock->root_dir_index;
	// Read in the root directory
	block_read(root_start, new_root_dir);
	// Allocate file system structure to preserve changes:
	fs = malloc(sizeof(struct filesystem));
	fs->fs_superblock = new_superblock;
	fs->fs_FAT = new_fat;
	fs->fs_root_dir = new_root_dir;
	return 0;
}

int fs_umount(void)
{
	if (fs == NULL) return -1;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (filedes_table[i].open == 1) return -1;
	}
	// Write the FAT and root_dir back to the disk
	// Write back fat
	for (int i = 1; i <= fs->fs_superblock->num_of_blocks_for_FAT; i++){
		// We have 4096/2 fs_fat entries per block
		// Make a 4096 byte buffer and fill it with fs_fat data
		uint16_t buf[BLOCK_SIZE/2];
		int offset = (i-1)*BLOCK_SIZE/2;
		for (int j = 0; j < BLOCK_SIZE/2; j++) {
			buf[j] = fs->fs_FAT[j+offset];
		}
		block_write(i, &buf);
	}
	// Write root dir
	block_write(fs->fs_superblock->root_dir_index, fs->fs_root_dir);
	// Free allocated structure memory:
	free(fs->fs_superblock);
	free(fs->fs_FAT);
	free(fs->fs_root_dir);
	free(fs);
	// Set the global vars back to NULL
	fs = NULL;
	// Close the file
	if(block_disk_close() == -1) return -1;
	return 0;
}

int fs_info(void)
{
	if (!fs) return -1; // No disk has been opened
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", fs->fs_superblock->amount_of_data_blocks +
		fs->fs_superblock->num_of_blocks_for_FAT + 2);
	printf("fat_blk_count=%d\n", fs->fs_superblock->num_of_blocks_for_FAT);
	printf("rdir_blk=%d\n",fs->fs_superblock->root_dir_index);
	printf("data_blk=%d\n",fs->fs_superblock->root_dir_index+1);
	printf("data_blk_count=%d\n",fs->fs_superblock->amount_of_data_blocks);
	int num_free_blocks = 0;
	for (int i = 0; i < fs->fs_superblock->amount_of_data_blocks; i++)
		if (fs->fs_FAT[i] == 0) num_free_blocks++;
	printf("fat_free_ratio=%d/%d\n", num_free_blocks,
		fs->fs_superblock->amount_of_data_blocks);
	int num_free_root_entries = 0;
	for (int j = 0; j < FS_FILE_MAX_COUNT; j++){
		if (fs->fs_root_dir->dir[j].filename[0] == 0) num_free_root_entries++;
	}
	printf("rdir_free_ratio=%d/%d\n",num_free_root_entries,
		FS_FILE_MAX_COUNT);
	return 0;
}

static int root_strcmp(int file_index, const char* filename) {
	return strcmp((char*)fs->fs_root_dir->dir[file_index].filename, filename);
}

int fs_create(const char *filename)
{
	if (!filename) return -1;
	if(strlen(filename) > FS_FILENAME_LEN) return -1;
	// Check if any slots have the same file name
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
		if(!root_strcmp(i,filename)) return -1;
	// Search root directory for empty spot
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(fs->fs_root_dir->dir[i].filename[0] == 0){
			// Create the new file
			strcpy((char*)fs->fs_root_dir->dir[i].filename, filename);
			fs->fs_root_dir->dir[i].filesize = 0;
			fs->fs_root_dir->dir[i].first_data_block_index = FAT_EOC;
			return 0;
		}
	}
	// No empty slots in FATfs->
	return -1;
}

int fs_delete(const char *filename)
{
	if (!fs) return -1;
	if (!filename) return -1;
	if(strlen(filename) > FS_FILENAME_LEN) return -1;
	// Check if the file is currently open
	for (int j = 0; j < FS_OPEN_MAX_COUNT; j++) {
		// strcmp with @filename
		// if open return -1;
		if(!strcmp((char*)filedes_table[j].filename, filename)){
			if(filedes_table[j].open) return -1;
		}
	}
	// Find the files
	int i;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(!root_strcmp(i, filename))
			break;
	}
	// If the file was not found; Return failure
	// Safe to change filename to zero after this conditional
	if (i == FS_FILE_MAX_COUNT) return -1;
	// We have found the file to delete
	// Set the first char of its filename to a zero i.e. "\0"
	fs->fs_root_dir->dir[i].filename[0] = 0;
	// Trace the FAT and set all values to zero
	int cur_block = fs->fs_root_dir->dir[i].first_data_block_index;
	while (cur_block != FAT_EOC) {
		int nextvalue = fs->fs_FAT[cur_block];
		fs->fs_FAT[cur_block] = 0;
		cur_block = nextvalue;
	}
	// return success
	return 0;
}

int fs_ls(void)
{
	if (!fs) return -1;
	printf("FS Ls:\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (fs->fs_root_dir->dir[i].filename[0] != 0) {
			// Extract file name
			char fname[FS_FILENAME_LEN];
			for (int f = 0; f < FS_FILENAME_LEN; f++)
				fname[f] = fs->fs_root_dir->dir[i].filename[f];
			// We have found a file
			printf("file: %s, size: %d, data_blk: %d\n",
				fname, fs->fs_root_dir->dir[i].filesize,
				fs->fs_root_dir->dir[i].first_data_block_index);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
	if (!fs) return -1;
	if (!filename) return -1;
	// Make sure we don't have the max number of open files
	int count = 0;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (filedes_table[i].open == 1) count++;
	}
	if (count >= FS_OPEN_MAX_COUNT) return -1;
	// Check that the filename is not too long
	if(strlen(filename) > FS_FILENAME_LEN) return -1;
	// Now look for the file
	int i;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(root_strcmp(i, filename)) continue;
		// We have found the file
		filedes newfiledes;
		newfiledes.file_offset = 0;
		// This was we can tell if the fd is open or not
		newfiledes.open = 1;
		newfiledes.root_index = i;
		strcpy((char*)newfiledes.filename,
			(char*)fs->fs_root_dir->dir[i].filename);
		// find the next open slot in the filedes_table
		for (int j = 0; j < FS_FILE_MAX_COUNT; j++) {
			// If open == 0 then the file is closed and we can proceed
			if (filedes_table[j].open == 0){
				// Found open slot
				memcpy(&filedes_table[j], &newfiledes, sizeof(filedes));
				return j;
			}
		}
	}
	// No file name found to open
	return -1;
}

int fs_close(int fd)
{
	// Bounds checking
	if (fd < 0 || fd > 31) return -1;
	// Check if there is an open file in filedes_table[fd]
	if (filedes_table[fd].open == 0) return -1;
	// Otherwise, we have a valid file descriptor
	else {
		// Set all values inside the file descriptor to zero (close the fd)
		filedes_table[fd].open = 0;
		filedes_table[fd].file_offset = 0;
		filedes_table[fd].root_index = 0;
		// Set the first character of filename to null character
		filedes_table[fd].filename[0] = 0;
	}
	return 0;
}

int fs_stat(int fd)
{
	// Bounds checking
	if (fd < 0 || fd > 31) return -1;
	// Checking if file is not currently open:
	if (filedes_table[fd].open == 0) return -1;
	int rootindex = filedes_table[fd].root_index;
	int filesize = fs->fs_root_dir->dir[rootindex].filesize;
	return filesize;
}

int fs_lseek(int fd, size_t offset)
{
	// Bounds checking
	if (fd < 0 || fd > 31) return -1;
	// Checking if file is not currently open:
	if (filedes_table[fd].open == 0) return -1;
	// Check if offset is larger than filesize:
	int rootindex = filedes_table[fd].root_index;
	uint32_t filesize = fs->fs_root_dir->dir[rootindex].filesize;
	if (offset > filesize) return -1;
	// Set the offset for the fd to the offset given
	filedes_table[fd].file_offset = offset;
	return 0;
}

// This is a helper function to find the index of the data block corresponding
// to the file's offset
static int offset_to_block(int fd, size_t offset) {
	// For an open file with file descriptor fd, find the block
	// coordinating to the current offset in the file
	// Returns the ABSOLUTE index of the data block, i.e. what
	// block_read or block_write will actually use
	// Make sure the file at @fd is actually open
	if (filedes_table[fd].open == 0) return -1;
	// Find how many blocks from the start we need to traverse
	int num_blocks_to_traverse = offset / BLOCK_SIZE;
	// First find the file's FIRST data block
	uint16_t file_first_data_block = fs->fs_root_dir->
		dir[(filedes_table[fd].root_index)].first_data_block_index;
	// Trace the FAT until we get the desired block OR
	// reach the end of the file.
	int cur_block = file_first_data_block;
	while (num_blocks_to_traverse > 0) {
		// Check if we have reached the end of the file
		if (fs->fs_FAT[cur_block] == FAT_EOC) {
			break;
		}
		cur_block = fs->fs_FAT[cur_block];
		num_blocks_to_traverse--;
	}
	int absolute_first_data_block_index = 2 +
		fs->fs_superblock->num_of_blocks_for_FAT;
	return absolute_first_data_block_index + cur_block;
}

static int find_first_open_FAT(){
	// Find the first open FAT
	int index = 0;
	for (int i = 1; i < fs->fs_superblock->num_of_blocks_for_FAT
		* BLOCK_SIZE; i++){
			if(fs->fs_FAT[i] == 0) {
				index = i;
				break;
			}
	}
	// if zero, the FAT is full
	if (index == 0) return -1;
	// If we are out of data blocks, return -1
	if (index > fs->fs_superblock->amount_of_data_blocks - 1) return -1;
	return index;
}

static int FAT_to_abs(int fat_index) {
	return fat_index + 2 + fs->fs_superblock->num_of_blocks_for_FAT;
}

int fs_write(int fd, void *buf, size_t count)
{
	// Bounds checking
	if (fd < 0 || fd > 31) return -1;
	if (buf == NULL) return -1;
	// Check if there is an open file in filedes_table[fd]
	if (filedes_table[fd].open == 0) return -1;
	if (count == 0) return 0;
	// offset for the input buffer
	size_t input_offset = 0;
	// Return value
	int num_bytes_written = 0;
	// End condition
	size_t num_bytes_to_write = count;
	// Get a pointer to the current offset
	size_t* curr_offset = &filedes_table[fd].file_offset;
	// Quick reference to root index
	int rootindex = filedes_table[fd].root_index;
	// This should be the new offset once we are finished writing
	size_t final_offset = count + *curr_offset;
	// Check how far we are overwriting
	size_t og_filesize = fs->fs_root_dir->dir[rootindex].filesize;
	// Initialize our block buffer
	uint8_t bounce_buffer[4096];
	int curr_block;
	int last_block = 0;
	uint16_t *prev_block;
	if (fs->fs_root_dir->dir[rootindex].first_data_block_index == FAT_EOC){
		curr_block = FAT_EOC;
		// If curr_block IS FAT_EOC we need to modify the root reference
		prev_block = &(fs->fs_root_dir->dir[rootindex].first_data_block_index);
	} else if ((*curr_offset % BLOCK_SIZE) == 0){
		// When the OFFSET is just one past the block size
		// Set curr_block to fat EOC to immediately allocate a block
		curr_block = FAT_EOC;
		int lblock = offset_to_block(fd, filedes_table[fd].file_offset);
		prev_block = &fs->fs_FAT[lblock];
	} else {
		curr_block = offset_to_block(fd, filedes_table[fd].file_offset);
		// Get the FAT index for the fd:
		curr_block -= (2 + fs->fs_superblock->num_of_blocks_for_FAT);
		prev_block = &fs->fs_FAT[curr_block];
	}
	do {
		// Check if we need more space:
		if(curr_block == FAT_EOC){
			curr_block = find_first_open_FAT();
			// If we have no space left, just return what we have written so far
			if (curr_block == -1) break;
			*prev_block = curr_block;
			fs->fs_FAT[curr_block] = FAT_EOC;
		}
		// Need absolute index for reading
		int abs_curr = FAT_to_abs(curr_block);
		// Read the current block into our bounce buffer
		block_read(abs_curr, &bounce_buffer);
		// Our buffer now holds the data from the current block
		// Num of bytes we need to write to this block is
		// 4096 - (offset) if offset->end
		// it is offset->end if we need to write more than we have left in this
		// block
		int num_bytes_to_copy;
		if (BLOCK_SIZE - (*curr_offset % BLOCK_SIZE) < num_bytes_to_write)
			num_bytes_to_copy = BLOCK_SIZE - (*curr_offset % BLOCK_SIZE);
		// (final_offset - cur_offset) if offset->final_offset
		else
			num_bytes_to_copy = final_offset - *curr_offset;
		// Copy relevant data
		memcpy(&bounce_buffer[*curr_offset % BLOCK_SIZE], buf + input_offset,
			num_bytes_to_copy);
		// Now write back to the disk
		block_write(abs_curr, bounce_buffer);
		// Adjust indicators
		input_offset += num_bytes_to_copy;
		num_bytes_to_write -= num_bytes_to_copy;
		*curr_offset += num_bytes_to_copy;
		num_bytes_written += num_bytes_to_copy;
		if (num_bytes_to_write > 0)
			prev_block = &fs->fs_FAT[curr_block];
		last_block = curr_block;
		curr_block = fs->fs_FAT[curr_block];
	} while (num_bytes_to_write > 0);
	// Rewrite new FAT_EOC
	if (*curr_offset > og_filesize) {
		fs->fs_root_dir->dir[rootindex].filesize = (uint32_t) *curr_offset;
		fs->fs_FAT[last_block] = FAT_EOC;
	}
	return num_bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	// Bounds checking
	if (fd < 0 || fd > 31) return -1;
	// Check if there is an open file in filedes_table[fd]
	if (filedes_table[fd].open == 0) return -1;
	if (buf == NULL) return -1;
	size_t num_bytes_to_read = count;
	// How we index the ouput buf*
	size_t output_offset = 0;
	// Return value
	int num_bytes_copied = 0;
	// Get a pointer to the current offset
	size_t* offset = &filedes_table[fd].file_offset;
	// Quick reference to root index
	int rootindex = filedes_table[fd].root_index;
	// Quick reference to filesize
	uint filesize = fs->fs_root_dir->dir[rootindex].filesize;
	// Current block index
	int db_index;
	// This should be the new offset once we are finished reading
	size_t final_offset = count + *offset;
	// Block index of where we stop reading
	int final_db_index = offset_to_block(fd,final_offset);
	// Initialize our block buffer
	uint8_t bounce_buffer[4096];
	while (*offset < filesize && num_bytes_to_read > 0
		&& *offset < final_offset) {
		db_index = offset_to_block(fd,filedes_table[fd].file_offset);
		// Fill the buffer
		block_read(db_index, &bounce_buffer);
		// Num of bytes we need to read in this block is
		// 4096 - (offset) if offset->end
		// it is offset->end if db_index is NOT final_db_index
		int num_bytes_to_copy;
		if (db_index != final_db_index)
			num_bytes_to_copy = BLOCK_SIZE - (*offset % BLOCK_SIZE);
		// (final_offset - cur_offset) if offset->final_offset
		// it is offset->final_offset
		if (db_index == final_db_index)
			num_bytes_to_copy = final_offset - *offset;
		// Copy relevant data
		memcpy(buf + output_offset,
			&bounce_buffer[*offset % BLOCK_SIZE],
			num_bytes_to_copy);
		// Adjust indicators
		output_offset += num_bytes_to_copy;
		num_bytes_to_read -= num_bytes_to_copy;
		*offset += num_bytes_to_copy;
		num_bytes_copied += num_bytes_to_copy;
	}
	return num_bytes_copied;
}
