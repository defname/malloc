/**
 * Reimplementation of malloc, realloc and free.
 *
 * It's done for practice and might not be the best or most efficient or bug
 * free implementation.
 *
 * The idea is to to allocate quite a big part of memory and manage user
 * allocations (done by my_malloc() and my_realloc()) within this area.
 * A double linked list of blocks with indicators if they are available is
 * maintained. At the beginning there is only one big free block containing
 * the complete heap area used. If memory is allocated the list is searched
 * for the first big enough block and a part of it is used.
 * If a block is freed it's marked as free and joined with previous and
 * following block if they are also free.
 *
 * Here is a little examle how the heap area is organized:
 *
 * initial state
 * +-------------------------------------------------------+
 * |                                                       |
 * +-------------------------------------------------------+
 *
 * void *a = my_alloc(20);
 * void *b = my_alloc(40);
 * void *c = my_alloc(30);
 * +---+-------+-----+-------------------------------------+
 * | a |   b   |  c  |                                     |
 * +---+-------+-----+-------------------------------------+
 * 
 * a = my_realloc(40);
 * +---+-------+-----+-------+-----------------------------+
 * |   |   b   |  c  |   a   |                             |
 * +---+-------+-----+-------+-----------------------------+
 *
 * my_free(b);
 * +-----------+-----+-------+-----------------------------+
 * |           |  c  |   a   |                             |
 * +-----------+-----+-------+-----------------------------+
 *
 * void *d = my_alloc(30);
 * +-----+-----+-----+-------+-----------------------------+
 * |  d  |     |  c  |   a   |                             |
 * +-----+-----+-----+-------+-----------------------------+
 */
#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>
#include <stdint.h>

#define DEBUG

/* Initial size of the heap, need to be a multiple of HEAP_ALIGNMENT */
#define HEAP_INITIAL_SIZE 1024
/* If the heap size isn't enough it will grow by this factor */
#define HEAP_GROW_FACTOR 2
/* all block sizes will be a multiple of this value */
#define HEAP_ALIGNMENT sizeof(uintptr_t)


/**
 * The size field of the BlockHeader struct is used for the size and to mark
 * a block as in use or free.
 * For this the most significant bit of the size field is used to indicate if
 * a block is free. If it's 0 the block is free, if it's 1 the block is in use.
 */

/* bitmask for most significant bit of uintptr_t */
#define MOST_SIGNIFICANT_BIT_MASK (((uintptr_t)1) << (sizeof(uintptr_t)*8-1))

/* bitmask to get the size of the block */ 
#define SIZE_MASK (UINTPTR_MAX ^ MOST_SIGNIFICANT_BIT_MASK)
#define IN_USE_MASK MOST_SIGNIFICANT_BIT_MASK

/* header size */
#define BLOCKHEADER_SIZE sizeof(BlockHeader)

/* block size (without header) */
#define BLOCK_SIZE(block) ((block)->size & SIZE_MASK)

/* 1 if block in use 0 if block is free */
#define BLOCK_IN_USE(block) (((block)->size & IN_USE_MASK) >> (sizeof(uintptr_t)*8-1))

/* 1 if block is free 0 if block in use */
#define BLOCK_FREE(block) (1-BLOCK_IN_USE(block))

/* pointer to the following block */
#define BLOCK_NEXT(block) ((BlockHeader*)((uintptr_t)(block)+BLOCKHEADER_SIZE+BLOCK_SIZE(block)))

/* check if block is below heap.end */
#define BLOCK_IN_RANGE(block) ((void*)(block) < heap.end)

/* calculate new size and remain the highest bit unchanged */
#define NEW_SIZE(inUse, newSize) (newSize | (inUse * IN_USE_MASK))

/* calculate the next bigger number that is a multiple of HEAP_ALIGNMENT */
#define ALIGN_SIZE(size) ((size) % HEAP_ALIGNMENT == 0 ? (size) : (size) + (HEAP_ALIGNMENT - (size)%HEAP_ALIGNMENT))

/* get a BlockHeader pointer by a pointer to a memory slot */
#define BLOCK_FROM_PTR(ptr) ((BlockHeader*)((uintptr_t)(ptr) - BLOCKHEADER_SIZE))



/**
 * The header used to manage allocated memory.
 */
typedef struct _BlockHeader {
    /* pointer to the previous block's header */ 
    struct _BlockHeader *previous;
    /* Size of the block *and* indicator if the block is in use or free.
     * The most significant bit is used to indicate if the block is
     * available (0) or in use (1).
     * The macros BLOCK_SIZE(), BLOCK_IN_USE() and BLOCK_FREE() can be used
     * avoid manual bit shifting.
     * NEW_SIZE() calculates a size while maintaining the indicator bit.
     */
    size_t size;
    /* Beginning of the data part of the block. This is returned by
     * my_malloc() and my_realloc().
     */
    char block[];
} BlockHeader;

/**
 * Use the following functions exactly as the malloc, realloc and free of the
 * standard library.
 */
void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

#ifdef DEBUG
/**
 * Print block information for debugging.
 *
 * @param block block too print information about
 */
void printBlock(BlockHeader *block);

/**
 * Print the heap for debugging.
 * Indicates if blocks are in use together with their sizes.
 */
void printHeap();

/**
 * Calculate fragmentation of the heap.
 */
double fragmentation();
#endif

#endif
