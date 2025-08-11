/*
 * Core runtime stubs for portable Frontier
 * Basic memory, handles, and string functions
 */

#include "runtime_stubs.h"
#include "standard_portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Basic memory and handle functions
void *newclearhandle(long size) { 
    return malloc(size); 
}

void *newfilledhandle(long size) { 
    void *h = malloc(size); 
    if (h) memset(h, 0, size); 
    return h; 
}

void enlargehandle(void *h, long s) { 
    (void)h; 
    (void)s; 
}

long gethandlesize(void *h) { 
    (void)h; 
    return 0; 
}

void *newheapstring(const char *str) { 
    if (!str) return NULL; 
    long len = strlen(str); 
    void *h = malloc(len + 1); 
    if (h) strcpy((char*)h, str); 
    return h; 
}

void *newtexthandle(const char *str) { 
    return newheapstring(str); 
}

// Hash table functions
void *newhashtable(void) { 
    return malloc(100); 
}

void disposehashtable(void *t) { 
    if (t) free(t); 
}

void chainhashtable(void *t) { 
    (void)t; 
}

void unchainhashtable(void *t) { 
    (void)t; 
}

// Stack operations
boolean pushhandle(void *h) { 
    (void)h; 
    return true; 
}

boolean pushstring(const char *s) { 
    (void)s; 
    return true; 
}

boolean pushint(long v) { 
    (void)v; 
    return true; 
}
