#include "malloc.h"

int main() {
    void *test = my_malloc(123);
    long *l = my_malloc(sizeof(long));
    char *c = my_malloc(sizeof(char));
    my_free(test);
    my_free(l);
    my_free(c);
}
