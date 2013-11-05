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

static void malloc_helper(uint8_t *ptr, int request_size) {
	int *s=NULL;
	int *o=NULL;
	get_header(ptr, s, o);
	int size = *s;
	int offset = *o;

	if ((size/2) < request_size) {
		// can't divide further; mark as allocated and return
		mod_header(ptr, size, 0);
		return;
	}
	
	size /= 2;
	uint8_t *ptr2 = ptr + size;
	mod_header(ptr, size, size);
	if (offset == 0) {
		mod_header(ptr2, size, 0);
	} else {
		mod_header(ptr2, size, (offset-size));
	}

	malloc_helper(ptr, request_size);

	return;
}

void *malloc(size_t request_size) {
    	// if heap_begin is NULL, then this must be the first
    	// time that malloc has been called.  ask for a new
    	// heap segment from the OS using mmap and initialize
    	// the heap begin pointer.
    	if (!heap_begin) {
        	heap_begin = mmap(NULL, HEAPSIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        	atexit(dump_memory_map);
		mod_header(heap_begin, HEAPSIZE, 0);
		freelist = heap_begin;
	}

	// calculate request size
	request_size += 8;
	int nearest_block = MINIMUM_ALLOC;
	while (nearest_block < request_size) {
		nearest_block = nearest_block*2;
	}
	request_size = nearest_block;

	// find a block of memory using first fit
	uint8_t *ptr = freelist;
	int prevoffset = 0;
	int *size=NULL;
	int *offset=NULL;

	while(1) {
		get_header(ptr, size, offset);

		if (*size < request_size) {
			// block is too small
			if (*offset == 0) {
				// no more blocks; return a null pointer
				ptr = NULL;
				return ptr;
			} else {
				// go to next free block
				ptr += *offset;
				prevoffset = *offset;
			}
		} else {
			// we've got our block
			break;
		}
	}

	// recursively divide the block to reduce fragmentation
	malloc_helper(ptr, request_size);

	// update freelist
	get_header(ptr, size, offset);
	if (&freelist == (void*)&ptr) {
		uint8_t *freeptr = (uint8_t*)freelist;
		freeptr += *offset;
		freelist = (void*)freeptr;
	} else {
		uint8_t *ptr2 = ptr - prevoffset;
		mod_header(ptr2, -1, prevoffset+*size);
	}

	// return user space of block
	ptr += 8;
	return ptr;
	
}

void free(void *memory_block) {

}

void dump_memory_map(void) {
	uint8_t *ptr = (uint8_t*)heap_begin;
	int *s = NULL; 
	int *o = NULL;
	int size = 0; 
	int offset = 0;
	
	while (((uint64_t)&ptr-(uint64_t)&heap_begin) < (uint64_t)HEAPSIZE-1) {
		get_header((void*)ptr, s, o);
		size = *s;
		offset = *o;

		if (offset != 0) {
			// found a free block
			printf("Block size: %i, offset %i, free\n", size, offset);
		} 
		else {
			/*
			if ( ((int)&(ptr+size))<(HEAPSIZE-1) || (&freelist!=&(void*)ptr) {
				// block is allocated
				printf("Block size: %i, offset %i, allocated\n", size, offset);
			} else {
				// block is last block in heap and is free
				printf("Block size: %i, offset %i, free\n", size, offset);
			}
			*/
			printf("Block size: %i, offset %i, free/allocated\n", size, offset);
		} 

		ptr+=size;
	}	
}

