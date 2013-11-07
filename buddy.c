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

static int is_buddy(void *block1, void *block2) {
	int heap_add = (uint64_t)(heap_begin);
	int b1 = (uint64_t)(block1)-heap_add;
	int b2 = (uint64_t)(block2)-heap_add;
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

static int get_distance(void *block1, void *block2) {
	uint8_t *ptr1 = (uint8_t*)block1;
	uint8_t *ptr2 = (uint8_t*)block2;
	int distance = 0;

	while (ptr1 != ptr2) {
		int size, offset;
		get_header(ptr1, &size, &offset);
		
		distance += size;
		ptr1 += size;
	}
	
	return distance;
}

static void malloc_helper(uint8_t *ptr, int request_size) {
	int size, offset;
	get_header(ptr, &size, &offset);

	if ((size/2) < request_size) {
		// can't divide further; mark as allocated and return
		mod_header(ptr, size, 0);
		return;
	}
	
	size /= 2;
	mod_header(ptr, size, size);
	uint8_t *ptr2 = ptr + size;
	if (offset == 0) {
		mod_header(ptr2, size, 0);
	} else {
		mod_header(ptr2, size, (offset-size));
	}

	malloc_helper(ptr, request_size);

	return;
}

static void free_helper(uint8_t *ptr) {
	int size, offset;
	get_header(ptr, &size, &offset);

	// find buddy
	uint8_t *bptr = NULL;
	if (size == HEAPSIZE) {
		// there is no spoon... I mean... buddy
		return;
	} else if (is_buddy(ptr, ptr-size)) {
		// buddy is to the left
		bptr = ptr-size;
	} else {
		// buddy is to the right
		bptr = ptr+size;
	}

	// merge buddies if possible
	int bsize, boffset;
	get_header(bptr, &bsize, &boffset);

	if (bsize==size && is_buddy(ptr, ptr-size) && boffset!=0) {
		// buddy is to the left and is free
		mod_header(bptr, size*2, boffset+offset);
		free_helper(bptr);
	} else if (bsize==size && offset==size) {
		// buddy is to the right and is free
		if (boffset==0) {
			mod_header(ptr, size*2, 0);
		} else {
			mod_header(ptr, size*2, offset+boffset);
		}
		free_helper(ptr);
	}
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
	int size, offset;

	while(1) {
		get_header(ptr, &size, &offset);

		if (size < request_size) {
			// block is too small
			if (offset == 0) {
				// no more blocks; return a null pointer
				ptr = NULL;
				return ptr;
			} else {
				// go to next free block
				ptr += offset;
				prevoffset = offset;
			}
		} else {
			// we've got our block
			break;
		}
	}

	// recursively divide the block to reduce fragmentation
	malloc_helper(ptr, request_size);

	// update freelist
	get_header(ptr, &size, &offset);
	if ((uint64_t)freelist == (uint64_t)ptr) {
		if ((uint64_t)ptr-(uint64_t)heap_begin+size < (uint64_t)HEAPSIZE-1) {
			// not last block in heap and not end of freelist
			uint8_t *freeptr = (uint8_t*)freelist;
			freeptr += size;
			freelist = (void*)freeptr;
		} else {
			freelist = NULL;
		}
	} else {
		uint8_t *ptr2 = ptr - prevoffset;
		if ((uint64_t)ptr-(uint64_t)heap_begin+size < (uint64_t)HEAPSIZE-1) {
			// not last block in heap
			mod_header(ptr2, -1, prevoffset+size);
		} else {
			mod_header(ptr2, -1, 0);
		}
	}

	// return user space of block
	ptr += 8;
	return ptr;
	
}

void free(void *block) {
	int size, offset;
	uint8_t *ptr = (uint8_t*)block;

	if (block == NULL) {
		return;
	}

	ptr -= 8;
	get_header(ptr, &size, &offset);
	
	// free the current block by updating freelist
	if (freelist == NULL) {
		// ptr is the only free block
		mod_header(ptr, size, 0);
		freelist = (void*)ptr;
	}	
	else if ((uint64_t)freelist > (uint64_t)ptr) {
		// ptr is the new head of freelist
		mod_header(ptr, size, get_distance(ptr,freelist));
		freelist = (void*)ptr;
	} 
	else {
		// block is in middle or end of freelist
		uint8_t *ptr2 = (uint8_t*)freelist;
		int size2, offset2;
		get_header(ptr2, &size2, &offset2);
		while ((ptr2+offset2) < ptr) {
			if (offset2 == 0) {
				break;
			} else {
				ptr2+=offset2;
				get_header(ptr2, &size2, &offset2);
			}
		}
		
		int distance = get_distance(ptr2, ptr);
		mod_header(ptr2, size2, distance);
		if ((uint64_t)ptr-(uint64_t)heap_begin+size >= (uint64_t)HEAPSIZE-1) {
			// block is last block in heap and freelist
			mod_header(ptr, size, 0);
		} else {
			mod_header(ptr, size, offset2-distance);
		}
	}
	
	// recursively merge buddies if possible
	free_helper(ptr);
}

void dump_memory_map(void) {
	uint8_t *ptr = (uint8_t*)heap_begin;
	int size, offset;
	int prevfree = -1;
	
	while ( ((uint64_t)ptr-(uint64_t)heap_begin) < ((uint64_t)HEAPSIZE-1) ) {
		get_header(ptr, &size, &offset);

		if (offset != 0) {
			// found a free block
			printf("Block size: %i, offset %i, free\n", size, offset);
			prevfree = offset;
		} 
		else {
			// block could either be allocated or free
			int size2, offset2;
			if ((uint64_t)ptr-(uint64_t)prevfree >= (uint64_t)heap_begin) {
				get_header(ptr-prevfree, &size2, &offset2);
				if (offset2 == prevfree || (uint64_t)freelist == (uint64_t)ptr) {
					// block is free
					printf("Block size: %i, offset %i, free\n", size, offset);
				} else {
					printf("Block size: %i, offset %i, allocated\n", size, offset);
				}
			} 
			else {
				printf("Block size: %i, offset %i, allocated\n", size, offset);
			}
		} 

		ptr+=size;
	}	
	printf("\n");
}

