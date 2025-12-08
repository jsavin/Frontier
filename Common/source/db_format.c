/* 2025-11-24 Codex: Clamp header comparisons with uint64 for BE safety. */
/* 2025-11-25 Codex: Implement legacy adapter widening + strict v7 reader entry points. */
/* 2025-11-26 Codex: Shift version-specific readers/writers into dedicated modules. */
/* 2025-11-27 Codex: Add migration breadcrumbs to chase drop-Cancoon failures. */
/* 2025-11-27 Codex: During migration, drop externals whose addresses point at free blocks. */
/* 2025-11-29 Codex: Thread migrator reads/writes through explicit db_contexts to avoid global Save As swaps. */
/* 2025-11-30 Codex: Scope db_context operations with Save As state guards for thread safety. */

#include "frontier.h"
#include "standard.h"
#include "shell_api.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

#include "db_format.h"
#include "dbinternal.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "tableinternal.h"
#include "threads.h"
#include "tableverbs.h"
#include "cancoon.h"
#include "cancooninternal.h"
#include "file.h"

/* Internal DB helpers used for context-wrapped operations. */
extern boolean dbassign_internal(dbaddress *padr, long newsize, ptrvoid pdata);
extern boolean dbcopy_internal(dbaddress adrorig, dbaddress *adrcopy);
extern boolean dbgetsize_internal(dbaddress adr, long *logicalsize);

/* Headless verb initialization functions */
#ifdef FRONTIER_HEADLESS
extern boolean headless_init_kernel_verbs(void);  /* Auto-generated from kernelverbs.rc */
#endif

// 2025-10-27 Codex: Added optional migration tracing to inspect v6/v7 table layouts during conversion.
// 2025-11-20 Codex: Added v7 header serializer and shared big-endian helpers to keep modern roots portable.
// 2025-11-25 Codex: Implement legacy adapter widening + strict v7 reader entry points.
// 2025-11-30 Codex: Guard headless runtime tracking when portable builds skip UI hooks.

static _Thread_local boolean g_db_format_runtime_initialized = false;
#if !defined(FRONTIER_PORTABLE)
static boolean g_db_format_runtime_headless = false;
#endif
static char last_backup_path[1024];
static boolean g_legacy_adapter_active = false;
static boolean g_legacy_adapter_force_repack = false;
static tydatabaserecord_64 g_legacy_widened_header;
static hdldatabaserecord g_legacy_source_db = nil;
static _Thread_local db_format_mode g_mode_stack[4];
static _Thread_local int g_mode_depth = 0;
static _Thread_local db_format_mode g_mode_state = {false, false, false}; /* current mode when stack is empty */

typedef struct db_context_guard {
    db_format_mode prev_mode;
    db_saveas_state prev_saveas;
    hdldatabaserecord prev_db;
} db_context_guard;

static void db_context_guard_enter(const db_context *context, db_context_guard *guard) {
    if (guard != NULL) {
        guard->prev_mode = db_format_mode_current();
        db_saveas_state_snapshot(&guard->prev_saveas);
        guard->prev_db = databasedata;
    }
    if (context != NULL) {
        if (context->database != nil)
            databasedata = context->database;
        db_saveas_state_apply(&context->saveas);
        g_mode_depth = 0; /* reset stacked overrides before applying explicit context */
        db_format_mode_apply(&context->mode);
    }
}

static void db_context_guard_exit(const db_context_guard *guard) {
    if (guard == NULL)
        return;
    db_format_mode_apply(&guard->prev_mode);
    databasedata = guard->prev_db;
    db_saveas_state_apply(&guard->prev_saveas);
}

#if defined(_WIN32)
#define db_trace_seek _fseeki64
typedef __int64 db_trace_off_t;
#else
#define db_trace_seek fseeko
typedef off_t db_trace_off_t;
#endif

#define DB_TRACE_ENV_LEVEL "FRONTIER_DB_TRACE_LEVEL"
#define DB_TRACE_ENV_DEPTH "FRONTIER_DB_TRACE_DEPTH"
#define DB_TRACE_MAX_VISITED 1024
#define DB_TRACE_MAX_PAYLOAD (8 * 1024 * 1024UL)
#define DB_TRACE_PATH_MAX 512
#define DB_TRACE_EXTERNAL_TABLE_ID 3  /* tyexternalid order: outline, wp, head, table */

typedef struct db_trace_context db_trace_context;

/* Forward declarations for helper routines used by the tracing instrumentation */
static uint16_t read_be16(const void *ptr);
static uint32_t read_legacy_u32(const unsigned char *field);
static uint64_t read_be64(const unsigned char *field);
static void db_trace_walk_table(db_trace_context *ctx, dbaddress adr, const char *path, int depth);

boolean db_format_prepare_runtime(void) {
    if (g_db_format_runtime_initialized)
        return true;

#if !defined(FRONTIER_PORTABLE)
    if (!g_db_format_runtime_headless) {
        shell_api_use_headless();
        g_db_format_runtime_headless = true;
    }
#endif

    if (!initmemory())
        return false;

    initstrings();

    if (!initlang())
        return false;

    if (!inittablestructure())
        return false;

    if (!langinitverbs())
        return false;

#ifdef FRONTIER_HEADLESS
    /* Initialize all headless verb processors (auto-generated) */
    if (!headless_init_kernel_verbs())
        return false;
#endif

    grabthreadglobals();

    g_db_format_runtime_initialized = true;
    return true;
}

struct db_trace_context {
    FILE *file;
    const char *path_label;
    int max_depth;
    int level;
    size_t max_entries;
    dbaddress visited[DB_TRACE_MAX_VISITED];
    size_t visited_count;
};

static int db_trace_level_cache = -1;
static int db_trace_depth_cache = -1;

static int db_trace_level(void) {
    if (db_trace_level_cache >= 0)
        return db_trace_level_cache;

    const char *env = getenv(DB_TRACE_ENV_LEVEL);
    if (env == NULL || *env == '\0') {
        db_trace_level_cache = 0;
        return db_trace_level_cache;
    }

    int parsed = (int) strtol(env, NULL, 10);
    if (parsed < 0)
        parsed = 0;
    db_trace_level_cache = parsed;
    return db_trace_level_cache;
}

static int db_trace_depth_limit(void) {
    if (db_trace_depth_cache >= 0)
        return db_trace_depth_cache;

    const char *env = getenv(DB_TRACE_ENV_DEPTH);
    if (env == NULL || *env == '\0') {
        db_trace_depth_cache = 1; /* default to root + immediate children */
        return db_trace_depth_cache;
    }

    int parsed = (int) strtol(env, NULL, 10);
    if (parsed < 0)
        parsed = 0;
    db_trace_depth_cache = parsed;
    return db_trace_depth_cache;
}

static void db_trace_log(int level, const char *fmt, ...) {
    if (db_trace_level() < level)
        return;

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[db-trace] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static size_t db_trace_entry_limit(int level) {
    return (level >= 2) ? (size_t) SIZE_MAX : (size_t) 32;
}

static boolean db_trace_add_visited(db_trace_context *ctx, dbaddress adr) {
    for (size_t i = 0; i < ctx->visited_count; ++i) {
        if (ctx->visited[i] == adr)
            return false;
    }

    if (ctx->visited_count >= DB_TRACE_MAX_VISITED) {
        db_trace_log(1, "%s: visited-table overflow, skipping addr 0x%08llx",
                     ctx->path_label,
                     (unsigned long long) adr);
        return false;
    }

    ctx->visited[ctx->visited_count++] = adr;
    return true;
}

static const char *db_trace_value_type_name(uint8_t type, char *scratch, size_t scratch_len) {
    switch (type) {
        case 0:  return "noval";
        case 1:  return "char";
        case 2:  return "int";
        case 3:  return "long";
        case 4:  return "oldstring";
        case 5:  return "binary";
        case 6:  return "boolean";
        case 7:  return "token";
        case 8:  return "date";
        case 9:  return "address";
        case 10: return "code";
        case 11: return "double";
        case 12: return "string";
        case 13: return "external";
        case 14: return "direction";
        case 15: return "password";
        case 16: return "ostype";
        case 17: return "unused2";
        case 18: return "point";
        case 19: return "rect";
        case 20: return "pattern";
        case 21: return "rgb";
        case 22: return "fixed";
        case 23: return "single";
        case 24: return "olddouble";
        case 25: return "objspec";
        case 26: return "filespec";
        case 27: return "alias";
        case 28: return "enum";
        case 29: return "list";
        case 30: return "record";
        default:
            if ((scratch != NULL) && (scratch_len > 0)) {
                snprintf(scratch, scratch_len, "type_%u", (unsigned int) type);
                return scratch;
            }
            return "unknown";
    }
}

static void db_trace_copy_key(const unsigned char *strings, size_t strings_len, uint32_t ixkey, char *out, size_t out_len) {
    if ((out == NULL) || (out_len == 0)) {
        return;
    }

    out[0] = '\0';
    if ((strings == NULL) || (strings_len == 0) || (ixkey >= strings_len))
        return;

    uint8_t len = strings[ixkey];
    size_t available = 0;
    if (ixkey + 1 < strings_len)
        available = strings_len - (ixkey + 1);
    size_t copy_len = len;
    if (copy_len > available)
        copy_len = available;
    if (copy_len >= out_len)
        copy_len = out_len - 1;

    memcpy(out, strings + ixkey + 1, copy_len);
    out[copy_len] = '\0';

    for (size_t i = 0; i < copy_len; ++i) {
        unsigned char c = (unsigned char) out[i];
        if (c < 0x20 || c > 0x7E)
            out[i] = '?';
    }
}

static boolean db_trace_read_payload(FILE *f, dbaddress adr, unsigned char **out_payload, size_t *out_len) {
    if ((f == NULL) || (adr == nildbaddress) || (out_payload == NULL) || (out_len == NULL))
        return false;

    unsigned char header_bytes[sizeheader];
    if (db_trace_seek(f, (db_trace_off_t) adr, SEEK_SET) != 0)
        return false;
    if (fread(header_bytes, 1, sizeof header_bytes, f) != sizeof header_bytes)
        return false;

    uint32_t raw_size = read_legacy_u32(header_bytes);
    boolean block_free = (raw_size & 0x80000000u) != 0;
    raw_size &= 0x7FFFFFFFu;

    if (block_free || raw_size == 0 || raw_size > DB_TRACE_MAX_PAYLOAD)
        return false;

    unsigned char *buffer = (unsigned char *) malloc(raw_size);
    if (buffer == NULL)
        return false;

    if (fread(buffer, 1, raw_size, f) != raw_size) {
        free(buffer);
        return false;
    }

    *out_payload = buffer;
    *out_len = raw_size;
    return true;
}

static boolean db_trace_detect_cancoon(const unsigned char *payload, size_t payload_len, dbaddress *adr_out) {
    const size_t kCancoonBytes = 442;
    if ((payload == NULL) || (payload_len != kCancoonBytes) || (adr_out == NULL))
        return false;

    if (payload_len < 6)
        return false;

    uint16_t version = read_be16(payload);
    if ((version != 2) && (version != 3))
        return false;

    dbaddress adr = (dbaddress) read_legacy_u32(payload + 2);
    if (adr == nildbaddress)
        return false;

    *adr_out = adr;
    return true;
}

static boolean db_trace_is_sentinel(const unsigned char *record10) {
    for (int i = 0; i < 10; ++i) {
        if (record10[i] != 0)
            return false;
    }
    return true;
}

static boolean db_trace_find_legacy_layout(const unsigned char *payload, size_t payload_len,
                                           size_t *strings_len_out, size_t *records_start_out,
                                           size_t *record_count_out) {
    const size_t header_size = 16;
    const size_t record_size = 10;

    if ((payload == NULL) || (payload_len < header_size + record_size))
        return false;

    for (size_t candidate_strings = 0; candidate_strings <= payload_len - header_size; ++candidate_strings) {
        size_t candidate_records_start = header_size + candidate_strings;
        if (candidate_records_start + record_size > payload_len)
            break;

        boolean valid = false;
        boolean found_sentinel = false;
        size_t local_records = 0;

        for (size_t offset = candidate_records_start; offset + record_size <= payload_len; offset += record_size) {
            const unsigned char *rec = payload + offset;
            if (db_trace_is_sentinel(rec)) {
                found_sentinel = true;
                continue;
            }

            uint32_t ixkey = read_legacy_u32(rec);
            if (ixkey >= candidate_strings) {
                valid = false;
                break;
            }

            valid = true;
            ++local_records;
        }

        if (valid && found_sentinel) {
            if (strings_len_out)
                *strings_len_out = candidate_strings;
            if (records_start_out)
                *records_start_out = candidate_records_start;
            if (record_count_out)
                *record_count_out = local_records;
            return true;
        }
    }

    return false;
}

static void db_trace_log_legacy_entries(db_trace_context *ctx,
                                        const unsigned char *payload,
                                        size_t payload_len,
                                        const char *path,
                                        int depth);

static void db_trace_log_modern_entries(db_trace_context *ctx,
                                        const unsigned char *payload,
                                        size_t payload_len,
                                        const char *path,
                                        int depth);

static void db_trace_consider_entry(db_trace_context *ctx,
                                    const char *parent_path,
                                    int depth,
                                    const char *key_name,
                                    uint8_t valuetype,
                                    uint32_t dataval);

static void db_trace_follow_external_table(db_trace_context *ctx,
                                           dbaddress external_record,
                                           const char *parent_path,
                                           int depth,
                                           const char *key_name);

static void db_trace_descend(db_trace_context *ctx,
                             const char *parent_path,
                             const char *child_name,
                             dbaddress table_addr,
                             int depth);

static dbaddress db_trace_read_dbaddress(const unsigned char *bytes, size_t available) {
    if ((bytes == NULL) || (available == 0))
        return nildbaddress;

    size_t width = sizeof(dbaddress);
    if (available < width) {
        if (available >= 8)
            width = 8;
        else if (available >= 4)
            width = 4;
        else
            width = available;
    }

    dbaddress value = 0;
    for (size_t i = 0; i < width; ++i)
        value = (value << 8) | bytes[i];
    return value;
}

static void db_trace_log_legacy_entries(db_trace_context *ctx,
                                        const unsigned char *payload,
                                        size_t payload_len,
                                        const char *path,
                                        int depth) {
    const size_t header_size = 16;
    size_t strings_len = 0;
    size_t records_start = 0;
    size_t record_count = 0;

    if (!db_trace_find_legacy_layout(payload, payload_len, &strings_len, &records_start, &record_count)) {
        db_trace_log(1, "%s: [%s] legacy table (%d) layout unresolved (payload=%zu)",
                     ctx->path_label, path, depth, payload_len);
        return;
    }

    db_trace_log(1, "%s: [%s] legacy table depth=%d entries=%zu strings=%zu",
                 ctx->path_label, path, depth, record_count, strings_len);

    if (ctx->level < 2)
        return;

    const unsigned char *strings = payload + header_size;
    size_t entries_logged = 0;

    for (size_t offset = records_start; offset + 10 <= payload_len; offset += 10) {
        const unsigned char *rec = payload + offset;
        if (db_trace_is_sentinel(rec))
            continue;

        uint32_t ixkey = read_legacy_u32(rec);
        uint8_t valuetype = rec[4];
        uint32_t dataval = read_legacy_u32(rec + 6);

        char key_name[96];
        char type_name_buf[24];
        db_trace_copy_key(strings, strings_len, ixkey, key_name, sizeof key_name);
        const char *type_name = db_trace_value_type_name(valuetype, type_name_buf, sizeof type_name_buf);

        db_trace_log(2, "%s: [%s] %-24s (%s) data=0x%08x",
                     ctx->path_label, path, key_name[0] ? key_name : "(unnamed)",
                     type_name, (unsigned int) dataval);

        db_trace_consider_entry(ctx, path, depth, key_name, valuetype, dataval);

        if (++entries_logged >= ctx->max_entries) {
            db_trace_log(2, "%s: [%s] entry log limit reached (%zu)",
                         ctx->path_label, path, ctx->max_entries);
            break;
        }
    }
}

static void db_trace_log_modern_entries(db_trace_context *ctx,
                                        const unsigned char *payload,
                                        size_t payload_len,
                                        const char *path,
                                        int depth) {
    if (payload_len < 4)
        return;

    uint32_t outer_size = read_legacy_u32(payload);
    if (outer_size + 4 > payload_len)
        return;

    const unsigned char *inner_merged = payload + 4;
    size_t inner_len = outer_size;
    if (inner_len < 4)
        return;

    uint32_t inner_size = read_legacy_u32(inner_merged);
    if (inner_size + 4 > inner_len)
        return;

    const unsigned char *header_and_records = inner_merged + 4;
    const unsigned char *strings = header_and_records + inner_size;
    size_t strings_len = inner_len - (4 + inner_size);

    const size_t header_size = 16;
    if (inner_size < header_size)
        return;

    const unsigned char *records = header_and_records + header_size;
    size_t records_len = inner_size - header_size;

    size_t record_count = 0;
    for (size_t offset = 0; offset + 10 <= records_len; offset += 10) {
        const unsigned char *rec = records + offset;
        if (db_trace_is_sentinel(rec))
            continue;
        ++record_count;
    }

    db_trace_log(1, "%s: [%s] modern table depth=%d entries=%zu strings=%zu",
                 ctx->path_label, path, depth, record_count, strings_len);

    if (ctx->level < 2)
        return;

    size_t entries_logged = 0;
    for (size_t offset = 0; offset + 10 <= records_len; offset += 10) {
        const unsigned char *rec = records + offset;
        if (db_trace_is_sentinel(rec))
            continue;

        uint32_t ixkey = read_legacy_u32(rec);
        uint8_t valuetype = rec[4];
        uint32_t dataval = read_legacy_u32(rec + 6);

        char key_name[96];
        char type_name_buf[24];
        db_trace_copy_key(strings, strings_len, ixkey, key_name, sizeof key_name);
        const char *type_name = db_trace_value_type_name(valuetype, type_name_buf, sizeof type_name_buf);

        db_trace_log(2, "%s: [%s] %-24s (%s) data=0x%08x",
                     ctx->path_label, path, key_name[0] ? key_name : "(unnamed)",
                     type_name, (unsigned int) dataval);

        db_trace_consider_entry(ctx, path, depth, key_name, valuetype, dataval);

        if (++entries_logged >= ctx->max_entries) {
            db_trace_log(2, "%s: [%s] entry log limit reached (%zu)",
                         ctx->path_label, path, ctx->max_entries);
            break;
        }
    }
}

static void db_trace_consider_entry(db_trace_context *ctx,
                                    const char *parent_path,
                                    int depth,
                                    const char *key_name,
                                    uint8_t valuetype,
                                    uint32_t dataval) {
    if (ctx == NULL || parent_path == NULL)
        return;
    if (ctx->max_depth == 0)
        return;
    if (depth >= ctx->max_depth)
        return;
    if (dataval == 0)
        return;

    if (valuetype == 13) { /* external value */
        db_trace_follow_external_table(ctx, (dbaddress) dataval, parent_path, depth, key_name);
    }
}

static void db_trace_follow_external_table(db_trace_context *ctx,
                                           dbaddress external_record,
                                           const char *parent_path,
                                           int depth,
                                           const char *key_name) {
    if (external_record == nildbaddress)
        return;

    unsigned char *payload = NULL;
    size_t payload_len = 0;
    if (!db_trace_read_payload(ctx->file, external_record, &payload, &payload_len)) {
        db_trace_log(2, "%s: [%s.%s] unable to read external block @0x%08llx",
                     ctx->path_label,
                     parent_path,
                     (key_name && *key_name) ? key_name : "(anon)",
                     (unsigned long long) external_record);
        return;
    }

    if (payload_len < 6) {
        db_trace_log(2, "%s: [%s.%s] external block too small (%zu bytes)",
                     ctx->path_label,
                     parent_path,
                     (key_name && *key_name) ? key_name : "(anon)",
                     payload_len);
        free(payload);
        return;
    }

    uint16_t version = read_be16(payload);
    uint16_t proc_id = read_be16(payload + 2);
    if (proc_id != DB_TRACE_EXTERNAL_TABLE_ID) {
        db_trace_log(2, "%s: [%s.%s] external id=%u (not table), skipping",
                     ctx->path_label,
                     parent_path,
                     (key_name && *key_name) ? key_name : "(anon)",
                     (unsigned int) proc_id);
        free(payload);
        return;
    }

    size_t address_offset = 4;
    if (payload_len <= address_offset) {
        db_trace_log(2, "%s: [%s.%s] external table missing address payload (len=%zu)",
                     ctx->path_label,
                     parent_path,
                     (key_name && *key_name) ? key_name : "(anon)",
                     payload_len);
        free(payload);
        return;
    }

    dbaddress table_addr = db_trace_read_dbaddress(payload + address_offset,
                                                   payload_len - address_offset);
    free(payload);

    if (table_addr == nildbaddress) {
        db_trace_log(2, "%s: [%s.%s] external table has nil address (version=%u)",
                     ctx->path_label,
                     parent_path,
                     (key_name && *key_name) ? key_name : "(anon)",
                     (unsigned int) version);
        return;
    }

    db_trace_descend(ctx, parent_path, key_name, table_addr, depth);
}

static void db_trace_descend(db_trace_context *ctx,
                             const char *parent_path,
                             const char *child_name,
                             dbaddress table_addr,
                             int depth) {
    if (depth >= ctx->max_depth)
        return;

    char next_path[DB_TRACE_PATH_MAX];
    if (child_name != NULL && *child_name) {
        snprintf(next_path, sizeof next_path, "%s.%s", parent_path, child_name);
    } else {
        snprintf(next_path, sizeof next_path, "%s.child", parent_path);
    }

    db_trace_walk_table(ctx, table_addr, next_path, depth + 1);
}

static void db_trace_walk_table(db_trace_context *ctx, dbaddress adr, const char *path, int depth) {
    if ((ctx == NULL) || (ctx->file == NULL) || (path == NULL))
        return;
    if (db_trace_level() == 0)
        return;
    if (adr == nildbaddress)
        return;
    if (depth > ctx->max_depth)
        return;
    if (!db_trace_add_visited(ctx, adr))
        return;

    unsigned char *payload = NULL;
    size_t payload_len = 0;
    if (!db_trace_read_payload(ctx->file, adr, &payload, &payload_len)) {
        db_trace_log(1, "%s: [%s] unable to read payload @0x%08llx",
                     ctx->path_label, path, (unsigned long long) adr);
        return;
    }

    dbaddress redirected = nildbaddress;
    if (db_trace_detect_cancoon(payload, payload_len, &redirected)) {
        db_trace_log(1, "%s: [%s] Cancoon header -> 0x%08llx",
                     ctx->path_label, path, (unsigned long long) redirected);
        free(payload);
        db_trace_walk_table(ctx, redirected, path, depth);
        return;
    }

    boolean legacy = true;
    if (payload_len >= 4) {
        uint32_t prefix = read_legacy_u32(payload);
        if ((prefix >= 16) && (prefix <= (payload_len - 4)))
            legacy = false;
    }

    if (legacy)
        db_trace_log_legacy_entries(ctx, payload, payload_len, path, depth);
    else
        db_trace_log_modern_entries(ctx, payload, payload_len, path, depth);

    free(payload);
}

static boolean db_trace_extract_root(const unsigned char *header, size_t header_len, short version, dbaddress *root_out) {
    if ((header == NULL) || (root_out == NULL))
        return false;

    const size_t view_stride = 8;
    const size_t view_base = 0x0E;
    dbaddress found = nildbaddress;

    if (version >= 7) {
        for (int i = 0; i < ctviews; ++i) {
            size_t offset = view_base + (size_t) i * view_stride;
            if (header_len < offset + view_stride)
                break;
            uint64_t raw = read_be64(header + offset);
            if (raw != 0) {
                found = (dbaddress) raw;
                break;
            }
        }
    } else {
        for (int i = 0; i < ctviews; ++i) {
            size_t offset = view_base + (size_t) i * view_stride;
            size_t legacy_bytes = 6; /* legacy files stored 48-bit addresses */
            if (header_len < offset + legacy_bytes)
                break;
            uint64_t raw = 0;
            for (size_t b = 0; b < legacy_bytes; ++b)
                raw = (raw << 8) | header[offset + b];
            if (raw != 0) {
                found = (dbaddress) raw;
                break;
            }
        }
    }

    if (found == nildbaddress)
        return false;

    *root_out = found;
    return true;
}

static void db_format_trace_database_path(const char *path_label) {
    if ((path_label == NULL) || (db_trace_level() == 0))
        return;

    FILE *f = fopen(path_label, "rb");
    if (f == NULL) {
        db_trace_log(1, "trace: unable to open %s", path_label);
        return;
    }

    unsigned char header[sizeof(tydatabaserecord_64)];
    size_t header_len = fread(header, 1, sizeof header, f);
    if (header_len < LEGACY_DB_HEADER_BYTES) {
        db_trace_log(1, "%s: header too small (%zu bytes)", path_label, header_len);
        fclose(f);
        return;
    }

    short version = (short) header[1];
    dbaddress root = nildbaddress;
    if (!db_trace_extract_root(header, header_len, version, &root)) {
        db_trace_log(1, "%s: unable to locate root table pointer (version %d)", path_label, version);
        fclose(f);
        return;
    }

    db_trace_log(1, "%s: version=%d root=0x%08llx", path_label, version, (unsigned long long) root);

    db_trace_context ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.file = f;
    ctx.path_label = path_label;
    ctx.level = db_trace_level();
    ctx.max_depth = db_trace_depth_limit();
    ctx.max_entries = db_trace_entry_limit(ctx.level);

    db_trace_walk_table(&ctx, root, "root", 0);
    fclose(f);
}

/* Utility helpers for big-endian encoding/decoding */
static uint16_t read_be16(const void *ptr) {
    const unsigned char *p = (const unsigned char *) ptr;
    return (uint16_t) ((p[0] << 8) | p[1]);
}

static uint32_t read_legacy_u32(const unsigned char *field) {
    return ((uint32_t) field[0] << 24) |
           ((uint32_t) field[1] << 16) |
           ((uint32_t) field[2] << 8)  |
            (uint32_t) field[3];
}

static dbaddress read_legacy_dbaddress32(const unsigned char *field) {
    return (dbaddress) read_legacy_u32(field);
}

static uint64_t read_be64(const unsigned char *field) {
    return ((uint64_t) field[0] << 56) |
           ((uint64_t) field[1] << 48) |
           ((uint64_t) field[2] << 40) |
           ((uint64_t) field[3] << 32) |
           ((uint64_t) field[4] << 24) |
           ((uint64_t) field[5] << 16) |
           ((uint64_t) field[6] << 8)  |
            (uint64_t) field[7];
}

/* 2025-11-24 Codex: Decode raw header into a consistent in-memory record (legacy vs v7). */
boolean db_format_decode_header(const unsigned char *rawheader, size_t raw_len, boolean *header_is_modern, tydatabaserecord *out) {
    int i;
    int header_version = 0;
    size_t needed = 0;

    if ((rawheader == NULL) || (header_is_modern == NULL) || (out == NULL))
        return false;

    if (!db_format_header_version(rawheader, raw_len, &header_version))
        return false;

    *header_is_modern = false;
    memset(out, 0, sizeof *out);

    needed = (header_version >= 7) ? sizeof(tydatabaserecord_64) : sizeof(tydatabaserecord);
    if (raw_len < needed)
        return false;

    if (header_version >= 7) {
        *header_is_modern = true;
        out->systemid = rawheader[0];
        out->versionnumber = rawheader[1];
        out->availlist = (dbaddress) read_be64(rawheader + offsetof(tydatabaserecord_64, availlist));
        out->oldfnumdatabase = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, oldfnumdatabase));
        out->flags = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, flags));

        for (i = 0; i < ctviews; i++) {
            size_t offset = offsetof(tydatabaserecord_64, views) + (size_t) i * sizeof(dbaddress);
            dbaddress view = (dbaddress) read_be64(rawheader + offset);
            /* Normalize if a legacy 32-bit view was shifted into the high dword. */
            if ((view & 0xFFFFFFFF00000000ULL) != 0 && (view & 0xFFFFFFFFULL) == 0) {
                view = (dbaddress) ((uint64_t) view >> 32);
            }
            out->views[i] = view;
#if defined(FRONTIER_HEADLESS)
            fprintf(stderr, "[headless] decode v7 view[%d]=0x%016llx\n", i, (unsigned long long) view);
#endif
        }

        out->releasestack = nil;
        out->fnumdatabase = 0;
        out->headerLength = (long) db_format_read_be32(rawheader + offsetof(tydatabaserecord_64, headerLength));
        out->longversionMajor = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, longversionMajor));
        out->longversionMinor = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, longversionMinor));

        out->u.extensions.availlistblock = (dbaddress) db_format_read_be64(rawheader + offsetof(tydatabaserecord_64, u.extensions.availlistblock));
        out->u.extensions.flreadonly = rawheader[offsetof(tydatabaserecord_64, u.extensions.flreadonly)];
    } else {
        memcpy(out, rawheader, sizeof *out);

        out->availlist = (dbaddress) read_legacy_dbaddress32(rawheader + 2);
        for (i = 0; i < ctviews; i++)
            out->views[i] = (dbaddress) read_legacy_dbaddress32(rawheader + 10 + (size_t) i * 4);

#ifdef SWAP_BYTE_ORDER
        {
        disktomemlong (out->u.extensions.availlistblock);
        disktomemshort (out->flags);
//      disktomemlong (out->fnumdatabase);
        disktomemlong (out->headerLength);
        disktomemshort (out->longversionMajor);
        disktomemshort (out->longversionMinor);
        }
#endif
    }

    return true;
}

/* 2025-11-25 Codex: Legacy adapter widening + strict v7 reader. */
boolean db_format_widen_legacy_header(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out) {
    int i;
    if ((decoded_header == NULL) || (widened_out == NULL))
        return false;
    if (decoded_header->versionnumber > 6)
        return false;

    memset(widened_out, 0, sizeof *widened_out);
    widened_out->systemid = decoded_header->systemid;
    widened_out->versionnumber = 7;
    widened_out->availlist = decoded_header->availlist;
    widened_out->oldfnumdatabase = decoded_header->oldfnumdatabase;
    widened_out->flags = decoded_header->flags;
    for (i = 0; i < ctviews; ++i)
        widened_out->views[i] = decoded_header->views[i];

    widened_out->releasestack = nil;
    widened_out->fnumdatabase = 0;

    if (decoded_header->headerLength > 0)
        widened_out->headerLength = decoded_header->headerLength;
    else
        widened_out->headerLength = (long) LEGACY_DB_HEADER_BYTES;

    if (widened_out->headerLength < (long) sizeof (tydatabaserecord_64))
        widened_out->headerLength = (long) sizeof (tydatabaserecord_64);

    widened_out->longversionMajor = decoded_header->longversionMajor != 0 ? decoded_header->longversionMajor : 6;
    widened_out->longversionMinor = decoded_header->longversionMinor != 0 ? decoded_header->longversionMinor : 1;

    widened_out->u.extensions.availlistblock = decoded_header->u.extensions.availlistblock;
    widened_out->u.extensions.availlistshadow = nildbaddress;
    widened_out->u.extensions.flreadonly = flreadonly;
    memset(widened_out->u.extensions.reserved, 0, sizeof widened_out->u.extensions.reserved);

    return true;
}

boolean db_format_load_legacy_adapter(const tydatabaserecord *decoded_header, boolean flreadonly, tydatabaserecord_64 *widened_out) {
    long header_len = 0;
    tydatabaserecord header_copy;
    tydatabaserecord_64 widened;

    if (decoded_header == NULL)
        return false;
    if (decoded_header->versionnumber > 6)
        return false;

    header_len = decoded_header->headerLength;
    if (header_len <= 0)
        header_len = (long) LEGACY_DB_HEADER_BYTES;
    if (header_len < (long) LEGACY_DB_HEADER_BYTES)
        return false;

    header_copy = *decoded_header;
    header_copy.headerLength = header_len;

    /* Keep legacy read path active; widening happens before writing. */
    db_format_mode legacy = {false, g_legacy_adapter_force_repack, false};
    db_format_mode_apply(&legacy);
    g_legacy_adapter_active = false;
    g_legacy_adapter_force_repack = false;

    memset(&g_legacy_widened_header, 0, sizeof g_legacy_widened_header);
    if (!db_format_widen_legacy_header(&header_copy, flreadonly, &widened))
        return false;

    g_legacy_adapter_active = true;
    g_legacy_adapter_force_repack = true;
    g_legacy_widened_header = widened;

    if (widened_out != NULL)
        *widened_out = widened;

    return true;
}

boolean db_format_load_v7_reader(const tydatabaserecord *decoded_header, boolean flreadonly) {
    #pragma unused (flreadonly)
    long header_len = 0;
    if (decoded_header == NULL)
        return false;
    if (decoded_header->versionnumber < 7)
        return false;

    header_len = decoded_header->headerLength;
    if (header_len <= 0)
        header_len = (long) sizeof (tydatabaserecord_64);
    if (header_len < (long) sizeof (tydatabaserecord_64))
        return false;

    db_format_mode modern = {true, g_legacy_adapter_force_repack, false};
    db_format_mode_apply(&modern);
    g_legacy_adapter_active = false;
    g_legacy_adapter_force_repack = false;
    memset(&g_legacy_widened_header, 0, sizeof g_legacy_widened_header);
    g_legacy_widened_header.systemid = dbsystemidMac; /* canonical default */
    g_legacy_widened_header.versionnumber = dbversionnumber;
    g_legacy_widened_header.headerLength = (long) sizeof(tydatabaserecord_64);
    g_legacy_widened_header.longversionMajor = 7;
    g_legacy_widened_header.longversionMinor = 0;
    return true;
}

boolean db_format_adapter_enable_wide_writes(const tydatabaserecord_64 **widened_header_out) {
    if (!g_legacy_adapter_active)
        return false;

    db_format_mode modern = {true, true, false};
    db_format_mode_apply(&modern);

    if (databasedata != nil) {
        if ((**databasedata).headerLength < (long) sizeof (tydatabaserecord_64))
            (**databasedata).headerLength = (long) sizeof (tydatabaserecord_64);
        if ((**databasedata).longversionMajor == 0)
            (**databasedata).longversionMajor = 6;
        if ((**databasedata).longversionMinor == 0)
            (**databasedata).longversionMinor = 1;
    }

    if (widened_header_out != NULL)
        *widened_header_out = &g_legacy_widened_header;

    return true;
}

boolean db_format_adapter_enable_wide_writes_context(const db_context *context, const tydatabaserecord_64 **widened_header_out) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = db_format_adapter_enable_wide_writes(widened_header_out);
    db_context_guard_exit(&guard);
    return ok;
}

boolean db_format_adapter_force_repack(void) {
    return g_legacy_adapter_force_repack;
}

void db_format_adapter_mark_address(dbaddress *adr_out) {
    if (!g_legacy_adapter_active || adr_out == NULL)
        return;

    if (g_legacy_widened_header.headerLength < (long) sizeof (tydatabaserecord_64))
        g_legacy_widened_header.headerLength = (long) sizeof (tydatabaserecord_64);
}

boolean db_format_adapter_is_active(void) {
    return g_legacy_adapter_active;
}

void db_format_set_legacy_source_db(hdldatabaserecord hdb) {
    g_legacy_source_db = hdb;
}

boolean db_format_is_legacy_db(hdldatabaserecord hdb) {
    return (hdb != nil) && (hdb == g_legacy_source_db);
}

boolean db_format_write_header64(const tydatabaserecord_64 *src, unsigned char *dest, size_t dest_size) {
    if ((src == NULL) || (dest == NULL) || (dest_size < sizeof(tydatabaserecord_64)))
        return false;

    memset(dest, 0, dest_size);
    dest[0] = src->systemid;
    dest[1] = src->versionnumber;

    db_format_write_dbaddress64(dest + offsetof(tydatabaserecord_64, availlist), src->availlist);
    db_format_write_be16(dest + offsetof(tydatabaserecord_64, oldfnumdatabase), (uint16_t) src->oldfnumdatabase);
    db_format_write_be16(dest + offsetof(tydatabaserecord_64, flags), (uint16_t) src->flags);

#if defined(FRONTIER_HEADLESS)
    /* Verify structure layout matches disk format */
    if (offsetof(tydatabaserecord_64, views) != 16) {
        fprintf(stderr, "[headless] FATAL: tydatabaserecord_64.views offset=%zu expected=16\n",
                offsetof(tydatabaserecord_64, views));
        return false;
    }
#endif

    for (int i = 0; i < ctviews; ++i) {
        size_t offset = offsetof(tydatabaserecord_64, views) + (size_t) i * sizeof(dbaddress);
        db_format_write_dbaddress64(dest + offset, src->views[i]);
    }

    db_format_write_be32(dest + offsetof(tydatabaserecord_64, headerLength), (uint32_t) src->headerLength);
    db_format_write_be16(dest + offsetof(tydatabaserecord_64, longversionMajor), (uint16_t) src->longversionMajor);
    db_format_write_be16(dest + offsetof(tydatabaserecord_64, longversionMinor), (uint16_t) src->longversionMinor);

    db_format_write_dbaddress64(dest + offsetof(tydatabaserecord_64, u.extensions.availlistblock), src->u.extensions.availlistblock);
    db_format_write_dbaddress64(dest + offsetof(tydatabaserecord_64, u.extensions.availlistshadow), src->u.extensions.availlistshadow);
    dest[offsetof(tydatabaserecord_64, u.extensions.flreadonly)] = src->u.extensions.flreadonly ? 1u : 0u;

    return true;
}

boolean detect_database_format(const tydatabaserecord *header) {
    if (header == NULL)
        return false;

    if (header->versionnumber <= 6)
        return true;  /* Legacy 32-bit format */
    if (header->versionnumber >= 7)
        return true;  /* New 64-bit format */
    return false;  /* Unsupported version */
}

boolean convert_32bit_header_to_64bit(const unsigned char *legacy_header, tydatabaserecord_64 *new_header) {
    if ((legacy_header == NULL) || (new_header == NULL))
        return false;

    memset(new_header, 0, sizeof *new_header);

    new_header->systemid = legacy_header[0];
    new_header->versionnumber = 7;
    new_header->availlist = read_legacy_dbaddress32(legacy_header + 2);
    new_header->oldfnumdatabase = (short) read_be16(legacy_header + 6);
    new_header->flags = (short) read_be16(legacy_header + 8);

    const size_t view_base = 10;
    const size_t view_stride = 4;
    for (int i = 0; i < ctviews; ++i)
        new_header->views[i] = read_legacy_dbaddress32(legacy_header + view_base + (size_t)i * view_stride);

    new_header->releasestack = 0; /* recalculated at runtime if needed */
    new_header->fnumdatabase = 0;
    uint32_t legacy_header_length = read_legacy_u32(legacy_header + 30);
    if (legacy_header_length == 0)
        legacy_header_length = (uint32_t) sizeof(tydatabaserecord_64);
    new_header->headerLength = (long) legacy_header_length;
    new_header->longversionMajor = (short) read_be16(legacy_header + 34);
    if (new_header->longversionMajor == 0)
        new_header->longversionMajor = 6;
    new_header->longversionMinor = (short) read_be16(legacy_header + 36);

    new_header->u.extensions.availlistblock = read_legacy_dbaddress32(legacy_header + 38);
    new_header->u.extensions.availlistshadow = nildbaddress;
    new_header->u.extensions.flreadonly = false;
    memset(new_header->u.extensions.reserved, 0, sizeof new_header->u.extensions.reserved);

    return true;
}

boolean create_root_backup(const char *original_path) {
    if (original_path == NULL)
        return false;

    last_backup_path[0] = '\0';

    char backup_path[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info == NULL)
        return false;

    snprintf(backup_path, sizeof backup_path,
             "%s.%04d%02d%02d_%02d%02d%02d",
             original_path,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    FILE *src = fopen(original_path, "rb");
    FILE *dst = fopen(backup_path, "wb");

    if (!src || !dst) {
        if (src)
            fclose(src);
        if (dst)
            fclose(dst);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return false;
        }
    }

    fclose(src);
    fclose(dst);
    strncpy(last_backup_path, backup_path, sizeof last_backup_path);
    last_backup_path[sizeof last_backup_path - 1] = '\0';
    return true;
}

/* During migration, skip externals whose addresses point at free blocks so packing won't fail. */
static void db_format_sanitize_root_externals(hdlhashtable hroot, const db_context *context) {
    if (hroot == nil)
        return;

    long ix = 0;
    hdlhashnode hnode = nil;
    while (hashgetnthnode(hroot, ix++, &hnode)) {
        if (hnode == nil)
            continue;

        tyvaluerecord *val = &(**hnode).val;
        if (val->valuetype != externalvaluetype)
            continue;

        hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;
        if (hv == nil)
            continue;

        if ((**hv).flinmemory)
            continue;

        dbaddress adr = (dbaddress) (**hv).variabledata;
        boolean ok = false;
        Handle htmp = nil;

        if (adr != nildbaddress && adr != 0) {
            if (context != NULL)
                ok = dbrefhandle_context(context, adr, &htmp);
            else
                ok = dbrefhandle(adr, &htmp);
        }

        if (htmp != nil)
            disposehandle(htmp);

        if (ok)
            continue; /* block exists; keep it */

        bigstring bsname;
        gethashkey(hnode, bsname);
        fprintf(stderr,
                "[headless] migrate dropping external name='%.*s' adr=0x%llx (free/unreadable)\n",
                (int) bsname[0],
                (char *) &bsname[1],
                (unsigned long long) adr);

        (**hnode).fldontsave = true;
        (**hv).flinmemory = true;
        (**hv).variabledata = 0;
        (**hv).oldaddress = nildbaddress;
        val->fldiskval = false;
    }
}

static boolean migrate_internal(const char *db_path, boolean drop_cancoon) {
    if (db_path == NULL || db_path[0] == '\0')
        return false;

    boolean ok = false;
    db_saveas_state entry_saveas;
    hdlfilenum src_fnum = 0;
    hdlfilenum dst_fnum = 0;
    Handle hrootvariable = nil;
    hdlhashtable hroot = nil;
    Handle hscript = nil;
    db_context source_context;
    db_context dest_context;
    boolean have_dest_context = false;
    boolean saved_root = false;
    dbaddress root_address = nildbaddress;
    dbaddress script_address = nildbaddress;
    dbaddress new_root_address = nildbaddress;
    dbaddress new_script_address = nildbaddress;
    dbaddress new_cancoon_address = nildbaddress;
    dbaddress view_address = nildbaddress;
    tyversion2cancoonrecord cancoon_record;
    uint16_t cancoon_version = 0;
    uint16_t cancoon_flags = 0;
    uint16_t cancoon_primary = 0;
    db_format_mode entry_mode = db_format_mode_current();
    char output_path[1024];
    char temp_path[1024];
    temp_path[0] = '\0';
    bigstring bspath;
    bigstring bsdst;
    tyfilespec src_fs;
    tyfilespec dst_fs;
    const char *fail_step = "init";
    const long header_len_final = (long) sizeof(tydatabaserecord_64);

#if defined(FRONTIER_HEADLESS)
    fprintf(stderr, "[headless] migrate start path=%s drop=%d\n", db_path, drop_cancoon ? 1 : 0);
#endif

    db_saveas_state_snapshot(&entry_saveas);

    /* Force legacy read mode while pulling from the v6 source; we restore at cleanup. */
    db_format_mode legacy_mode = {false, false, false};
    db_format_mode_push(&legacy_mode);

    fail_step = "prepare runtime";
    if (!db_format_prepare_runtime())
        goto cleanup;
#if defined(FRONTIER_HEADLESS)
    fprintf(stderr, "[headless] migrate after prepare runtime\n");
#endif

    if (db_trace_level() > 0)
        db_format_trace_database_path(db_path);

    /* Derive output path (<base>-v7.root) */
    const char *ext = strrchr(db_path, '.');
    if (ext && strcmp(ext, ".root") == 0) {
        size_t base_len = (size_t)(ext - db_path);
        snprintf(output_path, sizeof output_path, "%.*s-v7.root", (int) base_len, db_path);
    } else {
        snprintf(output_path, sizeof output_path, "%s-v7", db_path);
    }

    strncpy(last_backup_path, output_path, sizeof last_backup_path);
    if (sizeof last_backup_path > 0)
        last_backup_path[sizeof last_backup_path - 1] = '\0';

    snprintf(temp_path, sizeof temp_path, "%s.tmp", output_path);

    copyctopstring(db_path, bspath);
    fail_step = "pathtofilespec(src)";
    if (!pathtofilespec(bspath, &src_fs))
        goto cleanup;

    fail_step = "openfile(src)";
    if (!openfile(&src_fs, &src_fnum, true))
        goto cleanup;

    fail_step = "dbopenfile(src)";
    if (!dbopenfile(src_fnum, true))
        goto cleanup;

    db_context_init(&source_context);
    source_context.mode = db_format_mode_current();
    source_context.database = databasedata;

    db_context_init(&dest_context);
    dest_context.mode = source_context.mode;
    dest_context.database = nil;

    dbgetview(cancoonview, &view_address);
    if (view_address == nildbaddress)
        goto cleanup;

    fail_step = "dbreference(Cancoon)";
    if (!dbreference_context(&source_context, view_address, (long) sizeof cancoon_record, &cancoon_record))
        goto cleanup;

    cancoon_version = read_be16(&cancoon_record.versionnumber);
    cancoon_flags = read_be16(&cancoon_record.flags);
    cancoon_primary = read_be16(&cancoon_record.ixprimaryagent);
    root_address = (dbaddress) read_legacy_u32((const unsigned char *) &cancoon_record.adrroottable);
    script_address = (dbaddress) read_legacy_u32((const unsigned char *) &cancoon_record.adrscriptstring);

    fail_step = "tableloadsystemtable(root)";
    if (!tableloadsystemtable(root_address, &hrootvariable, &hroot, false))
        goto cleanup;

    fail_step = "langhash_materialize_disk_values(root)";
    if (!langhash_materialize_disk_values(hroot))
        goto cleanup;

    /* Force full repack of the root table under the adapter so legacy blocks are rewritten in BE64. */
    if (db_format_adapter_force_repack()) {
        hdltablevariable hv = (hdltablevariable) hrootvariable;
        hdlhashtable ht = (hdlhashtable) (**hv).variabledata;
        if (ht != nil) {
            (**ht).fldirty = true;
            (**ht).flsubsdirty = true;
            (**hv).oldaddress = nildbaddress; /* force new allocation */
        }
        (**hv).flinmemory = true;
    }
    /* Load root into memory before switching to 64-bit writes. */
    fail_step = "tableverbinmemory(root)";
    if (!tableverbinmemory((hdlexternalvariable) hrootvariable, HNoNode))
        goto cleanup;

    db_format_sanitize_root_externals(hroot, &source_context);

    if (script_address != nildbaddress && script_address != 0) {
        fail_step = "dbrefhandle(script)";
        if (!dbrefhandle_context(&source_context, script_address, &hscript))
            goto cleanup;
    }

    copyctopstring(temp_path, bsdst);
    fail_step = "pathtofilespec(dst)";
    if (!pathtofilespec(bsdst, &dst_fs))
        goto cleanup;

    fail_step = "opennewfile(dst)";
    if (!opennewfile(&dst_fs, 'LAND', 'ROOT', &dst_fnum))
        goto cleanup;

    fail_step = "dbstartsaveas";
    if (!dbstartsaveas_context(&dest_context, dst_fnum))
        goto cleanup;

    if (dest_context.database == nil) {
        fail_step = "destination-handle";
        goto cleanup;
    }
    /* From here on, writes should target the destination handle. */
    dest_context.database = dest_context.saveas.destination;
    db_saveas_state_apply(&dest_context.saveas);

    dest_context.mode = source_context.mode;
    dest_context.mode.use_64bit_format = true;
    dest_context.mode.adapter_repack = db_format_adapter_force_repack();
    dest_context.mode.drop_cancoon = false;
    have_dest_context = true;

    /* Switch the destination into BE64 write mode before any assigns. */
    if (have_dest_context)
        db_format_adapter_enable_wide_writes_context(&dest_context, NULL);
    else
        db_format_adapter_enable_wide_writes(NULL);

    fail_step = "tablesavesystemtable(root)";
    /* Read from the source handle while Save As remains active for destination writes. */
    db_context_guard save_guard;
    db_context save_ctx = source_context;
    if (have_dest_context) {
        save_ctx.mode = dest_context.mode; /* write modern BE64 payloads into the destination */
        save_ctx.saveas = dest_context.saveas;
#if defined(FRONTIER_HEADLESS)
        fprintf(stderr,
                "[headless] migrate save_ctx.mode use64=%d adapter=%d drop=%d dest_db=%p src_db=%p\n",
                save_ctx.mode.use_64bit_format ? 1 : 0,
                save_ctx.mode.adapter_repack ? 1 : 0,
                save_ctx.mode.drop_cancoon ? 1 : 0,
                (void *) save_ctx.saveas.destination,
                (void *) save_ctx.saveas.source);
#endif
    }
    db_context_guard_enter(have_dest_context ? &save_ctx : &source_context, &save_guard);
#if defined(FRONTIER_HEADLESS)
    fprintf(stderr,
            "[headless] migrate guard applied mode use64=%d adapter=%d drop=%d depth=%d current_db=%p\n",
            db_format_mode_current().use_64bit_format ? 1 : 0,
            db_format_mode_current().adapter_repack ? 1 : 0,
            db_format_mode_current().drop_cancoon ? 1 : 0,
            g_mode_depth,
            (void *) databasedata);
#endif

    saved_root = tablesavesystemtable(hrootvariable, &new_root_address);
    if (!saved_root) {
        db_context_guard_exit(&save_guard);
        goto cleanup;
    }
    if (have_dest_context && dest_context.database != nil) {
        long eof = 0;
        filegeteof((hdlfilenum) (**dest_context.database).fnumdatabase, &eof);
        fprintf(stderr, "[headless] migrate write checkpoint fnum=%ld eof=%ld\n",
                (long) (**dest_context.database).fnumdatabase, eof);
    }
    db_context_guard_exit(&save_guard);
    if (have_dest_context)
        db_saveas_state_apply(&dest_context.saveas); /* restore destination after source read */

    if ((uint64_t) new_root_address > 0xFFFFFFFFULL)
        goto cleanup;

    if (hscript != nil) {
        new_script_address = script_address;
        fail_step = "dbassignhandle(script)";
        if (!dbassignhandle_context(&dest_context, hscript, &new_script_address))
            goto cleanup;
        if ((uint64_t) new_script_address > 0xFFFFFFFFULL)
            goto cleanup;
    } else {
        new_script_address = 0;
    }

    if (drop_cancoon) {
        /* Modern v7 root: drop legacy Cancoon and point view0 at the root table only. */
        for (int i = 0; i < ctviews; ++i)
            dbsetview(i, nildbaddress);
        dbsetview(cancoonview, new_root_address);
        new_cancoon_address = nildbaddress;
    } else {
        /* Legacy-compatible: rewrite Cancoon with updated pointers. */
        db_format_write_be16(&cancoon_record.versionnumber, cancoon_version);
        db_format_write_be16(&cancoon_record.flags, cancoon_flags);
        db_format_write_be16(&cancoon_record.ixprimaryagent, cancoon_primary);
        db_format_write_be32(&cancoon_record.adrroottable, (uint32_t) new_root_address);
        db_format_write_be32(&cancoon_record.adrscriptstring, (uint32_t) new_script_address);

        fail_step = "dbassign(Cancoon)";
        if (!dbassign_context(&dest_context, &new_cancoon_address, (long) sizeof cancoon_record, &cancoon_record))
            goto cleanup;

        dbsetview(cancoonview, new_cancoon_address);
    }

    if (databasedata != nil) {
        (**databasedata).headerLength = header_len_final;
        (**databasedata).versionnumber = dbversionnumber;
        if ((**databasedata).longversionMajor == 0)
            (**databasedata).longversionMajor = 7;
        if ((**databasedata).longversionMinor == 0)
            (**databasedata).longversionMinor = 0;
    }

#if defined(FRONTIER_HEADLESS)
    if (have_dest_context && dest_context.saveas.destination != nil) {
        fprintf(stderr,
                "[headless] saveas pre-close dest=%p master=%p source=%p\n",
                (void *) dest_context.saveas.destination,
                validhandle((Handle) dest_context.saveas.destination) ? (void *) (*dest_context.saveas.destination) : NULL,
                (void *) dest_context.saveas.source);
    }
#endif

    fail_step = "dbendsaveas";
    if (have_dest_context) {
        if (!dbendsaveas_context(&dest_context))
            goto cleanup;
    } else {
        if (!dbendsaveas())
            goto cleanup;
    }

    closefile(dst_fnum);
    dst_fnum = 0;

    fail_step = "rename(tmp->final)";
    if (rename(temp_path, output_path) != 0)
        goto cleanup;

    dbaddress view_for_log = new_root_address;

    if (db_trace_level() > 0)
        db_format_trace_database_path(output_path);
    fprintf(stderr,
            "[headless] migrate drop=%d ok view0=0x%llx new_root=0x%llx new_script=0x%llx header_len=%ld outfile=%s\n",
            drop_cancoon,
            (unsigned long long) view_for_log,
            (unsigned long long) new_root_address,
            (unsigned long long) new_script_address,
            header_len_final,
            output_path);

    ok = true;

cleanup:
    db_format_mode_pop(); /* restore prior mode before exit */
    if (hscript != nil)
        disposehandle(hscript);
    if (hrootvariable != nil)
        tableverbdispose((hdlexternalvariable) hrootvariable, true);

    if (fldatabasesaveas) {
        if (have_dest_context)
            dbendsaveas_context(&dest_context);
        else
            dbendsaveas();
    }

    if (databasedata != nil)
        dbdispose();

    if (src_fnum != 0)
        closefile(src_fnum);

    if (!ok) {
        if (dst_fnum != 0)
            closefile(dst_fnum);
        if (temp_path[0] != '\0')
            remove(temp_path);
    }

    if (!ok) {
        fprintf(stderr,
                "[headless] migrate drop=%d fail at %s view=0x%llx root=0x%llx new_root=0x%llx script=0x%llx new_script=0x%llx cancoon=0x%llx new_cancoon=0x%llx tmp=%s\n",
                drop_cancoon,
                fail_step,
                (unsigned long long) view_address,
                (unsigned long long) root_address,
                (unsigned long long) new_root_address,
                (unsigned long long) script_address,
                (unsigned long long) new_script_address,
                (unsigned long long) view_address,
                (unsigned long long) new_cancoon_address,
                temp_path);
    } else if (db_trace_level() > 0) {
        fprintf(stderr,
                "[headless] migrate drop=%d ok view=0x%llx root=0x%llx new_root=0x%llx new_script=0x%llx outfile=%s\n",
                drop_cancoon,
                (unsigned long long) view_address,
                (unsigned long long) root_address,
                (unsigned long long) new_root_address,
                (unsigned long long) new_script_address,
                output_path);
    }

    db_format_mode_apply(&entry_mode);
    db_saveas_state_apply(&entry_saveas);

    return ok;
}

boolean migrate_32bit_to_64bit(const char *db_path) {
    return migrate_internal(db_path, true);
}

boolean migrate_32bit_to_64bit_drop_cancoon(const char *db_path) {
    return migrate_internal(db_path, true);
}

boolean ensure_database_modern(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size) {
    if (migrated)
        *migrated = false;
    if (db_path == NULL || db_path[0] == '\0')
        return false;

    FILE *fp = fopen(db_path, "rb");
    if (!fp)
        return false;

    tydatabaserecord header;
    boolean ok = fread(&header, sizeof header, 1, fp) == 1;
    fclose(fp);
    if (!ok)
        return false;

    if (!detect_database_format(&header))
        return false;

    /* Seed format mode from the on-disk header so we don't remigrate already-modern roots. */
    db_format_mode detected_mode = {header.versionnumber >= 7, false, false};
    db_format_mode_apply(&detected_mode);

    if (db_format_mode_current().use_64bit_format) {
        /* Already modern - return original path */
        if (output_path && output_path_size > 0) {
            strncpy(output_path, db_path, output_path_size);
            if (output_path_size > 0)
                output_path[output_path_size - 1] = '\0';
        }
        return true;
    }

    if (!migrate_internal(db_path, true))
        return false;

    /* Migration succeeded; return path to new v7 file */
    if (output_path && output_path_size > 0) {
        if (!db_format_last_backup_path(output_path, output_path_size))
            return false;
    }

    /* Future reads should treat file as modern. */
    if (migrated)
        *migrated = true;
    return true;
}

boolean db_format_last_backup_path(char *buffer, size_t length) {
    if (buffer == NULL || length == 0)
        return false;
    if (last_backup_path[0] == '\0') {
        buffer[0] = '\0';
        return false;
    }
    strncpy(buffer, last_backup_path, length);
    if (length > 0)
        buffer[length - 1] = '\0';
    return true;
}

void db_format_clear_last_backup_path(void) {
    last_backup_path[0] = '\0';
}

void db_format_force_strict_v7_reader(void) {
    g_legacy_adapter_active = false;
    g_legacy_adapter_force_repack = false;
    memset(&g_legacy_widened_header, 0, sizeof g_legacy_widened_header);
    db_format_mode mode = {true, false, false};
    db_format_mode_apply(&mode);
}
void db_format_mode_apply(const db_format_mode *mode) {
    g_mode_state = *mode;
    g_legacy_adapter_force_repack = mode->adapter_repack;
}

void db_format_mode_push(const db_format_mode *mode) {
    db_format_mode effective = {false, false, false};
    if (mode != NULL)
        effective = *mode;
    if (g_mode_depth < (int) (sizeof g_mode_stack / sizeof g_mode_stack[0]))
        g_mode_stack[g_mode_depth++] = effective;
    db_format_mode_apply(&effective);
}

void db_format_mode_pop(void) {
    if (g_mode_depth > 0)
        g_mode_depth--;
    if (g_mode_depth > 0)
        db_format_mode_apply(&g_mode_stack[g_mode_depth - 1]);
    else {
        db_format_mode reset = {false, false, false};
        db_format_mode_apply(&reset);
    }
}

db_format_mode db_format_mode_current(void) {
    if (g_mode_depth > 0)
        return g_mode_stack[g_mode_depth - 1];
    db_format_mode current = g_mode_state;
    current.adapter_repack = g_legacy_adapter_force_repack;
    return current;
}

void db_context_init(db_context *context) {
    if (context == NULL)
        return;
    context->mode = db_format_mode_current();
    context->database = databasedata;
    db_saveas_state_snapshot(&context->saveas);
}

void db_context_apply(const db_context *context) {
    if (context == NULL)
        return;
    db_format_mode_apply(&context->mode);
    if (context->database != nil)
        databasedata = context->database;
}

boolean hashpacktable_context(const db_context *context, hdlhashtable ht, boolean flsave, Handle *hpacked, boolean *flmustsave) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = hashpacktable(ht, flsave, hpacked, flmustsave);
    db_context_guard_exit(&guard);
    return ok;
}

boolean hashunpacktable_context(const db_context *context, Handle hpacked, boolean flmemory, hdlhashtable htable) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = hashunpacktable(hpacked, flmemory, htable);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbassignhandle_context(const db_context *context, Handle h, dbaddress *adr) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbassignhandle(h, adr);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbcopy_context(const db_context *context, dbaddress src, dbaddress *dest) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbcopy_internal(src, dest);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbassign_context(const db_context *context, dbaddress *padr, long newsize, ptrvoid pdata) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbassign_internal(padr, newsize, pdata);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbreference_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbreference_internal(adr, ctbytes, pdata);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbreference_handle_context(const db_context *context, dbaddress adr, Handle *h) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbrefhandle(adr, h);
    db_context_guard_exit(&guard);
    return ok;
}
