
# Reimplementation of malloc, realloc and free.

It's done for practice and might not be the best or most efficient or bug
free implementation.

The idea is to to allocate quite a big part of memory and manage user
allocations (done by my_malloc() and my_realloc()) within this area.
A double linked list of blocks with indicators if they are available is
maintained. At the beginning there is only one big free block containing
the complete heap area used. If memory is allocated the list is searched
for the first big enough block and a part of it is used.
If a block is freed it's marked as free and joined with previous and
following block if they are also free.

Here is a little examle how the heap area is organized:

initial state
```
+-------------------------------------------------------+
|                                                       |
+-------------------------------------------------------+
```

```c
void *a = my_alloc(20);
void *b = my_alloc(40);
void *c = my_alloc(30);
```
```
+---+-------+-----+-------------------------------------+
| a |   b   |  c  |                                     |
+---+-------+-----+-------------------------------------+
```

```c
a = my_realloc(40);
```
```
+---+-------+-----+-------+-----------------------------+
|   |   b   |  c  |   a   |                             |
+---+-------+-----+-------+-----------------------------+
```

```c
my_free(b);
```
```
+-----------+-----+-------+-----------------------------+
|           |  c  |   a   |                             |
+-----------+-----+-------+-----------------------------+
```

```c
void *d = my_alloc(30);
```
```
+-----+-----+-----+-------+-----------------------------+
|  d  |     |  c  |   a   |                             |
+-----+-----+-----+-------+-----------------------------+
```

## Usage
Include ``malloc.h`` and use ``malloc()``, ``calloc()``, ``realloc()`` and 
``free()`` as usual and compile with gcc with the ``-fno-builtin-malloc``
flag.

It's work in progress and not made for productive use.

You can comile it as shared library with
```sh
gcc -shared -o libmymalloc.so -fpic 
```
and run any program with
```
LD_PRELOAD=./libmymalloc.so program
```
A lot of real world programs will fail, because it's likely that they also use memory
allocated by other functions than the three implemented here.

Per default simple debug messages will be printed on every call. Turn it off
by using the ``-DNDEBUG`` flag for compilation.


## TODO
- free or decrease heap if it's empty or almost empty (?)

## Bugs
Segmentation fault while starting some programs with the shared library preloaded
(e.g. ``filezilla`` with ``LD_PRELOAD=./libmymalloc.so filezilla``).

