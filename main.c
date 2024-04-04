//#include "malloc.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
int main() {
    char *c = malloc(sizeof(char));
    uint64_t *l = malloc(sizeof(uint64_t));
    *l = UINT64_MAX;
    uint32_t *f = malloc(sizeof(uint32_t));
    c = realloc(c, 1000000*sizeof(char));
    printHeap();
    printf("---- 1 ----\n");
    free(c);
    printf("---- 2 ----\n");
    free(l);
    void *foo = calloc(1, 32);
    printHeap();
}
