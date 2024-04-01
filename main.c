#include "malloc.h"

int main() {
    void *test = my_malloc(123);
    long *l = my_malloc(sizeof(long));
    char *c = my_malloc(sizeof(char));
    printHeap();
    my_free(test);
    printHeap();
    my_free(l);
    my_free(c);
}
