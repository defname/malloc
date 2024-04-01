#include "malloc.h"

int main() {
    void *test = my_malloc(123);
    long *l = my_malloc(sizeof(long));
    char *c = my_malloc(sizeof(char));
    printHeap();
    test = my_realloc(test, 126);
    printHeap();
    test = my_realloc(test, 200);
    printHeap();
    my_free(test);
    my_free(l);
    my_free(c);
}
