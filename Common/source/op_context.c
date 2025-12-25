#include "op_context.h"
#include "memory.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef DEBUG
op_context_t* op_context_acquire_debug(uint32_t flags, const char *file, uint32_t line) {
#else
op_context_t* op_context_acquire(uint32_t flags) {
#endif
    op_context_t *ctx = (op_context_t *)calloc(1, sizeof(op_context_t));

    if (ctx == NULL) {
        log_error(LOG_COMP_OP, "Failed to allocate op_context_t");
        return NULL;
    }

    atomic_init(&ctx->refcount, 1);
    atomic_init(&ctx->version, 0);
    ctx->flags = flags;

    #ifdef DEBUG
    ctx->source_file = file;
    ctx->source_line = line;
    log_trace(LOG_COMP_OP, "op_context_acquire: ctx=%p from %s:%u",
              (void *)ctx, file, line);
    #else
    log_trace(LOG_COMP_OP, "op_context_acquire: ctx=%p", (void *)ctx);
    #endif

    // Reserved fields already zeroed by calloc
    assert(ctx->reserved[0] == NULL);

    return ctx;
}

op_context_t* op_context_retain(op_context_t *ctx) {
    if (ctx == NULL) {
        return NULL;
    }

    uint32_t old_refcount = atomic_fetch_add(&ctx->refcount, 1);

    log_trace(LOG_COMP_OP, "op_context_retain: ctx=%p refcount %u->%u",
              (void *)ctx, old_refcount, old_refcount + 1);

    assert(old_refcount > 0); // Catch use-after-free

    return ctx;
}

void op_context_release(op_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    uint32_t old_refcount = atomic_fetch_sub(&ctx->refcount, 1);

    log_trace(LOG_COMP_OP, "op_context_release: ctx=%p refcount %u->%u",
              (void *)ctx, old_refcount, old_refcount - 1);

    assert(old_refcount > 0); // Catch double-free

    if (old_refcount == 1) {
        // Last reference, free context
        #ifdef DEBUG
        log_trace(LOG_COMP_OP, "op_context_release: freeing ctx=%p (created at %s:%u)",
                  (void *)ctx, ctx->source_file, ctx->source_line);
        #else
        log_trace(LOG_COMP_OP, "op_context_release: freeing ctx=%p", (void *)ctx);
        #endif

        // Validate reserved fields are still NULL (Phase 3)
        for (int i = 0; i < 8; i++) {
            assert(ctx->reserved[i] == NULL);
        }

        free(ctx);
    }
}

uint64_t op_context_version_bump(op_context_t *ctx) {
    assert(ctx != NULL); // Must have context for mutations

    uint64_t new_version = atomic_fetch_add(&ctx->version, 1) + 1;

    log_trace(LOG_COMP_OP, "op_context_version_bump: ctx=%p version=%llu",
              (void *)ctx, (unsigned long long)new_version);

    return new_version;
}

uint64_t op_context_version_get(op_context_t *ctx) {
    assert(ctx != NULL);
    return atomic_load(&ctx->version);
}

bool op_context_validate(op_context_t *ctx) {
    if (ctx == NULL) {
        return false;
    }

    // Refcount must be > 0
    uint32_t refcount = atomic_load(&ctx->refcount);
    if (refcount == 0) {
        log_error(LOG_COMP_OP, "op_context_validate: ctx=%p has refcount=0", (void *)ctx);
        return false;
    }

    // Reserved fields must be NULL in Phase 3
    for (int i = 0; i < 8; i++) {
        if (ctx->reserved[i] != NULL) {
            log_error(LOG_COMP_OP,
                      "op_context_validate: ctx=%p reserved[%d] is non-NULL (not Phase 3 compliant)",
                      (void *)ctx, i);
            return false;
        }
    }

    return true;
}
