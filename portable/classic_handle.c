/* classic_handle.c - Classic Mac Handle semantics implementation */
#include "classic_handle.h"
#include <stdlib.h>
#include <string.h>

typedef struct ClassicHeader {
    unsigned char* data;
    size_t size;
    int lock_count;
} ClassicHeader;

static inline ClassicHeader* H2Hdr(Handle h){ return (ClassicHeader*)((char*)h - offsetof(ClassicHeader, data)); }

Handle ClassicNewHandle(size_t initial_size){
    ClassicHeader* hdr = (ClassicHeader*)malloc(sizeof(ClassicHeader));
    if (!hdr) return NULL;
    hdr->size = initial_size;
    hdr->lock_count = 0;
    hdr->data = NULL;
    if (initial_size){
        hdr->data = (unsigned char*)malloc(initial_size);
        if (!hdr->data){ free(hdr); return NULL; }
        memset(hdr->data, 0, initial_size);
    }
    return (Handle)&hdr->data;
}

void ClassicDisposeHandle(Handle h){
    if (!h) return;
    ClassicHeader* hdr = H2Hdr(h);
    if (hdr->data) free(hdr->data);
    free(hdr);
}

void ClassicHLock(Handle h){ if (!h) return; ClassicHeader* hdr = H2Hdr(h); hdr->lock_count++; (void)hdr; }
void ClassicHUnlock(Handle h){ if (!h) return; ClassicHeader* hdr = H2Hdr(h); if (hdr->lock_count>0) hdr->lock_count--; }

size_t ClassicGetHandleSize(Handle h){ if (!h) return 0; ClassicHeader* hdr = H2Hdr(h); return hdr->size; }

int ClassicSetHandleSize(Handle h, size_t new_size){
    if (!h) return 0;
    ClassicHeader* hdr = H2Hdr(h);
    if (new_size == 0){ if (hdr->data){ free(hdr->data); hdr->data=NULL; } hdr->size=0; return 1; }
    unsigned char* nd = (unsigned char*)realloc(hdr->data, new_size);
    if (!nd) return 0;
    if (new_size > hdr->size) memset(nd + hdr->size, 0, new_size - hdr->size);
    hdr->data = nd;
    hdr->size = new_size;
    return 1;
}

/* Helper for duplication */
Handle ClassicDupHandle(Handle h){
    if (!h) return NULL;
    ClassicHeader* hdr = H2Hdr(h);
    Handle nh = ClassicNewHandle(hdr->size);
    if (!nh) return NULL;
    if (hdr->size && hdr->data && *nh){
        memcpy(*nh, hdr->data, hdr->size);
    }
    return nh;
}


