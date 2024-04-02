#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "malloc.h"

#ifdef DEBUG
#include <stdio.h>
#include <math.h>
#endif

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
 * Find and return the last block of the heap.
 *
 * @return pointer to the header of the last block of the heap
 */
static BlockHeader *findLastBlock() {
    if (heap.begin == NULL) return NULL;
    BlockHeader *prev = heap.begin;
    BlockHeader *block = BLOCK_NEXT(prev);
    while (BLOCK_IN_RANGE(block)) {
        prev = block;
        block = BLOCK_NEXT(block);
    }
    return prev;
}

/**
 * Increase the heap by HEAP_GROW_FACTOR or initialize it with HEAP_INITIAL_SIZE.
 *
 * @return new size of the heap if it was increased. 0 if increasing failed.
 */
static int increaseHeap() {
    /* initialize heap */
    if (heap.begin == NULL) {
        heap.begin = (BlockHeader*)sbrk(HEAP_INITIAL_SIZE);
        if (heap.begin == (void*)-1) return -1;
        heap.begin->previous = NULL;
        heap.begin->size = HEAP_INITIAL_SIZE - BLOCKHEADER_SIZE;
        heap.end = (void*)((intptr_t)heap.begin + HEAP_INITIAL_SIZE);
        return HEAP_INITIAL_SIZE;
    }

    /* increase heap */
    size_t totalSize = HEAP_SIZE;
    size_t addSize = totalSize * (HEAP_GROW_FACTOR-1);
    BlockHeader *lastBlock = findLastBlock();

    int result = brk(heap.end + addSize);
    if (result == -1) return -1;

    BlockHeader *newBlock = heap.end;
    heap.end += addSize;
    
    if (BLOCK_FREE(lastBlock)) {
        lastBlock->size += addSize;
    }
    else {
        newBlock->size = addSize - BLOCKHEADER_SIZE;
        newBlock->previous = lastBlock;
    }
    return HEAP_SIZE;
}

/**
 * Free the complete heap area.
 */
static void freeHeap() {
    brk(heap.begin);
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
    if (increaseHeap() == -1) return NULL;

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
    size_t oldSize = BLOCK_SIZE(block);

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
    if (block == NULL) return NULL;
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

void *my_calloc(size_t num, size_t size) {
    size_t totalSize = num * size;
    if (totalSize == 0) return NULL;
    BlockHeader *block = getBlock(totalSize);
    if (block == NULL) return NULL;
    uintptr_t *ptr = (uintptr_t*)block->block;
    while (ptr < (uintptr_t*)BLOCK_NEXT(block)) {
        *ptr = 0;
        ptr++;
    }
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

    /* find new block */
    BlockHeader *newBlock = getBlock(size);
    
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

#ifdef REPLACE_ORIGINAL_MALLOC
void *malloc(size_t size) { return my_malloc(size); }
void *calloc(size_t num, size_t size) { return my_calloc(num, size); }
void *realloc(void *ptr, size_t size) { return my_realloc(ptr, size); }
void free(void *ptr) { my_free(ptr); }
#endif

#ifdef DEBUG
/**
 * Print block information for debugging.
 */
void printBlock(BlockHeader *block) {
    printf("╭─ %p ────────────────────╮\n", block);
    if (block->previous != NULL)
        printf("│ previous:            %p │\n", block->previous);
    printf("│ size:                    %10lu │\n", BLOCK_SIZE(block));
    printf("│            %24s │\n", BLOCK_IN_USE(block) ? "in use" : "free");

    char *c = block->block;
    while (c < block->block + BLOCK_SIZE(block)) {
        printf("│ ");
        for (int i=0; i<HEAP_ALIGNMENT/2; i++) {
            printf("%08x ", *c);
            c++;
        }
        printf("│\n");
    }
    printf("╰─────────────────────────────────────╯\n");
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
 * Print all blocks in the heap.
 */
void printAllBlocks() {
    BlockHeader *block = heap.begin;
    while (BLOCK_IN_RANGE(block)) {
        printBlock(block);
        block = BLOCK_NEXT(block);
    }
    printf("\n");
}

/**
 * As found at
 * https://asawicki.info/news_1757_a_metric_for_memory_fragmentation
 */
double fragmentation() {
    uint64_t quality = 0;
    size_t totalFreeSize = 0;
    BlockHeader *block = heap.begin;
    if (block == NULL) return 0;

    for (; BLOCK_IN_RANGE(block); block = BLOCK_NEXT(block)) {
        if (BLOCK_IN_USE(block)) continue;
        size_t size = BLOCK_SIZE(block);
        quality += size * size;
        totalFreeSize += size;
    }
    if (totalFreeSize == 0) return 0;
    double qualityPercent = sqrt((double)quality) / (double)totalFreeSize;
    return 1 - qualityPercent * qualityPercent;
}
#endif
