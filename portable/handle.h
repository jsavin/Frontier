/*
 * portable/handle.h - Cross-platform Handle abstraction for Frontier
 */

#ifndef FRONTIER_PORTABLE_HANDLE_H
#define FRONTIER_PORTABLE_HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct frontier_handle_header frontier_handle_header;
typedef struct frontier_handle_header* frontier_handle_t; /* equivalent to legacy Handle */

/* API */
frontier_handle_t frontier_new_handle(size_t initial_size);
bool frontier_dispose_handle(frontier_handle_t h);

/* Lock/unlock return a stable pointer to the data region. */
void* frontier_lock(frontier_handle_t h);
void frontier_unlock(frontier_handle_t h);

/* Resizing preserves contents up to min(old,new). Returns false on OOM. */
bool frontier_set_size(frontier_handle_t h, size_t new_size);
size_t frontier_get_size(frontier_handle_t h);

/* Duplicate handle with deep copy. Returns NULL on failure. */
frontier_handle_t frontier_dup(frontier_handle_t h);

/* Legacy helpers parity */
static inline frontier_handle_t ptr_to_handle(void* p) { return (frontier_handle_t)p; }
static inline void* handle_to_ptr(frontier_handle_t h) { return frontier_lock(h); }

#endif /* FRONTIER_PORTABLE_HANDLE_H */


