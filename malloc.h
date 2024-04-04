/**
 * Reimplementation of malloc, calloc, realloc and free.
 *
 * It's done for practice and might not be the best or most efficient or bug
 * free implementation.
 *
 * To manage the memory a double linked list of memory blocks is maintained.
 * If malloc() is called the list is searched for a suitable (free) block,
 * it is shrinked to the requested size (if it's possible and reasonable)
 * and separated from the (free) rest of the block.
 * If a block is freed it is marked as free and if possible joined with
 * the previous and the following blocks (only if they are free too of course).
 * If malloc() is called and no suitable block is found, additional memory
 * is requested from the operating system (via sbrk at the moment).
 * If the new memory is located directly after the last block they will be
 * joined otherwise they are not joinable what is easy to check with
 * the double link list.
 * It is always ensured that adjacent blocks are joined if possible, so that
 * there are never two free blocks next to each other.
 */
#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>
#include <stdint.h>

/* if defined malloc, realloc, calloc and free are defined and the original
 * malloc functions can be replaced by compiling with
 *   gcc -fno-builtin-malloc
 */
#define REPLACE_ORIGINAL_MALLOC

/* Initial size of the heap, need to be a multiple of HEAP_ALIGNMENT */
#define HEAP_INITIAL_SIZE 128
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

/* pointer to the end of the block (to the first byte after the block) */
#define BLOCK_END(blck) (void*)((uintptr_t)(blck)->block + BLOCK_SIZE(blck))

/* 1 if block in use 0 if block is free */
#define BLOCK_IN_USE(block) (((block)->size & IN_USE_MASK) >> (sizeof(uintptr_t)*8-1))

/* 1 if block is free 0 if block in use */
#define BLOCK_FREE(block) (1-BLOCK_IN_USE(block))

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
    /* Blocks are organized as a double linked list */ 
    struct _BlockHeader *previous;
    struct _BlockHeader *next;
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
void *my_calloc(size_t num, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

#ifdef REPLACE_ORIGINAL_MALLOC
void *malloc(size_t size);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
#endif


#ifndef NDEBUG
/**
 * Print a message (needs to be exactly 10 character long) together with
 * a pointer address to stdout.
 * Only if NDEBUG is not set.
 */
#define PRINT_PTR(msg, ptr) do { \
    write(STDOUT_FILENO, msg, 10); \
    write(STDOUT_FILENO, "   ", 3); \
    outputPtr(ptr); \
    fsync(STDOUT_FILENO); \
} while (0)
#else
#define PRINT_PTR(msg, ptr)
#endif

#ifndef NDEBUG
/**
 * Print a pointer address to stdout without any allocations.
 *
 * Used for debugging messages with PRINT_PTR() avoiding
 * segmentation faults through infinite recursion.
 */
void outputPtr(void *ptr);

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
 * Print all blocks in the heap.
 */
void printAllBlocks();

/**
 * Calculate fragmentation of the heap.
 */
double fragmentation();
#endif

#endif
