#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

#define HEAP_INITIAL_SIZE 1024*1024
#define HEAP_GROW_FACTOR 2
#define HEAP_ALIGNMENT 8

typedef struct _BlockHeader {
    struct _BlockHeader *previous;
    size_t size;
    char block[];
} BlockHeader;

void printHeap();

#endif
