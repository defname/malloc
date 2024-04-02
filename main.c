#include "malloc.h"
#include <stdint.h>

int main() {
    void *test = my_malloc(123);
    uint64_t *l = my_malloc(sizeof(uint64_t));
    *l = UINT64_MAX;
    char *c = my_malloc(sizeof(char));
    printHeap();
    l = my_realloc(l, 100*sizeof(uint64_t));
    printHeap();
    my_free(test);
    my_free(l);
    my_free(c);
}
