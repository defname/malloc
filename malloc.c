#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "malloc.h"

#ifndef NDEBUG
#include <stdio.h>
#include <math.h>
#endif

BlockHeader *heap = NULL;

/**
 * Get the last block.
 * 
 * @return pointer to the BlockHeader of the last block
 *         or NULL if the heap is uninitialized
 */
static BlockHeader *getLastBlock() {
    if (heap == NULL) return NULL;
    BlockHeader *block = heap;
    while (block->next != NULL) {
        block = block->next;
    }
    return block;
}

/**
 * Increase the heap by at least minSize bytes. minSize needs to be a multiple
 * of HEAP_ALIGNMENT.
 * 
 * @param minSize minimum number of bytes that should be available after the
 *                call. minSize has to be a multiple of HEAP_ALIGNMENT.
 *                Use ALIGN_SIZE(size) before.
 * @return number of bytes available in the last block or -1 if allocation was
 *         unsuccessful
 */
static BlockHeader* increaseHeap(size_t minSize) {
    /* if heap is uninitialized initialize it */
    if (heap == NULL) {
        size_t size = minSize + BLOCKHEADER_SIZE;
        size = size > HEAP_INITIAL_SIZE ? size * 2 : HEAP_INITIAL_SIZE;
        heap = sbrk(HEAP_INITIAL_SIZE);
        if ((void*)heap == (void*)-1) {
            return NULL;
        }
        heap->size = size - BLOCKHEADER_SIZE;
        /* initialize double linked list */
        heap->previous = NULL;
        heap->next = NULL;

        return heap;
    }

    BlockHeader *lastBlock = getLastBlock();
    /* lastBlock != NULL because heap != NULL */

    BlockHeader *newBlock = sbrk(minSize + BLOCKHEADER_SIZE);
    if ((void*)newBlock == (void*)-1) return NULL;

    /* if the last block is free and the new allocated memory is next to it
     * increase last block
     */
    if (BLOCK_END(lastBlock) == (void*)newBlock && BLOCK_FREE(lastBlock)) {
        lastBlock->size += minSize + BLOCKHEADER_SIZE;
        return lastBlock;
    }

    /* insert an empty block */
    newBlock->size = minSize;
    lastBlock->next = newBlock;
    newBlock->previous = lastBlock;
    newBlock->next = NULL;
    return newBlock;
}

/**
 * Find a free block with at least minSize bytes of memory.
 *
 * @param minSize number of bytes needed
 * @return pointer to the header of the found block or NULL
 *         if the search was unsuccessfull.
 */
static BlockHeader *findFreeBlock(size_t minSize) {
    if (heap == NULL) return NULL;
    BlockHeader *block = heap;

    while (block != NULL) {
        if (BLOCK_FREE(block) && BLOCK_SIZE(block) >= minSize) {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

/**
 * Join block with next block if possible.
 *
 * @param block pointer to the header of the block to join with its descendant
 * @return new size of the block
 */
static size_t joinBlockWithFollower(BlockHeader *block) {
    BlockHeader *next = block->next;
    if (next == NULL
        || BLOCK_IN_USE(next)
        || BLOCK_END(block) != (void*)next) {
        return BLOCK_SIZE(block);
    }
    block->size += BLOCKHEADER_SIZE + BLOCK_SIZE(next);
    
    /* update linked list */
    block->next = next->next;
    if (block->next != NULL) block->next->previous = block;

    return BLOCK_SIZE(block);
}

/**
 * @return pointer to the header of the enlarged or found block
 */
static size_t resizeBlock(BlockHeader *block, size_t minSize) {
    size_t alignedSize = ALIGN_SIZE(minSize);

    /* join block with it's follower */
    size_t enlargedBlockSize = joinBlockWithFollower(block);
    /* if the enlarged block is not big enough there is nothing we can do
     * here
     */
    if (enlargedBlockSize < alignedSize) {
        return 0;
    }

    /*  Now try to shrink the block again to alignedSize.
     *
     *  HEAP_ALIGNMENT is the minimum size a block can contain.
     *  +-------------------------------------------+
     *  |                 block size                |
     *  +------------------------+--------+---------+
     *  | alignedSize            | HEADER | minSize |
     *  +------------------------+--------+---------+
     */
    if (enlargedBlockSize < alignedSize + BLOCKHEADER_SIZE + HEAP_ALIGNMENT) {
        /* So if orig size is to small to contain an additional
         * empty block with at least HEAP_ALIGNMENT size there
         * is no point in shrinking it in the first place.
         */
        return enlargedBlockSize;
    }


    block->size = NEW_SIZE(BLOCK_IN_USE(block), alignedSize);

    BlockHeader *newBlock = BLOCK_END(block);
    /* we ensured that newBlock->size will be at least HEAP_ALIGNMENT */
    newBlock->size = enlargedBlockSize - BLOCKHEADER_SIZE - alignedSize;

    /* update double linked list */
    newBlock->previous = block;
    newBlock->next = block->next;
    block->next = newBlock;
    if (newBlock->next != NULL) newBlock->next->previous = newBlock;

    return BLOCK_SIZE(block);
}


/**
 * Find free space and create a block of minSize.
 * The block size might be bigger depending on whats available if it
 * can be shrinked.
 *
 * @param minSize the minimum amount of bytes the block should provide
 * @return pointer to the header of the block already marked as in-use
 */
static BlockHeader *getBlock(size_t minSize) {
    size_t alignedSize = ALIGN_SIZE(minSize);

    /* try to find a free block */
    BlockHeader *block = findFreeBlock(alignedSize);
    /* if no big enough free block is found increase the heap */
    if (block == NULL)
        block = increaseHeap(alignedSize);
    /* if this fails there is nothing we can do */
    if (block == NULL) return NULL;

    resizeBlock(block, alignedSize);
    
    block->size |= IN_USE_MASK;
    return block;
}

/**
 * Mark block as free and join it with adjacent free blocks.
 *
 * @param block block to be freed
 */
void freeBlock(BlockHeader *block) {
    block->size &= SIZE_MASK;
    joinBlockWithFollower(block);
    if (block->previous == NULL) {
        heap = block;
    }
    else if (BLOCK_FREE(block->previous)) {
        joinBlockWithFollower(block->previous);
    }
}


/**
 * The following functions should behave exactly like their official versions
 */

void *my_malloc(size_t size) {
    if (size == 0) return NULL;

    BlockHeader *block = getBlock(size);
    if (block == NULL) return NULL;

    PRINT_PTR("malloc    ", block->block);

    return block->block;
}

void *my_calloc(size_t num, size_t size) {
    size_t totalSize = num * size;
    if (totalSize == 0) return NULL;

    BlockHeader *block = getBlock(totalSize);
    if (block == NULL) return NULL;

    for (uintptr_t *ptr = (uintptr_t*)block->block; (void*)ptr < BLOCK_END(block); ptr++) {
        *ptr = 0;
    }
    
    PRINT_PTR("calloc    ", block->block);

    return block->block;
}

void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return my_malloc(size);

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    BlockHeader *block = BLOCK_FROM_PTR(ptr);

    /* try to resize block */
    if (resizeBlock(block, size) > 0) { /* if successful return return it */
        PRINT_PTR("realloc(r)", block->block);

        return block->block;
    }

    /* find another block and copy the data */
    BlockHeader *newBlock = getBlock(size);
    if (newBlock == NULL) {
        /* should ptr be freed here?? */
        return NULL;
    }
    memcpy(newBlock->block, block->block, BLOCK_SIZE(block));

    PRINT_PTR("realloc(m)", block->block);

    /* free old block */
    freeBlock(block);

    return newBlock->block;
}

void my_free(void *ptr) {
    if (ptr == NULL) return;
    PRINT_PTR("free      ", ptr);
    freeBlock(BLOCK_FROM_PTR(ptr));
}


#ifdef REPLACE_ORIGINAL_MALLOC
void *malloc(size_t size) { return my_malloc(size); }
void *calloc(size_t num, size_t size) { return my_calloc(num, size); }
void *realloc(void *ptr, size_t size) { return my_realloc(ptr, size); }
void free(void *ptr) { my_free(ptr); }
#endif


#ifndef NDEBUG
/**
 * Print pointer adress to stdout without using any allocation.
 */
void outputPtr(void *ptr) {
    uintptr_t n = (uintptr_t)ptr;
    size_t digitsCount = sizeof(uintptr_t) * 2;
    uint8_t digits[digitsCount];
    for (int i=digitsCount-1; i>=0; i--) {
        digits[i] = n % 16;
        n /= 16;
    }
    write(STDOUT_FILENO, "0x", 2);
    for (int i=0; i< digitsCount; i++) {
        char c = digits[i] < 10 ? '0' + digits[i] : 'A' + (digits[i]-10);
        write(STDOUT_FILENO, &c, 1);
    }
    write(STDOUT_FILENO, "\n", 1);
}

static void printfPtr(void *ptr) {
//#define NULL_STRING "0x000000000000"
#define NULL_STRING "          NULL"
    if (ptr == NULL) printf(NULL_STRING);
    else printf("%p", ptr);
#undef NULL_STRING
}

/**
 * Print block information for debugging.
 */
void printBlock(BlockHeader *block) {
    printf("╭─ %p ────────────────────╮\n", block);
    printf("│ previous:     ");
    printfPtr(block->previous);
    printf(" │\n");
    printf("│ next:     ");
    printfPtr(block->next);
    printf(" │\n");
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
    BlockHeader *block = heap;

    if (block == NULL) return;

    size_t totalSize = 0;

    printf("╔══════════ Heap ══════════╗\n");
    while (block != NULL) {
        if (block->previous != NULL && BLOCK_END(block->previous) != (void*)block) {
            printf("╠══════════════════════════╣\n");
        }
        printf("╟───── %p ─────╢\n", block);
        printf("║ previous: ");
        printfPtr(block->previous);
        printf(" ║\n");
        printf("║ next:     ");
        printfPtr(block->next);
        printf(" ║\n");
        printf("║ %s             %10lu ║\n", BLOCK_IN_USE(block) ? "#" : " ", BLOCK_SIZE(block));

        totalSize += BLOCK_SIZE(block);
        block = block->next;
    }
    printf("╠══════════════════════════╣\n");
    printf("║ total size:   %10lu ║\n", totalSize);
    printf("║ fragmentation:     %.3f ║\n", fragmentation());
    printf("╚══════════════════════════╝\n");
}

/**
 * Print all blocks in the heap.
 */
void printAllBlocks() {
    BlockHeader *block = heap;
    while (block != NULL) {
        printBlock(block);
        block = block->next;
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
    BlockHeader *block = heap;
    if (block == NULL) return 0;

    for (; block != NULL; block = block->next) {
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

