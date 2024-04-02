#include "malloc.h"
#include <stdint.h>
#include <stdio.h>
int main() {
    printf("FPPPP\n");

    return 0;
    char *c = malloc(sizeof(char));
    uint64_t *l = malloc(sizeof(uint64_t));
    *l = UINT64_MAX;
    uint32_t *f = malloc(sizeof(uint32_t));
    printHeap();
    c = realloc(c, 1000000*sizeof(char));
    printHeap();
    free(c);
    free(l);
    void *foo = calloc(1, 32);
}
