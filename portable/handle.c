/*
 * portable/handle.c - Cross-platform Handle abstraction for Frontier
 */

#include "handle.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct frontier_handle_header {
    pthread_mutex_t mutex;
    size_t size_bytes;
    int lock_count;
    int refcount;
    unsigned char* data; /* separate data buffer to keep handle pointer stable */
};

static frontier_handle_header* alloc_header(size_t size) {
    frontier_handle_header* h = (frontier_handle_header*)malloc(sizeof(frontier_handle_header));
    if (!h) return NULL;
    pthread_mutex_init(&h->mutex, NULL);
    h->size_bytes = size;
    h->lock_count = 0;
    h->refcount = 1;
    if (size > 0) {
        h->data = (unsigned char*)malloc(size);
        if (!h->data) { pthread_mutex_destroy(&h->mutex); free(h); return NULL; }
        memset(h->data, 0, size);
    } else {
        h->data = NULL;
    }
    return h;
}

frontier_handle_t frontier_new_handle(size_t initial_size) {
    return alloc_header(initial_size);
}

bool frontier_dispose_handle(frontier_handle_t h) {
    if (!h) return true;
    pthread_mutex_lock(&h->mutex);
    int rc = --h->refcount;
    pthread_mutex_unlock(&h->mutex);
    if (rc <= 0) {
        if (h->data) free(h->data);
        pthread_mutex_destroy(&h->mutex);
        free(h);
    }
    return true;
}

void* frontier_lock(frontier_handle_t h) {
    if (!h) return NULL;
    pthread_mutex_lock(&h->mutex);
    h->lock_count++;
    void* p = h->data;
    pthread_mutex_unlock(&h->mutex);
    return p;
}

void frontier_unlock(frontier_handle_t h) {
    if (!h) return;
    pthread_mutex_lock(&h->mutex);
    if (h->lock_count > 0) h->lock_count--;
    pthread_mutex_unlock(&h->mutex);
}

bool frontier_set_size(frontier_handle_t h, size_t new_size) {
    if (!h) return false;
    pthread_mutex_lock(&h->mutex);
    size_t old_size = h->size_bytes;
    if (new_size == 0) {
        if (h->data) { free(h->data); h->data = NULL; }
        h->size_bytes = 0;
        pthread_mutex_unlock(&h->mutex);
        return true;
    }
    unsigned char* nd = (unsigned char*)realloc(h->data, new_size);
    if (!nd) { pthread_mutex_unlock(&h->mutex); return false; }
    if (new_size > old_size) {
        memset(nd + old_size, 0, new_size - old_size);
    }
    h->data = nd;
    h->size_bytes = new_size;
    pthread_mutex_unlock(&h->mutex);
    return true;
}

size_t frontier_get_size(frontier_handle_t h) {
    if (!h) return 0;
    pthread_mutex_lock(&h->mutex);
    size_t s = h->size_bytes;
    pthread_mutex_unlock(&h->mutex);
    return s;
}

frontier_handle_t frontier_dup(frontier_handle_t h) {
    if (!h) return NULL;
    pthread_mutex_lock(&h->mutex);
    size_t s = h->size_bytes;
    pthread_mutex_unlock(&h->mutex);
    frontier_handle_t nh = alloc_header(s);
    if (!nh) return NULL;
    if (s && h->data && nh->data) memcpy(nh->data, h->data, s);
    return nh;
}


