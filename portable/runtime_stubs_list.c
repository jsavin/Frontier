/*
 * List operations runtime stubs for portable Frontier
 * List manipulation and array operations
 */

#include "runtime_stubs.h"
#include "standard_portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// List operations
void *listaddvalue(void *list, void *value) {
    (void)list;
    (void)value;
    return NULL;
}

void *listarrayvalue(void *list, long index) {
    (void)list;
    (void)index;
    return NULL;
}

void listassignvalue(void *list, long index, void *value) {
    (void)list;
    (void)index;
    (void)value;
}

int listcomparevalue(void *list1, void *list2) {
    (void)list1;
    (void)list2;
    return 0;
}

void listdeletevalue(void *list, long index) {
    (void)list;
    (void)index;
}

void *listsubtractvalue(void *list, void *value) {
    (void)list;
    (void)value;
    return NULL;
}
