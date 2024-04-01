#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "malloc.h"

#ifdef DEBUG
#include <stdio.h>
#include <math.h>
#endif


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
 * struct to remember the begin and the end of the total heap area available.
 */
typedef struct {
    BlockHeader *begin;
    void *end;
} Heap;

/* global object to remember the heap area */
Heap heap = {NULL, NULL};

/**
 * Increase the heap by HEAP_GROW_FACTOR or initialize it with HEAP_INITIAL_SIZE.
 */
static void increaseHeap() {
    /* initialize heap */
    if (heap.begin == NULL) {
        heap.begin = malloc(HEAP_INITIAL_SIZE);
        if (heap.begin == NULL) exit(1);
        heap.begin->previous = NULL;
        heap.begin->size = HEAP_INITIAL_SIZE - BLOCKHEADER_SIZE;
        heap.end = (void*)((intptr_t)heap.begin + HEAP_INITIAL_SIZE);
        return;
    }

    /* increase heap */
    size_t oldHeapSize = (uintptr_t)heap.end - (uintptr_t)heap.begin;
    size_t newHeapSize = oldHeapSize * HEAP_GROW_FACTOR;
    BlockHeader *newBegin = realloc(heap.begin, newHeapSize);
    if (newBegin == NULL) exit(1);

    /* update header */
    BlockHeader *prev = newBegin;
    BlockHeader *current = BLOCK_NEXT(prev);
    while ((void*)current < (void*)newBegin + oldHeapSize) {
        current->previous = prev;
        prev = current;
        current = BLOCK_NEXT(current);
    }
    heap.begin = newBegin;
    heap.end = (void*)((intptr_t)heap.begin + newHeapSize);

    /* update free space */
    if (BLOCK_IN_USE(prev)) { /* add new empty block if the last block is used */
        current->previous = prev;
        current->size = (uintptr_t)heap.end - (uintptr_t)current->block;
        current->size |= IN_USE_MASK;
    }
    else { /* update size of last block if it's free */
        prev->size = (uintptr_t)heap.end - (uintptr_t)prev->block;
    }
}

/**
 * Free the complete heap area.
 */
static void freeHeap() {
    free(heap.begin);
    heap.begin = NULL;
    heap.end = NULL;
}

/**
 * Find a free block with at least size available bytes.
 *
 * @param size the minimum size the block needs to have
 * @return pointer to the found block.
 */
static BlockHeader *findFreeBlock(size_t size) {
    BlockHeader *block = heap.begin;
    while (BLOCK_IN_RANGE(block)) {
        if (BLOCK_FREE(block) && BLOCK_SIZE(block) >= size) {
            return block;
        }
        block = BLOCK_NEXT(block);
    }
    increaseHeap();
    return findFreeBlock(size);
}

/**
 * Join block with the following block if it's possible.
 *
 * @param block block to join with it's descendant
 * @return new size of block
 */
static size_t joinBlockWithFollower(BlockHeader *block) {
    BlockHeader *next = BLOCK_NEXT(block);
    if (!BLOCK_IN_RANGE(next) || BLOCK_IN_USE(next)) {
        return BLOCK_SIZE(block);
    }
    block->size += BLOCKHEADER_SIZE + BLOCK_SIZE(next);
    /* update followers back reference */
    next = BLOCK_NEXT(block);
    if (BLOCK_IN_RANGE(next)) next->previous = block;

    return BLOCK_SIZE(block);
}

/**
 * Shrink size of block to size.
 * aligned_size need to be smaller or equal to the original size of block
 * and it needs to be a multiple of HEAP_ALIGNMENT.
 *
 * @param block block to shrink
 * @param alignedSize new size which is a multiple of HEAP_ALIGNMENT and fits
 *        into block.
 * @return new size of bigEnoughBlock
 */
static size_t shrinkBlock(BlockHeader *block, size_t alignedSize) {
    size_t oldSize = BLOCK_SIZE (block);

    /* if there is no space to add an additional header don't shrink */
    if (alignedSize + BLOCKHEADER_SIZE >= oldSize) return BLOCK_SIZE(block);

    /* resize block */
    block->size = NEW_SIZE(BLOCK_IN_USE(block), alignedSize);
    /* add an empty block behind */
    BlockHeader *newEmptyBlock = BLOCK_NEXT(block);
    newEmptyBlock->size = oldSize - BLOCKHEADER_SIZE - alignedSize;
    newEmptyBlock->previous = block;
    
    return BLOCK_SIZE(block);
}

/**
 * Get a block that fits size and mark it as used.
 *
 * @param size the requested size.
 * @return a pointer to the Block with at least size bytes available
 */
static BlockHeader *getBlock(size_t size) {
    size_t alignedSize = ALIGN_SIZE(size);
    BlockHeader *block = findFreeBlock(alignedSize);
    shrinkBlock(block, alignedSize);
    block->size |= IN_USE_MASK;
    return block;
}

static void freeBlock(BlockHeader *block) {
    /* mark as free */
    block->size &= SIZE_MASK;

    /* join following block */
    joinBlockWithFollower(block);

    /* join previous block */
    BlockHeader *previous = block->previous;
    if (previous != NULL && BLOCK_FREE(previous)) {
        block->previous->size += BLOCKHEADER_SIZE + BLOCK_SIZE(block);
    }
}

/**
 * Resize a non-free block to size if possible. Return NULL if it is not.
 * 
 * @param block non-free block to resize
 * @param size new size of block
 */
static BlockHeader *resizeBlock(BlockHeader *block, size_t size) {
    size_t oldSize = BLOCK_SIZE(block);
    size_t alignedSize = ALIGN_SIZE(size);
    if (alignedSize == oldSize) {
        return block;
    }

    /* Increase block size if possible.
     * This is also useful if the new block size is lower than the old one,
     * since so is ensured that there is space for the new header if available.
     * */
    size_t newBlockSize = joinBlockWithFollower(block);
    
    /* decrease size */
    if (alignedSize < newBlockSize) {
        shrinkBlock(block, alignedSize);
        return block;
    }

    /* shrink block to original size */
    shrinkBlock(block, oldSize);

    return NULL;
}

void *my_malloc(size_t size) {
    BlockHeader *block = getBlock(size);
    if (block == NULL) return NULL;
    return block->block;
}

void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return my_malloc(size);
    }

    BlockHeader *block = BLOCK_FROM_PTR(ptr);
    size_t oldSize = BLOCK_SIZE(block);

    /* try to resize the block */
    if (resizeBlock(block, size) != NULL) return block->block;

    /* remember offset of the block relative to heap.begin, to restore
     * the pointer in the case the heap is moved by realloc
     * during getBlock
     */
    uintptr_t blockOffset = (uintptr_t)block - (uintptr_t)heap.begin;
    /* find new block */
    BlockHeader *newBlock = getBlock(size);
    /* restore block pointer */
    block = (BlockHeader*)(uintptr_t)heap.begin + blockOffset;
    
    if (newBlock == NULL) return NULL;

    /* copy old block to new one */
    memcpy((void*)newBlock->block, block->block, oldSize);
    /* free old block */
    freeBlock(block);
    return newBlock->block;
}

void my_free(void *ptr) {
    if (ptr == NULL) return;
    BlockHeader *block = BLOCK_FROM_PTR(ptr);
    freeBlock(block);
}

#ifdef DEBUG
/**
 * Print block information for debugging.
 */
void printBlock(BlockHeader *block) {
    printf("╭───── %p ─────╮\n", block);
    if (block->previous != NULL)
        printf("│ previous: %p │\n", block->previous);
    printf("│ size:         %10lu │\n", BLOCK_SIZE(block));
    printf("│ %24s │\n", BLOCK_IN_USE(block) ? "in use" : "free");
    printf("╰──────────────────────────╯\n");
}

/**
 * Print heap for debugging.
 * Indicates if blocks are in used or free together with its size.
 */
void printHeap() {
    BlockHeader *block = heap.begin;

    if (block == NULL) return;

    printf("╔══════════ Heap ══════════╗\n");
    printf("║ begin:    %p ║\n", heap.begin);
    printf("║ end:      %p ║\n", heap.end);
    while (BLOCK_IN_RANGE(block)) {
        printf("╟───── %p ─────╢\n", block);
        printf("║ %s             %10lu ║\n", BLOCK_IN_USE(block) ? "#" : " ", BLOCK_SIZE(block));

        block = BLOCK_NEXT(block);
    }
        printf("╟───── %p ─────╢\n", block);
    printf("║ total size:   %10lu ║\n", (uintptr_t)heap.end - (uintptr_t)heap.begin);
    printf("║ fragmentation:     %.3f ║\n", fragmentation());
    printf("╚══════════════════════════╝\n");
}

/**
 * As found at
 * https://asawicki.info/news_1757_a_metric_for_memory_fragmentation
 */
double fragmentation() {
    int quality = 0;
    size_t totalFreeSize = 0;
    BlockHeader *block = heap.begin;
    if (block == NULL) return 0;

    for (; BLOCK_IN_RANGE(block); block = BLOCK_NEXT(block)) {
        if (BLOCK_IN_USE(block)) continue;
        size_t size = BLOCK_SIZE(block);
        quality += size * size;
        totalFreeSize += size;
    }
    double qualityPercent = sqrt((double)quality) / totalFreeSize;
    return 1 - qualityPercent * qualityPercent;
}
#endif
