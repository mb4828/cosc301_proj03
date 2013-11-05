#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// function declarations
void *malloc(size_t);
void free(void *);
void dump_memory_map(void);

const int HEAPSIZE = (1*1024*1024); // 1 MB
const int MINIMUM_ALLOC = sizeof(int) * 2;

// global file-scope variables for keeping track
// of the beginning of the heap.
void *heap_begin = NULL;
void *freelist = NULL;

static void mod_header(void *block, int size, int offset) {
	int *iblock = (int*)block;
	if (size != -1)
		iblock[0] = size;
	if (offset != -1)
		iblock[1] = offset;
}

static void get_header(void *block, int *size, int *offset) {
	int *iblock = (int*)block;
	*size = iblock[0];
	*offset = iblock[1];
}

static int is_buddy(void *heap_begin, void *block1, void *block2) {
	int heap_add = (uint64_t)(&heap_begin);
	int b1 = (uint64_t)(&block1)-heap_add;
	int b2 = (uint64_t)(&block2)-heap_add;
	b1 = b1^b2;

	int bit_diff = 0;
	int x=0;
	for (;x<64;x++) {
		if ((b1 & 0x1) == 1)
			bit_diff++;
		b1 = b1 >> 1;
	}

	if (bit_diff == 1)
		return 1;
	return 0;	
}

void *malloc(size_t request_size) {

    	// if heap_begin is NULL, then this must be the first
    	// time that malloc has been called.  ask for a new
    	// heap segment from the OS using mmap and initialize
    	// the heap begin pointer.
	/*
    	if (!heap_begin) {
        	heap_begin = mmap(NULL, HEAPSIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        	atexit(dump_memory_map);
	}
	*/

	// calculate request size
	request_size += 8;
	int nearest_block = MINIMUM_ALLOC;
	while (nearest_block < request_size) {
		nearest_block = nearest_block*2;
	}

	void *tmp = NULL;
	return tmp;
}

void free(void *memory_block) {

}

void dump_memory_map(void) {
	uint8_t *ptr = (uint8_t*)heap_begin;
	int *size = NULL; 
	int *offset = NULL;
	int size = 0; 
	int offset = 0;
	
	while ((&ptr-&heap_begin) < HEAPSIZE) {
		get_header((void*)ptr, size, offset);
		size = *size;
		offset = *offset;

		if (next != 0) {
			// found a free block
			printf("Block size: %i, offset %i, free\n", size, offset);
		} 
		else {
			if ( (&(ptr+size))<(HEAPSIZE-1) || (&freelist!=&ptr) {
				// block is allocated
				printf("Block size: %i, offset %i, allocated\n", size, offset);
			} else {
				// block is last block in heap and is free
				printf("Block size: %i, offset %i, free\n", size, offset);
			}
		} 
	}
	
	ptr+=size;
}

