#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

#define DEBUG

#define HEAP_INITIAL_SIZE 1024*1024
#define HEAP_GROW_FACTOR 2
#define HEAP_ALIGNMENT 8

typedef struct _BlockHeader {
    struct _BlockHeader *previous;
    size_t size;
    char block[];
} BlockHeader;

void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

#ifdef DEBUG
void printHeap();
#endif

#endif
