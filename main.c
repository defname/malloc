#include "malloc.h"
#include <stdint.h>

int main() {
    char *c = my_malloc(sizeof(char));
    uint64_t *l = my_malloc(sizeof(uint64_t));
    *l = UINT64_MAX;
    uint32_t *f = my_malloc(sizeof(uint32_t));
    printAllBlocks();
    my_free(c);
    my_free(l);
    printAllBlocks();
    void *foo = my_calloc(1, 32);
    printAllBlocks();
}
