/*
 * Error handling runtime stubs for portable Frontier
 * Language error handling and parameter error functions
 */

#include "runtime_stubs.h"
#include "standard_portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Error handling functions
void langerror(short e) { 
    printf("Language error: %d\n", e); 
}

void langerrorclear(void) { 
    // TODO: Implement error clearing
}

int langerrorenabled(void) { 
    return 1; 
}

void lang3paramerror(short e, const bigstring p1, const bigstring p2, const bigstring p3) { 
    printf("3-param error: %d\n", e); 
    (void)p1; 
    (void)p2; 
    (void)p3; 
}

void langparamerror(short e, const bigstring p) { 
    printf("param error: %d\n", e); 
    (void)p; 
}

void lang2paramerror(short e, const bigstring p1, const bigstring p2) { 
    printf("2-param error: %d\n", e); 
    (void)p1; 
    (void)p2; 
}

void langlongparamerror(short e, long p) { 
    printf("long param error: %d, %ld\n", e, p); 
}

void langostypeparamerror(short e, OSType p) { 
    printf("OSType param error: %d, %08X\n", e, (unsigned int)p); 
}

void langbadexternaloperror(short e, tyvaluerecord p) { 
    printf("bad external op error: %d\n", e); 
    (void)p; 
}

// Basic utility functions
int alllower(const char *str) {
    if (!str) return 0;
    while (*str) {
        if (*str >= 'A' && *str <= 'Z') return 0;
        str++;
    }
    return 1;
}

int isallnumeric(const char *str) {
    if (!str) return 0;
    while (*str) {
        if (*str < '0' || *str > '9') return 0;
        str++;
    }
    return 1;
}
