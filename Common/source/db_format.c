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
#include <ctype.h>
#include <errno.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "logging.h"  /* Phase 2D: fprintf migration */
#include "db_format.h"
#include "dbinternal.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "tablestructure.h"
#include "kernelverbs.h"  /* For targetinitverbs() declaration */
#include "tableinternal.h"
#include "threads.h"
#include "tableverbs.h"
#include "opverbs.h"
#include "wpverbs.h"
#include "pictverbs.h"
#include "menuverbs.h"
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
extern boolean dbinitverbs(void);  /* dbverbs.c - initialize Guest Database infrastructure */
extern boolean opinitverbs(void);  /* op processor verbs */
extern boolean opattributesinitverbs(void);  /* opattributes processor verbs */
#endif

// 2025-10-27 Codex: Added optional migration tracing to inspect v6/v7 table layouts during conversion.
// 2025-11-20 Codex: Added v7 header serializer and shared big-endian helpers to keep v7 roots portable.
// 2025-11-25 Codex: Implement legacy adapter widening + strict v7 reader entry points.
// 2025-11-30 Codex: Guard headless runtime tracking when portable builds skip UI hooks.

static _Thread_local boolean g_db_format_runtime_initialized = false;
#if !defined(FRONTIER_PORTABLE)
static boolean g_db_format_runtime_headless = false;
#endif
/* _Thread_local: safe under the GIL threading model (ADR-014) where all
 * callers read this value immediately after migrating in the same call stack.
 * If migration is ever performed on a worker thread with the result read on
 * another thread, this would need to change to a shared global with locking.
 *
 * Written ONLY by migrate_internal() — stores the v7 output path.
 * Callers should read it immediately after migrate_32bit_to_64bit() or
 * ensure_database_v7(). */
static _Thread_local char last_migration_output_path[1024];

/* Thread-local buffer holding the path to the most recent backup file.
 * Written ONLY by create_root_backup() — stores the timestamped backup path.
 * Callers should read it immediately after create_root_backup().
 *
 * _Thread_local: safe under the GIL threading model (ADR-014) where
 * create_root_backup() callers read this value immediately in the same
 * call stack. See last_migration_output_path comment for details. */
static _Thread_local char last_backup_output_path[1024];
static boolean g_legacy_adapter_active = false;
static boolean g_legacy_adapter_force_repack = false;
static boolean g_legacy_adapter_mode_locked = false; /* Prevents v7->v6 downgrades during migration */
static tydatabaserecord_64 g_legacy_widened_header;
static hdldatabaserecord g_legacy_source_db = nil;
static _Thread_local db_format_mode g_mode_stack[4];
static _Thread_local int g_mode_depth = 0;
/* 2026-01-29: g_mode_state is NOT thread-local because all threads share the same database format.
 * Worker threads (e.g., webserver accept thread) need to see the v7 mode set by the main thread.
 * The stack remains thread-local so different threads can push/pop without interference. */
static db_format_mode g_mode_state = {false, false}; /* current mode when stack is empty */

typedef struct db_context_guard {
    db_format_mode prev_mode;
    db_saveas_state prev_saveas;
    hdldatabaserecord prev_db;
} db_context_guard;

__attribute__((unused))
static void db_context_guard_enter(const db_context *context, db_context_guard *guard) {
    /* DEPRECATED: This function implements the guard pattern for backward compatibility.
     *
     * New code should use explicit context passing (langexternalpack_internal pattern).
     * The guard pattern still exists for legacy callers in db.c, but the refactored code
     * path (database layer, table packing, external variable handling) no longer uses it.
     *
     * See docs/mode_stack_refactor_learnings.md for architectural guidance.
     */
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

__attribute__((unused))
static void db_context_guard_exit(const db_context_guard *guard) {
    /* DEPRECATED: This function implements the guard pattern for backward compatibility.
     * See db_context_guard_enter() deprecation notice above.
     */
    if (guard == NULL)
        return;

    /* CRITICAL MIGRATION INVARIANT: During v6→v7 migration (adapter_active=1), we MUST NOT
     * restore the previous mode if it would downgrade from v7→v6. The migration process requires
     * stable v7 write mode throughout all operations.
     *
     * Why This Matters (see docs/mode_stack_refactor_learnings.md - "Four Address Spaces"):
     * 1. On-disk v6 addresses (32-bit LE) → Read from source database
     * 2. In-memory pointers (64-bit) → Loaded into memory during packing
     * 3. Expanded structures (64-bit padded) → Prepared for v7 format
     * 4. On-disk v7 addresses (64-bit BE) → Written to destination database
     *
     * If we restored v6 mode during migration, child operations would write data in v6 format
     * into the v7 destination database, causing format corruption. The adapter_enable_wide_writes()
     * call sets mode.use_64bit_format=true and locks it to prevent exactly this scenario.
     *
     * This guard exists for backward compatibility with legacy code that still uses the
     * deprecated db_context_guard pattern. It is NOT used by the refactored code path.
     */
#if defined(FRONTIER_HEADLESS)
    static int debug_count = 0;
    if (debug_count++ < 5) {
        log_trace(LOG_COMP_DB, "db_context_guard_exit: adapter_active=%d prev.use64=%d current.use64=%d",
                  (int) g_legacy_adapter_active,
                  (int) guard->prev_mode.use_64bit_format,
                  (int) g_mode_state.use_64bit_format);
    }
#endif
    if (g_legacy_adapter_active &&
        guard->prev_mode.use_64bit_format == false &&
        g_mode_state.use_64bit_format == true) {
        /* Keep the current v7 write mode instead of restoring v6 mode */
#if defined(FRONTIER_HEADLESS)
        static int warn_count = 0;
        if (warn_count++ < 3) {
            log_warn(LOG_COMP_DB, "db_context_guard_exit: NOT restoring prev mode (would downgrade v7->v6)");
        }
#endif
    } else {
        db_format_mode_apply(&guard->prev_mode);
    }
    databasedata = guard->prev_db;
    db_saveas_state_apply(&guard->prev_saveas);
}

/* ODB context guard for nested database operations (migration, etc.) */
void odb_guard_enter(odb_context_guard *guard) {
	if (guard == NULL)
		return;
	guard->saved_currenthashtable = (void *) currenthashtable;
	guard->saved_databasedata = databasedata;
	guard->saved_hashtablestack = (void *) hashtablestack;
	guard->saved_rootvariable = (void *) rootvariable;
	guard->saved_roottable = (void *) roottable;
	guard->saved_cancoonglobals = (void *) cancoonglobals;
	/* Save table structure globals that cleartablestructureglobals() clears.
	 * Without this, opening a guest database via odbOpenFile destroys the
	 * system root's pathstable, systemtable, builtinstable, etc. */
	guard->saved_systemtable = (void *) systemtable;
	guard->saved_builtinstable = (void *) builtinstable;
	guard->saved_pathstable = (void *) pathstable;
	guard->saved_verbstable = (void *) verbstable;
	guard->saved_iacgluetable = (void *) iacgluetable;
	guard->saved_iachandlertable = (void *) iachandlertable;
	guard->saved_resourcestable = (void *) resourcestable;
	guard->saved_agentstable = (void *) agentstable;
	guard->saved_menubartable = (void *) menubartable;
	guard->saved_objectmodeltable = (void *) objectmodeltable;

	/* Nil out currenthashtable to prevent tmpstack contamination.
	 * Without this, copyvaluerecord during migration pushes handles onto
	 * the caller's local scope tmpstack via pushtmpstackvalue. When those
	 * handles are later freed by the guest database, cleartmpstack tries
	 * to double-free them — heap corruption. Setting nil makes
	 * pushtmpstackvalue return early (langtmpstack.c line 103). */
	currenthashtable = nil;

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "odb_guard_enter: saved db=%p root=%p currenttable=%p (now nil)",
	          (void *) databasedata, (void *) rootvariable, (void *) guard->saved_currenthashtable);
#endif
}

void odb_guard_exit(odb_context_guard *guard) {
	if (guard == NULL)
		return;
#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "odb_guard_exit: restoring db=%p root=%p currenttable=%p",
	          (void *) guard->saved_databasedata, guard->saved_rootvariable,
	          guard->saved_currenthashtable);
#endif
	currenthashtable = (hdlhashtable) guard->saved_currenthashtable;
	databasedata = guard->saved_databasedata;
	hashtablestack = (hdltablestack) guard->saved_hashtablestack;
	rootvariable = (Handle) guard->saved_rootvariable;
	roottable = (hdlhashtable) guard->saved_roottable;
	cancoonglobals = (hdlcancoonrecord) guard->saved_cancoonglobals;
	/* Restore table structure globals */
	systemtable = (hdlhashtable) guard->saved_systemtable;
	builtinstable = (hdlhashtable) guard->saved_builtinstable;
	pathstable = (hdlhashtable) guard->saved_pathstable;
	verbstable = (hdlhashtable) guard->saved_verbstable;
	iacgluetable = (hdlhashtable) guard->saved_iacgluetable;
	iachandlertable = (hdlhashtable) guard->saved_iachandlertable;
	resourcestable = (hdlhashtable) guard->saved_resourcestable;
	agentstable = (hdlhashtable) guard->saved_agentstable;
	menubartable = (hdlhashtable) guard->saved_menubartable;
	objectmodeltable = (hdlhashtable) guard->saved_objectmodeltable;
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

#ifdef FRONTIER_HEADLESS
    /* ADR-005: Initialize thread-local parameter state after memory subsystem */
    extern void headless_init_threadglobals(void);
    headless_init_threadglobals();
#endif

    initstrings();

    if (!initlang())
        return false;

#ifdef FRONTIER_HEADLESS
    /* Initialize script compilation callbacks for JIT compilation */
    extern void headless_init_script_compiler(void);
    headless_init_script_compiler();
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: initialized headless script compiler");
#endif

    if (!inittablestructure())
        return false;

#ifdef FRONTIER_HEADLESS
    /* Install constants, built-in functions, and keywords before registering verbs */
    if (!langinitresources_headless())
        return false;
#endif

    if (!langinitverbs())
        return false;

#ifdef FRONTIER_HEADLESS
    /* Initialize target processor verbs (must be before headless_init_kernel_verbs) */
    if (!targetinitverbs())
        return false;

    /* Initialize all headless verb processors (auto-generated) */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: calling headless_init_kernel_verbs");
    if (!headless_init_kernel_verbs()) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: headless_init_kernel_verbs FAILED");
        return false;
    }
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: headless_init_kernel_verbs completed successfully");

    /* Initialize op processor with custom callback */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: calling opinitverbs");
    if (!opinitverbs()) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: opinitverbs FAILED");
        return false;
    }
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: opinitverbs completed successfully");

    /* Initialize opattributes processor with custom callback */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: calling opattributesinitverbs");
    if (!opattributesinitverbs()) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: opattributesinitverbs FAILED");
        return false;
    }
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: opattributesinitverbs completed successfully");

    /* Save reference to headless-created efptable before database loading */
    extern void save_headless_efptable(void);  /* Forward declaration */
    save_headless_efptable();
    log_debug(LOG_COMP_STARTUP, "db_format_prepare_runtime: saved headless efptable reference");

    /* Link system table structure to make processors accessible via system.compiler.kernel.* */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: linking system table structure");
    if (!linksystemtablestructure(roottable)) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: linksystemtablestructure FAILED");
        return false;
    }
    log_debug(LOG_COMP_STARTUP, "db_format_prepare_runtime: system table structure linked successfully");

    /* Resolve address values in system.paths from v6 migration (PR #336 fix) */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: resolving system.paths addresses");
    if (!resolve_system_paths(roottable)) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: resolve_system_paths FAILED");
        return false;
    }
    log_debug(LOG_COMP_STARTUP, "db_format_prepare_runtime: system.paths addresses resolved successfully");

    /* Populate system.paths with processor shortcuts for bare verb resolution */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: populating system.paths");
    if (!headless_init_system_paths(roottable)) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: headless_init_system_paths FAILED");
        return false;
    }
    log_debug(LOG_COMP_STARTUP, "db_format_prepare_runtime: system.paths populated successfully");

    /* Initialize db verb infrastructure (Guest Database linked list) */
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: calling dbinitverbs");
    if (!dbinitverbs()) {
        log_error(LOG_COMP_STARTUP, "db_format_prepare_runtime: dbinitverbs FAILED");
        return false;
    }
    log_trace(LOG_COMP_STARTUP, "db_format_prepare_runtime: dbinitverbs completed successfully");
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

#define db_trace_log(level, fmt, ...) \
    do { \
        if (db_trace_level() >= (level)) { \
            log_trace(LOG_COMP_DB, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

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

static void db_trace_log_v7_entries(db_trace_context *ctx,
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

static void db_trace_log_v7_entries(db_trace_context *ctx,
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

    db_trace_log(1, "%s: [%s] v7 table depth=%d entries=%zu strings=%zu",
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
        db_trace_log_v7_entries(ctx, payload, payload_len, path, depth);

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
boolean db_format_decode_header(const unsigned char *rawheader, size_t raw_len, boolean *header_is_v7, tydatabaserecord *out) {
    int i;
    int header_version = 0;
    size_t needed = 0;

    if ((rawheader == NULL) || (header_is_v7 == NULL) || (out == NULL))
        return false;

    if (!db_format_header_version(rawheader, raw_len, &header_version))
        return false;

    *header_is_v7 = false;
    memset(out, 0, sizeof *out);

    needed = (header_version >= 7) ? sizeof(tydatabaserecord_64) : sizeof(tydatabaserecord);
    if (raw_len < needed)
        return false;

    if (header_version >= 7) {
        *header_is_v7 = true;
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
            log_trace(LOG_COMP_DB, "decode v7 view[%d]=0x%016llx", i, (unsigned long long) view);
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
    db_format_mode legacy = {false, g_legacy_adapter_force_repack};
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
    if (db_format_is_v6_header(decoded_header))
        return false;

    header_len = decoded_header->headerLength;
    if (header_len <= 0)
        header_len = (long) sizeof (tydatabaserecord_64);
    if (header_len < (long) sizeof (tydatabaserecord_64))
        return false;

    db_format_mode modern = {true, g_legacy_adapter_force_repack};
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
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "db_format_adapter_enable_wide_writes: adapter_active=%d", (int) g_legacy_adapter_active);
#endif
    if (!g_legacy_adapter_active)
        return false;

    db_format_mode modern = {true, true};
    db_format_mode_apply(&modern);

    /* Lock the mode to prevent v7->v6 downgrades during migration */
    g_legacy_adapter_mode_locked = true;
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "db_format_adapter_enable_wide_writes: mode LOCKED (v7 writes enforced)");
#endif

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
    /*
    2025-12-20: Explicit context - NO GUARDS, NO SAVE/RESTORE
    Set mode directly from context, call function, done.
    Caller ensures correct database is active.
    */
    if (context != NULL) {
        if (context->database != nil)
            databasedata = context->database;
        db_format_mode_apply(&context->mode);
    }
    return db_format_adapter_enable_wide_writes(widened_header_out);
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

void db_format_adapter_reset(void) {
    g_legacy_adapter_active = false;
    g_legacy_adapter_force_repack = false;
    g_legacy_adapter_mode_locked = false;
    memset(&g_legacy_widened_header, 0, sizeof g_legacy_widened_header);
    g_legacy_source_db = nil;
}

void db_format_set_legacy_source_db(hdldatabaserecord hdb) {
    log_debug(LOG_COMP_DB, "db_format_set_legacy_source_db: setting g_legacy_source_db=%p", (void*)hdb);
    g_legacy_source_db = hdb;
}

boolean db_format_is_legacy_db(hdldatabaserecord hdb) {
    boolean result = (hdb != nil) && (hdb == g_legacy_source_db);
    log_debug(LOG_COMP_DB, "db_format_is_legacy_db: hdb=%p g_legacy=%p result=%d",
              (void*)hdb, (void*)g_legacy_source_db, result);
    return result;
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
        log_error(LOG_COMP_DB, "FATAL: tydatabaserecord_64.views offset=%zu expected=16",
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

/* Allow headroom for future format bumps without silently accepting garbage. */
#define DB_FORMAT_MAX_VERSION 10

boolean detect_database_format(const tydatabaserecord *header) {
	if (header == NULL)
		return false;

	/* Reject out-of-range version numbers before deciding legacy/v7. */
	if (header->versionnumber < 1 || header->versionnumber > DB_FORMAT_MAX_VERSION)
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

    last_backup_output_path[0] = '\0';

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
    strncpy(last_backup_output_path, backup_path, sizeof last_backup_output_path);
    last_backup_output_path[sizeof last_backup_output_path - 1] = '\0';
    return true;
}

/* During v6→v7 migration, update external object database handles to point to destination.
 * This fixes the issue where externals captured v6 source database handle during initial load,
 * but need to reference v7 destination database after migration.
 * See: planning/phase3/kernel_verb_porting/DATABASE_HANDLE_MISMATCH_CONFIRMED.md */
__attribute__((unused))
static void db_format_fixup_external_handles(hdlhashtable hroot, hdldatabaserecord dest_db) {
    if (hroot == nil || dest_db == nil)
        return;

    long ix = 0;
    hdlhashnode hnode = nil;
    long fixed_count = 0;

    while (hashgetnthnode(hroot, ix++, &hnode)) {
        if (hnode == nil)
            continue;

        tyvaluerecord *val = &(**hnode).val;
        if (val->valuetype != externalvaluetype)
            continue;

        hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;
        if (hv == nil)
            continue;

        hdldatabaserecord old_db = (**hv).hdatabase;

        /* Update database handle to point to destination */
        (**hv).hdatabase = dest_db;
        fixed_count++;

#if defined(FRONTIER_HEADLESS)
        if (fixed_count <= 10) {  /* Log first 10 to avoid spam */
            bigstring bsname;
            gethashkey(hnode, bsname);
            log_trace(LOG_COMP_DB, "migrate fixed external handle name='%.*s' id=%d flinmemory=%d old_db=%p new_db=%p",
                      (int) bsname[0], (char *) &bsname[1],
                      (int) (**hv).id, (int) (**hv).flinmemory,
                      (void*)old_db, (void*)dest_db);
        }
#endif

        /* Recurse into table externals to fix nested values */
        if ((**hv).id == idtableprocessor) {
            hdlhashtable childtable = (hdlhashtable) (**hv).variabledata;
            if (childtable != nil) {
                db_format_fixup_external_handles(childtable, dest_db);
            }
        }
    }

#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate fixed %ld external handles in total", fixed_count);
#endif
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

        /* Clear oldaddress on all in-memory externals to force new allocation during save */
        if ((**hv).flinmemory) {
            bigstring bsname;
            gethashkey(hnode, bsname);
            log_trace(LOG_COMP_DB, "migrate clearing oldaddress name='%.*s' id=%d",
                      (int) bsname[0], (char *) &bsname[1], (int) (**hv).id);
            (**hv).oldaddress = nildbaddress;

            /* Recurse into table externals to clear oldaddress on nested values */
            if ((**hv).id == idtableprocessor) {
                hdlhashtable childtable = (hdlhashtable) (**hv).variabledata;
                log_trace(LOG_COMP_DB, "migrate recursing into table '%.*s'",
                          (int) bsname[0], (char *) &bsname[1]);
                db_format_sanitize_root_externals(childtable, context);
            }
            continue;
        }

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
        log_warn(LOG_COMP_DB, "migrate dropping external name='%.*s' adr=0x%llx (free/unreadable)",
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

static boolean db_format_force_materialize_external_tables_recursive(
    hdlhashtable htable,
    const db_context *context,
    int depth
) {
#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_DB, "materialize_recursive: depth=%d htable=%p", depth, (void *)htable);
#endif

    if (htable == nil || depth > 50) { /* prevent infinite recursion */
#if defined(FRONTIER_HEADLESS)
        if (depth > 50)
            log_debug(LOG_COMP_DB, "  skipping: depth_limit_exceeded depth=%d", depth);
#endif
        return true;
    }

    long ix = 0;
    hdlhashnode hnode = nil;
    long node_count = 0;
    long external_count = 0;
    long materialized_count = 0;

    while (hashgetnthnode(htable, ix++, &hnode)) {
        if (hnode == nil)
            continue;

        node_count++;
        bigstring bsname;
        gethashkey(hnode, bsname);

        tyvaluerecord *val = &(**hnode).val;
#if defined(FRONTIER_HEADLESS)
        log_debug(LOG_COMP_DB, "  node[%ld] name='%.*s' valuetype=%d",
                  node_count, (int) bsname[0], (char *) &bsname[1], (int) val->valuetype);
#endif

        if (val->valuetype != externalvaluetype) {
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "    skip: not external valuetype");
#endif
            continue;
        }

        hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;
        if (hv == nil) {
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "    skip: hv is nil");
#endif
            continue;
        }

        int var_id = (**hv).id;
        external_count++;
        dbaddress v6_adr = (dbaddress) (**hv).variabledata;
        boolean was_in_memory = (**hv).flinmemory;

#if defined(FRONTIER_HEADLESS)
        log_debug(LOG_COMP_DB, "MATERIALIZE: name='%.*s' id=%d v6_adr=0x%llx was_in_memory=%d depth=%d",
                  (int) bsname[0], (char *) &bsname[1], var_id, (unsigned long long) v6_adr, (int) was_in_memory, depth);
#endif

        /* Load into memory if not already loaded */
        if (!was_in_memory) {
            boolean loaded = false;

            /* Call appropriate verbinmemory function based on external type */
            if (var_id == idtableprocessor) {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    calling tableverbinmemory for '%.*s'",
                          (int) bsname[0], (char *) &bsname[1]);
#endif
                loaded = tableverbinmemory(context, hv, hnode);
            } else if (var_id == idoutlineprocessor || var_id == idscriptprocessor) {
                /* Scripts and outlines share the same infrastructure (both use hdloutlinerecord).
                 * idscriptprocessor is just an outline with (**hv).flscript = true flag set. */
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    calling opverbinmemory for '%.*s' (id=%d %s)",
                          (int) bsname[0], (char *) &bsname[1], var_id,
                          var_id == idscriptprocessor ? "script" : "outline");
#endif
                loaded = opverbinmemory(context, hv);
            } else if (var_id == idwordprocessor) {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    leaf external (wptext) - will clear oldaddress without loading (memory optimization)");
#endif
                /* WPText externals: Use deferred loading strategy (critical memory optimization).
                 *
                 * Strategy: Clear oldaddress without loading, then load on-demand during packing.
                 *
                 * Rationale: WPText objects can number in the hundreds/thousands in production databases.
                 * Loading all WPText objects upfront during materialization would:
                 * - Exhaust available memory (each WPText has Paige structures, styles, fonts)
                 * - Cause migration to hang for minutes on large databases
                 * - Load objects that may never be packed (if migration is selective)
                 *
                 * The deferred approach:
                 * - Clears oldaddress to force fresh v7 allocation during packing
                 * - Packing code calls ensure_external_in_memory() which triggers
                 *   wpverbinmemory() for on-demand loading from source database
                 * - Only loads WPText objects that are actually being packed
                 *
                 * Error Handling: If a WPText object is corrupt or fails to load during packing:
                 * - The packing operation will fail and return an error
                 * - Migration will abort with clear error message indicating which object failed
                 * - User can investigate the corrupt object in the source database
                 * - This is safer than silently skipping corrupt objects
                 *
                 * Validation: Integration tests confirm this approach works with databases
                 * containing hundreds of WPText objects. See migration test suite results.
                 *
                 * Alternative: Could call wpverbinmemory() here to load immediately, but this
                 * causes memory exhaustion and hangs on real-world databases (tested). */
                loaded = true;  /* Treated as success - we'll handle via oldaddress clearing */
            } else if (var_id == idpictprocessor) {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    calling pictverbinmemory for '%.*s' hv=%p",
                          (int) bsname[0], (char *) &bsname[1], (void*)hv);
#endif
                loaded = pictverbinmemory(context, hv);
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    pictverbinmemory returned: %d", loaded);
#endif
            } else if (var_id == idmenuprocessor) {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    calling menuverbinmemory_context for '%.*s' hv=%p",
                          (int) bsname[0], (char *) &bsname[1], (void*)hv);
#endif
                loaded = menuverbinmemory_context(context, hv);
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    menuverbinmemory_context returned: %d", loaded);
#endif
            } else {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    skip: unsupported external type id=%d", var_id);
#endif
                /* For other unsupported types, skip entirely */
                continue;
            }

            if (!loaded) {
#if defined(FRONTIER_HEADLESS)
                log_error(LOG_COMP_DB, "    ERROR: verbinmemory failed for '%.*s' id=%d",
                          (int) bsname[0], (char *) &bsname[1], var_id);
#endif
                return false;
            }

            /* Verify it's now in memory (except for wptext which we intentionally don't load) */
            if (var_id != idwordprocessor) {
#if defined(FRONTIER_HEADLESS)
                log_debug(LOG_COMP_DB, "    verbinmemory succeeded, checking flinmemory");
#endif

                if (!(**hv).flinmemory) {
#if defined(FRONTIER_HEADLESS)
                    log_error(LOG_COMP_DB, "    ERROR: flinmemory not set after verbinmemory for '%.*s'",
                              (int) bsname[0], (char *) &bsname[1]);
#endif
                    return false;
                }

                materialized_count++;
            }
        } else {
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "    already in memory, not materializing");
#endif
        }

        /* Clear oldaddress to force new allocation in v7 (for all externals, loaded or not) */
#if defined(FRONTIER_HEADLESS)
        dbaddress old_oldaddr = (**hv).oldaddress;
#endif
        (**hv).oldaddress = nildbaddress;

#if defined(FRONTIER_HEADLESS)
        log_debug(LOG_COMP_DB, "CLEARED oldaddress: name='%.*s' type=%d was=0x%llx now=nil depth=%d",
                  (int) bsname[0], (char *) &bsname[1], var_id,
                  (unsigned long long) old_oldaddr, depth);
#endif

        /* Recurse into newly-loaded table (only for table externals, not pictures/outlines/etc.) */
        if (var_id == idtableprocessor) {
            hdlhashtable child = (hdlhashtable) (**hv).variabledata;
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "    recursing into child table '%.*s' depth_next=%d child=%p",
                      (int) bsname[0], (char *) &bsname[1], depth + 1, (void *)child);
#endif

            if (!db_format_force_materialize_external_tables_recursive(
                    child, context, depth + 1))
                return false;
        } else {
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "    not recursing - external type %d is a leaf node", var_id);
#endif
        }
    }

#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_DB, "materialize_recursive: depth=%d complete: nodes=%ld externals=%ld materialized=%ld",
              depth, node_count, external_count, materialized_count);
#endif

    return true;
}

static boolean db_format_force_materialize_external_tables(
    hdlhashtable hroot,
    const db_context *context
) {
    return db_format_force_materialize_external_tables_recursive(
        hroot, context, 0);
}

/* Helper: Clean up migration database handles to ensure proper disposal and nil-setting.
 * Addresses PR #137 issue - prevents double-free by handling three disposal scenarios:
 * 1. Success path already called dbendsaveas*() - nothing to do (fldatabasesaveas=false)
 * 2. Error path with active Save As - call dbendsaveas*() to teardown
 * 3. Error path with allocated destination but no active Save As - direct disposal
 *
 * NOTE: This is part of addressing Issue #138 - making disposal patterns explicit.
 * Related to Issue #135/#136 - eliminating push/pop anti-patterns in favor of
 * deterministic, explicit cleanup. */
static void cleanup_migration_database(db_context *dest_context, boolean have_dest_context) {
    if (fldatabasesaveas) {
        /* Error path: Save As is active, need to teardown partial state.
         * dbendsaveas*() calls dbdispose() internally and sets fldatabasesaveas = false.
         * Return value ignored: we're in cleanup/error handling, disposal is best-effort. */
        if (have_dest_context) {
            dbendsaveas_context(dest_context);
        } else {
            dbendsaveas();
        }
        databasedata = nil;
    } else if (databasedata != nil) {
        /* Rare error path: destination allocated but Save As not started yet.
         * Example scenario:
         * 1. dbstartsaveas_context() succeeds → databasedata = destination
         * 2. Early validation fails (e.g., source corrupt) → goto cleanup
         * 3. fldatabasesaveas still false, but databasedata needs disposal */
        dbdispose();
        databasedata = nil;
    }
    /* else: databasedata already nil (normal success path), nothing to do */
}

static boolean migrate_internal(const char *db_path, const char *explicit_output) {
    if (db_path == NULL || db_path[0] == '\0')
        return false;

    /* Save ALL database globals before nested dbopenfile() corrupts them.
     * migrate_internal() calls dbopenfile() at line 1812, which creates a new
     * databasedata handle and modifies global state. Without this guard, the
     * caller's database context gets corrupted, causing crashes or hangs.
     * See Issue: Auto-migration crash due to nested database opens. */
    odb_context_guard caller_guard;
    odb_guard_enter(&caller_guard);

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
    char backup_path[1024];  /* v6 backup: e.g., Frontier.v6.root */
    char temp_path[1024];
    temp_path[0] = '\0';
    backup_path[0] = '\0';
    bigstring bspath;
    bigstring bsdst;
    tyfilespec src_fs;
    tyfilespec dst_fs;
    const char *fail_step = "init";
    const long header_len_final = (long) sizeof(tydatabaserecord_64);

#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate start path=%s", db_path);
#endif

    db_saveas_state_snapshot(&entry_saveas);

    /* Force legacy read mode while pulling from the v6 source; we restore at cleanup. */
    db_format_mode legacy_mode = {false, false};
    db_format_mode_push(&legacy_mode);

    fail_step = "prepare runtime";
    if (!db_format_prepare_runtime())
        goto cleanup;
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate after prepare runtime");
#endif

    if (db_trace_level() > 0)
        db_format_trace_database_path(db_path);

    /* Derive output path and v6 backup path.
     *
     * When explicit_output is given (--output flag), the v6 input is never
     * modified — v7 output is written to a temp file next to the explicit
     * output path and renamed into place.  backup_path stays empty so the
     * rename-v6-to-backup step is skipped.
     *
     * Default (in-place) pattern:
     *   Frontier.root (v6) -> Frontier.v6.root (backup) + Frontier.root (v7)
     * Safety: write v7 to temp first, THEN rename v6, THEN rename temp. */
    if (explicit_output != NULL) {
        /* Explicit output: no backup, write directly to caller's path */
        snprintf(output_path, sizeof output_path, "%s", explicit_output);
        backup_path[0] = '\0';  /* no v6 backup needed */
    } else {
        /* In-place migration: derive backup path via shared helper */
        if (!db_format_derive_v6_backup_path(db_path, backup_path, sizeof backup_path)) {
            fail_step = "backup path derivation (path too long)";
            ok = false;
            goto cleanup;
        }

        const char *ext = strrchr(db_path, '.');
        if (ext && strcmp(ext, ".root") == 0) {
            /* Input has .root extension: output = original path */
            snprintf(output_path, sizeof output_path, "%s", db_path);
        } else {
            /* No .root extension: output = <path>.root */
            snprintf(output_path, sizeof output_path, "%s.root", db_path);
        }
    }

    strncpy(last_migration_output_path, output_path, sizeof last_migration_output_path);
    if (sizeof last_migration_output_path > 0)
        last_migration_output_path[sizeof last_migration_output_path - 1] = '\0';

    snprintf(temp_path, sizeof temp_path, "%s.v7.tmp", output_path);

    copyctopstring(db_path, bspath);
    fail_step = "pathtofilespec(src)";
    if (!pathtofilespec(bspath, &src_fs))
        goto cleanup;

    fail_step = "openfile(src)";
    if (!openfile(&src_fs, &src_fnum, true))
        goto cleanup;

#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate: opened SOURCE file fnum=%d", (int)src_fnum);
#endif

    fail_step = "dbopenfile(src)";
    if (!dbopenfile(src_fnum, true))
        goto cleanup;

    db_context_init(&source_context);
    source_context.mode = db_format_mode_current();
    source_context.database = databasedata;

#if defined(FRONTIER_HEADLESS)
    if (databasedata) {
        log_trace(LOG_COMP_DB, "migrate: source database handle fnum=%ld flreadonly=%d",
                  (long)(**databasedata).fnumdatabase,
                  (int)(**databasedata).u.extensions.flreadonly);
    }
#endif

    db_context_init(&dest_context);
    /* Use v7 modern format for destination, not source's legacy v6 format.
     * Fixes Issue #123: prevents writing v4 table headers into v7 database. */
    db_format_mode modern_mode = {true, false};  /* use_64bit_format=true */
    dest_context.mode = modern_mode;
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

    /* Apply adapter_repack mode globally before materialization so tableverbinmemory clears oldaddress.
     * This ensures external table variables will have oldaddress=nil after being loaded into memory,
     * forcing fresh allocation in v7 format instead of reusing v6 addresses.
     * Keep use_64bit_format=true even though we're reading v6 data - the mode guard will force it anyway. */
    db_format_mode materialize_mode = source_context.mode;
    materialize_mode.use_64bit_format = true;  /* Force v7 mode for consistency */
    materialize_mode.adapter_repack = true;     /* Clear oldaddress during materialization */
    db_format_mode_apply(&materialize_mode);

    fail_step = "force_materialize_external_tables(root)";
    if (!db_format_force_materialize_external_tables(hroot, &source_context))
        goto cleanup;

    /* CRITICAL FIX for guest database migration address reuse bug:
     * The root table is already loaded into memory by tableloadsystemtable() above,
     * so tableverbinmemory() returns early without clearing oldaddress.
     * We must explicitly clear oldaddress here AFTER loading to force fresh allocation
     * in the destination database. The adapter_repack mode above ensures nested tables
     * also get fresh addresses when they are loaded. */
    {
        hdltablevariable hv = (hdltablevariable) hrootvariable;
        hdlhashtable ht = (hdlhashtable) (**hv).variabledata;
        if (ht != nil) {
            (**ht).fldirty = true;
            (**ht).flsubsdirty = true;
        }
        /* Force fresh allocation: root is already in memory, so clear oldaddress now */
        if (hv != nil && *hv != nil) {
            (**hv).oldaddress = nildbaddress;
#if defined(FRONTIER_HEADLESS)
            log_debug(LOG_COMP_DB, "migrate: cleared root oldaddress=0x%llx to nildbaddress, flinmemory=%d",
                      (unsigned long long)(**hv).oldaddress,
                      (int)(**hv).flinmemory);
#endif
        }
    }
    /* Load root into memory - should be a no-op since tableloadsystemtable() already loaded it */
    fail_step = "tableverbinmemory(root)";
    if (!tableverbinmemory(&source_context, (hdlexternalvariable) hrootvariable, HNoNode))
        goto cleanup;
#if defined(FRONTIER_HEADLESS)
    {
        hdltablevariable hv = (hdltablevariable) hrootvariable;
        log_debug(LOG_COMP_DB, "migrate: after tableverbinmemory, oldaddress=0x%llx flinmemory=%d",
                  (unsigned long long)(**hv).oldaddress,
                  (int)(**hv).flinmemory);
    }
#endif

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

#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate: opened DESTINATION file fnum=%d (source was %d)",
              (int)dst_fnum, (int)src_fnum);
#endif

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

#if defined(FRONTIER_HEADLESS)
    if (dest_context.database) {
        log_trace(LOG_COMP_DB, "migrate: destination database handle fnum=%ld flreadonly=%d",
                  (long)(**dest_context.database).fnumdatabase,
                  (int)(**dest_context.database).u.extensions.flreadonly);
    }
#endif

    dest_context.mode = source_context.mode;
    dest_context.mode.use_64bit_format = true;
    dest_context.mode.adapter_repack = true;  /* Force repack during migration to ensure nested tables are saved */
    have_dest_context = true;

    /* Switch the destination into BE64 write mode before any assigns.
     * Call the non-context version directly so the mode persists globally. */
    db_format_adapter_enable_wide_writes(NULL);

    fail_step = "tablesavesystemtable(root)";
    /*
     * NOTE: External database handles are NOT updated here because WP packing
     * needs to READ Paige data from SOURCE while WRITING RTF to DESTINATION.
     * The fixup happens AFTER packing completes.
     */
    /*
    2025-12-20: NO GUARDS - set mode explicitly for table save
    Apply destination mode directly for v7 writes during migration
    */
    db_context save_ctx = source_context;
    if (have_dest_context) {
        save_ctx.mode = dest_context.mode; /* write v7 BE64 payloads into the destination */
        save_ctx.saveas = dest_context.saveas;
        save_ctx.database = dest_context.database;
#if defined(FRONTIER_HEADLESS)
        log_trace(LOG_COMP_DB, "migrate save_ctx.mode use64=%d adapter=%d dest_db=%p src_db=%p",
                  save_ctx.mode.use_64bit_format ? 1 : 0,
                  save_ctx.mode.adapter_repack ? 1 : 0,
                  (void *) save_ctx.saveas.destination,
                  (void *) save_ctx.saveas.source);
#endif
    }

    /* Set mode directly - NO GUARD, NO SAVE/RESTORE */
    if (have_dest_context) {
        if (save_ctx.database != nil)
            databasedata = save_ctx.database;
        /* Clear mode stack before applying v7 mode to prevent stacked v6 mode from overriding */
        g_mode_depth = 0;
        db_format_mode_apply(&save_ctx.mode);
        db_saveas_state_apply(&save_ctx.saveas);
    }

#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate mode applied (NO GUARD) use64=%d adapter=%d depth=%d current_db=%p",
              db_format_mode_current().use_64bit_format ? 1 : 0,
              db_format_mode_current().adapter_repack ? 1 : 0,
              g_mode_depth,
              (void *) databasedata);
#endif

    saved_root = tablesavesystemtable(hrootvariable, &new_root_address);
#if defined(FRONTIER_HEADLESS)
    log_debug(LOG_COMP_DB, "migrate: after tablesavesystemtable, new_root_address=0x%llx saved_root=%d",
              (unsigned long long)new_root_address, saved_root ? 1 : 0);
#endif
    if (!saved_root) {
        goto cleanup;
    }
    /* Ensure subsequent opens don't reuse the in-memory system table. */
    cleartablestructureglobals();
    if (hrootvariable != nil) {
        /* false => dispose contents; handle freed below via cleartablestructureglobals */
        tableverbdispose((hdlexternalvariable) hrootvariable, false);
        hrootvariable = nil;
    }
    if (have_dest_context && dest_context.database != nil) {
        long eof = 0;
        filegeteof((hdlfilenum) (**dest_context.database).fnumdatabase, &eof);
        log_trace(LOG_COMP_DB, "migrate write checkpoint fnum=%ld eof=%ld",
                  (long) (**dest_context.database).fnumdatabase, eof);
    }
    /* 2025-12-20: NO GUARD EXIT - mode remains as set for subsequent operations */

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

    /* v7 migration always drops legacy Cancoon and points view0 at the root table. */
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "migrate: setting view[%d] to new_root_address=0x%llx",
              cancoonview, (unsigned long long)new_root_address);
#endif
    for (int i = 0; i < ctviews; ++i)
        dbsetview(i, nildbaddress);
    dbsetview(cancoonview, new_root_address);
    new_cancoon_address = nildbaddress;

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
        log_trace(LOG_COMP_DB, "saveas pre-close dest=%p master=%p source=%p",
                  (void *) dest_context.saveas.destination,
                  validhandle((Handle) dest_context.saveas.destination) ? (void *) (*dest_context.saveas.destination) : NULL,
                  (void *) dest_context.saveas.source);
    }
#endif

    fail_step = "dbendsaveas";
    if (have_dest_context) {
        if (!dbendsaveas_context(&dest_context))
            goto cleanup;
        /* dbendsaveas_context already disposed the destination database.
         * Set databasedata to nil to prevent double-free in cleanup. */
        databasedata = nil;
    } else {
        if (!dbendsaveas())
            goto cleanup;
        databasedata = nil;
    }

    closefile(dst_fnum);
    dst_fnum = 0;

    /* Atomic-ish placement: rename v6 original to .v6.root backup, then
     * rename the completed temp file to the original path.  If the v6
     * rename fails (e.g., output_path == db_path and the file is locked),
     * we still have the temp and the untouched original.  If the temp
     * rename fails we leave the .v6.root backup so nothing is lost. */
    fail_step = "rename(v6->backup)";
    if (backup_path[0] != '\0' && strcmp(output_path, db_path) == 0) {
        /* Check if backup already exists — refuse to overwrite a valid v6 original */
        FILE *fp_existing = fopen(backup_path, "rb");
        if (fp_existing) {
            tydatabaserecord existing_hdr;
            boolean hdr_ok = fread(&existing_hdr, sizeof existing_hdr, 1, fp_existing) == 1;
            fclose(fp_existing);
            if (hdr_ok && db_format_is_v6_header(&existing_hdr)) {
                log_error(LOG_COMP_DB, "migrate_internal: refusing to overwrite existing v6 backup: %s", backup_path);
                fail_step = "backup already exists";
                ok = false;
                goto cleanup;
            }
            if (!hdr_ok) {
                log_error(LOG_COMP_DB, "migrate_internal: cannot read header of existing backup %s — refusing to overwrite (may contain valid v6 data)", backup_path);
                fail_step = "backup header unreadable";
                ok = false;
                goto cleanup;
            }
            /* Existing backup is v7 — safe to overwrite */
            log_warn(LOG_COMP_DB, "migrate_internal: overwriting non-v6 backup: %s", backup_path);
        }
        /* Only rename original when output overwrites input */
        if (rename(db_path, backup_path) != 0) {
            log_error(LOG_COMP_DB, "migrate_internal: rename v6 to backup failed: %s -> %s: %s",
                      db_path, backup_path, strerror(errno));
            fail_step = "rename(v6->backup)";
            ok = false;
            goto cleanup;
        }
    }

    fail_step = "rename(tmp->final)";
    if (rename(temp_path, output_path) != 0) {
        /* Try to restore the v6 original if the final rename fails */
        if (backup_path[0] != '\0' && strcmp(output_path, db_path) == 0) {
            if (rename(backup_path, db_path) != 0) {
                /* Both the temp→final rename AND the best-effort v6 restore
                 * failed.  The goto cleanup below will call remove(temp_path)
                 * (line ~2296), so the temp file is still cleaned up. */
                log_error(LOG_COMP_DB,
                    "migrate_internal: CRITICAL — could not restore v6 backup. "
                    "v6 data is at: %s, v7 temp data is at: %s. "
                    "To recover: rename %s to %s",
                    backup_path, temp_path, backup_path, db_path);
            }
        }
        goto cleanup;
    }

    dbaddress view_for_log = new_root_address;

    if (db_trace_level() > 0)
        db_format_trace_database_path(output_path);
    log_trace(LOG_COMP_DB, "migrate ok view0=0x%llx new_root=0x%llx new_script=0x%llx header_len=%ld outfile=%s",
              (unsigned long long) view_for_log,
              (unsigned long long) new_root_address,
              (unsigned long long) new_script_address,
              header_len_final,
              output_path);

    ok = true;

    /* Postcondition check: Success path should have nil'd databasedata */
    assert(databasedata == nil);

cleanup:
    db_format_mode_pop(); /* restore prior mode before exit */
    if (hscript != nil)
        disposehandle(hscript);
    if (hrootvariable != nil)
        tableverbdispose((hdlexternalvariable) hrootvariable, true);

    /* Database cleanup: Handle disposal in all three scenarios (see helper for details).
     * This helper function was extracted to improve testability and make the complex
     * cleanup logic easier to reason about (PR #137 review feedback). */
    cleanup_migration_database(&dest_context, have_dest_context);

    if (src_fnum != 0)
        closefile(src_fnum);

    if (!ok) {
        if (dst_fnum != 0)
            closefile(dst_fnum);
        if (temp_path[0] != '\0')
            remove(temp_path);
    }

    if (!ok) {
        log_error(LOG_COMP_DB, "migrate fail at %s view=0x%llx root=0x%llx new_root=0x%llx script=0x%llx new_script=0x%llx cancoon=0x%llx new_cancoon=0x%llx tmp=%s",
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
        log_trace(LOG_COMP_DB, "migrate ok view=0x%llx root=0x%llx new_root=0x%llx new_script=0x%llx outfile=%s",
                  (unsigned long long) view_address,
                  (unsigned long long) root_address,
                  (unsigned long long) new_root_address,
                  (unsigned long long) new_script_address,
                  output_path);
    }

    db_format_mode_apply(&entry_mode);
    db_saveas_state_apply(&entry_saveas);

    db_format_adapter_reset(); /* Clear mode lock so subsequent v6 guest DB opens work */

    /* Postcondition: databasedata should be nil after cleanup */
    assert(databasedata == nil);

    /* Restore caller's database globals.
     * This ensures that when db.open() calls migrate_internal(), the caller's
     * database context is restored exactly as it was before migration started.
     * Without this, the caller sees corrupted globals and crashes/hangs. */
    odb_guard_exit(&caller_guard);

    return ok;
}

/* ---- Migration lock file helpers (issue #271: TOCTOU race prevention) ---- */

#if !defined(_WIN32)

#define MIGRATION_LOCK_SUFFIX ".migrating"
#define MIGRATION_LOCK_STALE_SECONDS (5 * 60)  /* 5 minutes */
#define MIGRATION_LOCK_POLL_MS 500
#define MIGRATION_LOCK_TIMEOUT_MS 30000

/*  migration_lock_acquire -- atomically create lock file for migration.
 *
 *  Returns the fd (>= 0) on success.
 *  Returns -1 if another process holds the lock (caller should wait/retry).
 *  Returns -2 on unexpected error. */

static int migration_lock_acquire(const char *db_path, char *lock_path, size_t lock_path_size) {

    int n = snprintf(lock_path, lock_path_size, "%s%s", db_path, MIGRATION_LOCK_SUFFIX);
    if (n < 0 || (size_t)n >= lock_path_size)
        return -2; /* path too long */

    /* Try atomic creation. */
    int fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0600);

    if (fd >= 0)
        return fd;  /* Lock acquired. */

    if (errno != EEXIST)
        return -2;  /* Unexpected error. */

    /* Lock file exists -- check if it is stale (older than 5 minutes). */
    struct stat st;

    if (stat(lock_path, &st) == 0) {
        time_t now = time(NULL);

        if (now - st.st_mtime > MIGRATION_LOCK_STALE_SECONDS) {
#if defined(FRONTIER_HEADLESS)
            log_warn(LOG_COMP_DB,
                "migration_lock_acquire: removing stale lock file %s (age %ld seconds)",
                lock_path, (long)(now - st.st_mtime));
#endif
            unlink(lock_path);

            /* Retry once after removing stale lock. */
            fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, 0600);

            if (fd >= 0)
                return fd;

            if (errno != EEXIST)
                return -2;
        }
    }

    return -1;  /* Another process holds the lock. */
}

/*  migration_lock_release -- remove the lock file and close the fd. */

static void migration_lock_release(int fd, const char *lock_path) {

    if (fd >= 0)
        close(fd);

    if (lock_path && lock_path[0] != '\0')
        unlink(lock_path);
}

/*  migration_lock_wait -- wait for another process to finish migration.
 *
 *  Polls every 500ms for up to 30 seconds.
 *  Returns true if the lock was released within the timeout.
 *
 *  GIL note: this may block while holding the GIL, but it only runs
 *  during db.open() (not during script evaluation). In the headless
 *  build, db.open is typically called during startup before any
 *  concurrent scripts are running. The inter-process race this guards
 *  against (two processes migrating the same file) is rare. */

static boolean migration_lock_wait(const char *lock_path) {

    int elapsed_ms = 0;

    while (elapsed_ms < MIGRATION_LOCK_TIMEOUT_MS) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = MIGRATION_LOCK_POLL_MS * 1000000L;
        nanosleep(&ts, NULL);

        elapsed_ms += MIGRATION_LOCK_POLL_MS;

        /* Check if the lock file has been removed. */
        if (access(lock_path, F_OK) != 0)
            return true;  /* Lock released -- migration completed. */
    }

    return false;  /* Timeout. */
}

#endif /* !_WIN32 */

boolean migrate_32bit_to_64bit(const char *db_path) {
    return migrate_internal(db_path, NULL);
}

boolean migrate_32bit_to_64bit_to_output(const char *db_path, const char *output) {
    return migrate_internal(db_path, output);
}

boolean ensure_database_v7(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size) {
    boolean did_crash_recovery = false;

    if (migrated)
        *migrated = false;
    if (db_path == NULL || db_path[0] == '\0')
        return false;

    /* Endianness note for raw versionnumber comparisons in this function:
     * v6 headers store versionnumber as LE 0x0006 (reads as 6 on LE hosts).
     * v7 headers store it as BE 0x0007 (reads as 0x0700 = 1792 on LE hosts).
     * So ">= 7" correctly identifies v7, and "< 7" correctly identifies v6,
     * on little-endian platforms (macOS, Linux). Same pattern as migrate_internal. */

    /* Check if a .v6.root backup exists from a previous migration.
     * If "Frontier.v6.root" exists alongside "Frontier.root", the .root file
     * is already v7 from a prior migration -- use it directly. */
    char v6_backup_path[1024];
    if (!db_format_derive_v6_backup_path(db_path, v6_backup_path, sizeof v6_backup_path))
        return false;

    FILE *fp_v6_backup = fopen(v6_backup_path, "rb");
    if (fp_v6_backup) {
        /* Read the backup header now — we need it for crash recovery below,
         * and this eliminates a redundant second fopen of the same file. */
        tydatabaserecord backup_header;
        boolean backup_hdr_ok = fread(&backup_header, sizeof backup_header, 1, fp_v6_backup) == 1;
        fclose(fp_v6_backup);

        /* .v6.root backup exists -- verify .root actually contains valid v7 data.
         * A crash between the v6 rename and temp-to-final rename could leave
         * .root as v6 or missing; in that case, fall through to re-migrate. */
        FILE *fp_check = fopen(db_path, "rb");
        if (fp_check) {
            /* Read with byte-count to handle v7 files smaller than sizeof(tydatabaserecord).
             * Issue #264: v7 databases may be only 90 bytes (sizeof(tydatabaserecord_64)). */
            tydatabaserecord header_check;
            memset(&header_check, 0, sizeof header_check);
            size_t check_bytes = fread(&header_check, 1, sizeof header_check, fp_check);
            fclose(fp_check);
            if (check_bytes >= sizeof(tydatabaserecord_64) && !db_format_is_v6_header(&header_check)) {
                /* Confirmed v7 -- return db_path as the output */
                if (output_path && output_path_size > 0) {
                    strncpy(output_path, db_path, output_path_size);
                    if (output_path_size > 0)
                        output_path[output_path_size - 1] = '\0';
                }
                if (migrated)
                    *migrated = false;
                return true;
            }
        }
        /* .root is missing or not valid v7 — verify the backup is actually v6
         * before restoring it.  This handles crash recovery: if the process died
         * between renaming v6->backup and placing the v7 temp file, the .root
         * file may be absent or truncated.
         * Uses backup_header read above to avoid reopening the file. */
        if (!backup_hdr_ok || !db_format_is_v6_header(&backup_header)) {
            /* Backup is not a valid v6 database — don't restore, just fall through */
#if defined(FRONTIER_HEADLESS)
            log_warn(LOG_COMP_DB,
                "ensure_database_v7: .v6.root backup is not v6 (version=%d), skipping restore",
                backup_hdr_ok ? (int)backup_header.versionnumber : -1);
#endif
            /* Fall through to migration which will fail gracefully */
        } else {
            /* Backup is valid v6 — restore it */
            if (rename(v6_backup_path, db_path) == 0) {
#if defined(FRONTIER_HEADLESS)
                log_info(LOG_COMP_DB,
                    "ensure_database_v7: restored v6 backup %s to %s after incomplete migration",
                    v6_backup_path, db_path);
#endif
                did_crash_recovery = true;
                /* Fall through to re-migrate below */
            } else {
#if defined(FRONTIER_HEADLESS)
                log_error(LOG_COMP_DB,
                    "ensure_database_v7: cannot restore v6 backup %s to %s: %s. "
                    "To recover manually, rename %s to %s",
                    v6_backup_path, db_path, strerror(errno),
                    v6_backup_path, db_path);
#endif
                return false;
            }
        }
    }

    /* Skip .root7 fallback after crash recovery — we just restored the v6
     * original and need to re-migrate, not use a stale .root7 file. */
    if (!did_crash_recovery) {
        /* Transitional fallback: check for legacy .root7 file from previous migration.
         * TODO (2026-03-22): Remove this fallback once all users have migrated away from .root7.
         * Note: advisory log only fires in headless builds; non-headless users get no warning.
         * When adding GUI support, surface this via the GUI notification channel. */
        char legacy_root7_path[1024];
        snprintf(legacy_root7_path, sizeof legacy_root7_path, "%s7", db_path);
        FILE *fp_legacy = fopen(legacy_root7_path, "rb");
        if (fp_legacy) {
            /* Read with byte-count to handle v7 files smaller than sizeof(tydatabaserecord).
             * Issue #264: v7 databases may be only 90 bytes (sizeof(tydatabaserecord_64)). */
            tydatabaserecord header_v7;
            memset(&header_v7, 0, sizeof header_v7);
            size_t legacy_bytes = fread(&header_v7, 1, sizeof header_v7, fp_legacy);
            fclose(fp_legacy);
            if (legacy_bytes >= sizeof(tydatabaserecord_64) && !db_format_is_v6_header(&header_v7)) {
                /* Legacy .root7 file exists and is valid v7 */
#if defined(FRONTIER_HEADLESS)
                log_info(LOG_COMP_DB,
                    "ensure_database_v7: found legacy .root7 file: %s -- "
                    "please rename to %s for forward compatibility",
                    legacy_root7_path, db_path);
#endif
                if (output_path && output_path_size > 0) {
                    strncpy(output_path, legacy_root7_path, output_path_size);
                    if (output_path_size > 0)
                        output_path[output_path_size - 1] = '\0';
                }
                if (migrated)
                    *migrated = false;
                return true;
            }
        }
    } /* !did_crash_recovery */

    FILE *fp = fopen(db_path, "rb");
    if (!fp)
        return false;

    /* Read the header using a union that covers both v6 (118 bytes) and v7 (90 bytes).
     * v7 databases created by db.new() may be only 90 bytes on disk, so reading
     * sizeof(tydatabaserecord) = 118 bytes as a single fread item would fail.
     * Instead, read byte-by-byte up to the larger size and accept partial reads
     * as long as we got at least the v7 header size (90 bytes). Issue #264. */
    tydatabaserecord header;
    memset(&header, 0, sizeof header);
    size_t bytes_read = fread(&header, 1, sizeof header, fp);
    fclose(fp);
    if (bytes_read < sizeof(tydatabaserecord_64))
        return false;  /* Too small for even the v7 header */

    if (!detect_database_format(&header))
        return false;

    /* Seed format mode from the on-disk header so we don't remigrate already-v7 roots. */
    db_format_mode detected_mode = {!db_format_is_v6_header(&header), false};
    db_format_mode_apply(&detected_mode);

    if (db_format_mode_current().use_64bit_format) {
        /* Already v7 - return original path */
        if (output_path && output_path_size > 0) {
            strncpy(output_path, db_path, output_path_size);
            if (output_path_size > 0)
                output_path[output_path_size - 1] = '\0';
        }
        return true;
    }

    /* Issue #271: TOCTOU race prevention — acquire a lock file before migrating
     * so that concurrent processes cannot both attempt migration simultaneously. */

#if !defined(_WIN32)
    {
        char lock_path[1024];
        int lock_fd = migration_lock_acquire(db_path, lock_path, sizeof lock_path);

        if (lock_fd == -2) {
            /* Unexpected error creating lock file. */
#if defined(FRONTIER_HEADLESS)
            log_error(LOG_COMP_DB,
                "ensure_database_v7: cannot create migration lock file %s: %s",
                lock_path, strerror(errno));
#endif
            return false;
        }

        if (lock_fd == -1) {
            /* Another process is migrating — wait for it to finish. */
#if defined(FRONTIER_HEADLESS)
            log_info(LOG_COMP_DB,
                "ensure_database_v7: migration in progress by another process, waiting...");
#endif

            if (!migration_lock_wait(lock_path)) {
#if defined(FRONTIER_HEADLESS)
                log_error(LOG_COMP_DB,
                    "ensure_database_v7: timed out waiting for migration lock %s", lock_path);
#endif
                return false;
            }

            /* Other process finished — re-read the header to see if it migrated. */
            FILE *fp_recheck = fopen(db_path, "rb");

            if (!fp_recheck)
                return false;

            tydatabaserecord recheck_header;
            memset(&recheck_header, 0, sizeof recheck_header);
            size_t recheck_bytes = fread(&recheck_header, 1, sizeof recheck_header, fp_recheck);
            fclose(fp_recheck);

            if (recheck_bytes >= sizeof(tydatabaserecord_64) && !db_format_is_v6_header(&recheck_header)) {
                /* Other process completed migration successfully.
                 * Don't call db_format_mode_apply here — consistent with the
                 * existing fast-return path at line 2509 which also skips it.
                 * The caller (dbopenverb) handles mode setup after open. */
                if (output_path && output_path_size > 0) {
                    strncpy(output_path, db_path, output_path_size);
                    if (output_path_size > 0)
                        output_path[output_path_size - 1] = '\0';
                }

                if (migrated)
                    *migrated = true;

                return true;
            }

            /* Other process did not leave a valid v7 file — fall through to migrate ourselves.
             * We need to acquire the lock first. */
            lock_fd = migration_lock_acquire(db_path, lock_path, sizeof lock_path);

            if (lock_fd < 0) {
#if defined(FRONTIER_HEADLESS)
                log_error(LOG_COMP_DB,
                    "ensure_database_v7: cannot acquire migration lock after wait: %s",
                    strerror(errno));
#endif
                return false;
            }
        }

        /* We hold the lock — perform migration. */
        boolean migrate_ok = migrate_internal(db_path, NULL);

        migration_lock_release(lock_fd, lock_path);

        if (!migrate_ok)
            return false;
    }
#else
    /* Windows: no lock file support yet — migrate without locking. */
    if (!migrate_internal(db_path, NULL))
        return false;
#endif

    /* Migration succeeded; return path to new v7 file */
    if (output_path && output_path_size > 0) {
        if (!db_format_last_migration_output_path(output_path, output_path_size))
            return false;
    }

    /* Future reads should treat file as v7. */
    if (migrated)
        *migrated = true;
    return true;
}

boolean db_format_last_migration_output_path(char *buffer, size_t length) {
    if (buffer == NULL || length == 0)
        return false;
    if (last_migration_output_path[0] == '\0') {
        buffer[0] = '\0';
        return false;
    }
    strncpy(buffer, last_migration_output_path, length);
    if (length > 0)
        buffer[length - 1] = '\0';
    return true;
}

void db_format_clear_last_migration_output_path(void) {
    last_migration_output_path[0] = '\0';
}

boolean db_format_last_backup_output_path(char *buffer, size_t length) {
    if (buffer == NULL || length == 0)
        return false;
    if (last_backup_output_path[0] == '\0') {
        buffer[0] = '\0';
        return false;
    }
    strncpy(buffer, last_backup_output_path, length);
    if (length > 0)
        buffer[length - 1] = '\0';
    return true;
}

void db_format_clear_last_backup_output_path(void) {
    last_backup_output_path[0] = '\0';
}

boolean db_format_derive_v6_backup_path(const char *db_path, char *backup, size_t backup_size) {
    /*
     * Derive the .v6.root backup path for a given database path.
     * If db_path ends with .root, replaces it with .v6.root.
     * Otherwise appends .v6.
     *
     * This is the single source of truth for backup naming — used by
     * migrate_internal() and the CLI --migrate output message.
     */
    if (db_path == NULL || backup == NULL || backup_size == 0)
        return false;

    int written;
    const char *ext = strrchr(db_path, '.');
    if (ext && strcmp(ext, ".root") == 0) {
        size_t base_len = (size_t)(ext - db_path);
        written = snprintf(backup, backup_size, "%.*s.v6.root", (int)base_len, db_path);
    } else {
        written = snprintf(backup, backup_size, "%s.v6", db_path);
    }
    if (written < 0 || (size_t)written >= backup_size) {
        log_error(LOG_COMP_DB, "db_format_derive_v6_backup_path: path too long for buffer");
        return false;
    }
    return true;
}

void db_format_force_strict_v7_reader(void) {
    g_legacy_adapter_active = false;
    g_legacy_adapter_force_repack = false;
    memset(&g_legacy_widened_header, 0, sizeof g_legacy_widened_header);
    db_format_mode mode = {true, false};
    db_format_mode_apply(&mode);
}
void db_format_mode_apply(const db_format_mode *mode) {
    /* During migration with mode lock active, prevent downgrading from v7 to v6 writes */
    if (g_legacy_adapter_mode_locked &&
        g_mode_state.use_64bit_format == true &&
        mode->use_64bit_format == false) {
#if defined(FRONTIER_HEADLESS)
        static int lock_count = 0;
        if (lock_count++ < 3) {
            log_warn(LOG_COMP_DB, "db_format_mode_apply: BLOCKED v7->v6 downgrade (mode locked)");
        }
#endif
        /* Keep adapter_repack flag but preserve v7 write mode */
        g_legacy_adapter_force_repack = mode->adapter_repack;
        return;
    }

    /* CRITICAL MIGRATION FIX: Block invalid v6+adapter mode combination.
     * During migration, adapter_repack=1 means we're converting v6->v7. If someone
     * tries to apply use_64bit=0 during this time, it will write v6 format to v7
     * database. Force use_64bit=1 when adapter is active.
     * See: planning/architectural_decision_records/MODE_STACK_REFACTOR_PLAN.md */
    db_format_mode fixed_mode = *mode;
#if defined(FRONTIER_HEADLESS)
    if (fixed_mode.use_64bit_format == 0 && fixed_mode.adapter_repack == 1) {
        static int warn_count = 0;
        if (warn_count++ < 3) {
            log_warn(LOG_COMP_DB, "WARNING: db_format_mode_apply blocked invalid v6+adapter mode!");
            log_warn(LOG_COMP_DB, "  Forcing use_64bit=1 to prevent v6 format in v7 database");
        }
        fixed_mode.use_64bit_format = true;
    }
    log_trace(LOG_COMP_DB, "db_format_mode_apply use_64bit=%d adapter_repack=%d",
              (int) fixed_mode.use_64bit_format, (int) fixed_mode.adapter_repack);
#endif
    g_mode_state = fixed_mode;
    g_legacy_adapter_force_repack = fixed_mode.adapter_repack;
}

void db_format_mode_push(const db_format_mode *mode) {
    db_format_mode effective = {false, false};
    if (mode != NULL)
        effective = *mode;
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "db_format_mode_push: depth %d->%d use_64bit=%d adapter_repack=%d",
              g_mode_depth, g_mode_depth + 1, (int) effective.use_64bit_format, (int) effective.adapter_repack);
#endif
    if (g_mode_depth < (int) (sizeof g_mode_stack / sizeof g_mode_stack[0]))
        g_mode_stack[g_mode_depth++] = effective;
    db_format_mode_apply(&effective);
}

void db_format_mode_pop(void) {
    if (g_mode_depth > 0)
        g_mode_depth--;
    if (g_mode_depth > 0)
        db_format_mode_apply(&g_mode_stack[g_mode_depth - 1]);
    /* When popping the last mode from the stack, do NOT call db_format_mode_apply -
       the base mode was set by db_format_adapter_enable_wide_writes and should remain
       in effect. Calling db_format_mode_apply here would overwrite it with whatever
       mode was last pushed/popped, which could have use_64bit_format=false. */
}

db_format_mode db_format_mode_current(void) {
    db_format_mode current;
    if (g_mode_depth > 0)
        current = g_mode_stack[g_mode_depth - 1];
    else
        current = g_mode_state;
    /* Always use the global adapter_repack flag which is kept in sync by mode_apply */
    current.adapter_repack = g_legacy_adapter_force_repack;
#if defined(FRONTIER_HEADLESS)
    static int call_count = 0;
    if (call_count++ < 20) {
        log_trace(LOG_COMP_DB, "db_format_mode_current: depth=%d use_64bit=%d (stack=%d state=%d) adapter_repack=%d",
                  g_mode_depth, (int) current.use_64bit_format,
                  g_mode_depth > 0 ? (int) g_mode_stack[g_mode_depth - 1].use_64bit_format : -1,
                  (int) g_mode_state.use_64bit_format, (int) current.adapter_repack);
    }
#endif
    return current;
}

void db_context_init(db_context *context) {
    if (context == NULL)
        return;
    context->mode = db_format_mode_current();
    context->database = databasedata;
    db_saveas_state_snapshot(&context->saveas);
}

void db_context_init_with_mode(db_context *context, const db_format_mode *mode) {
    if (context == NULL || mode == NULL)
        return;
    db_context_init(context);
    context->mode = *mode;
}

void db_context_init_legacy_read(db_context *context, hdldatabaserecord db) {
    db_format_mode legacy_mode = {false, false};
    db_context_init_with_mode(context, &legacy_mode);
    context->database = db;
}

void db_context_init_v7_read(db_context *context, hdldatabaserecord db) {
    db_format_mode v7_mode = {true, false};
    db_context_init_with_mode(context, &v7_mode);
    context->database = db;
}

void db_context_init_v7_write(db_context *context, hdldatabaserecord db) {
    /* Currently identical to v7_read: use_64bit_format=true, adapter_repack=false.
     * Kept as a separate function so callers express intent (read vs write) and
     * the two paths can diverge if v7_write ever needs write-specific state
     * (e.g., dirty flags, journal references). */
    db_context_init_v7_read(context, db);
}

void db_context_clone_with_mode(const db_context *src, db_context *dst, const db_format_mode *mode) {
    if (src == NULL || dst == NULL || mode == NULL)
        return;
    *dst = *src;  /* Shallow copy */
    dst->mode = *mode;
}

void db_context_apply(const db_context *context) {
    if (context == NULL)
        return;
    db_format_mode_apply(&context->mode);
    if (context->database != nil)
        databasedata = context->database;
}

hdlfilenum db_context_fnum(const db_context *context) {

	if (context == NULL || context->database == nil) {

		assert(false && "db_context_fnum: NULL context or nil database");

		log_error(LOG_COMP_DB, "db_context_fnum: NULL context or nil database handle");

		return (hdlfilenum) -1;
		}

	return (hdlfilenum)((**context->database).fnumdatabase);
	} /*db_context_fnum*/

boolean hashpacktable_context(const db_context *context, hdlhashtable ht, boolean flsave, Handle *hpacked, boolean *flmustsave) {
    /* Call internal version with explicit context - no more context guard needed */
    return hashpacktable_internal(context, ht, flsave, hpacked, flmustsave);
}

boolean hashunpacktable_context(const db_context *context, Handle hpacked, boolean flmemory, hdlhashtable htable) {
#if defined(FRONTIER_HEADLESS)
    log_trace(LOG_COMP_DB, "hashunpacktable_context enter htable=%p flmemory=%d ctx=%p",
              (void *) htable, (int) flmemory, (void *) context);
#endif
    /* Call internal version with explicit context - no more context guard needed */
    return hashunpacktable_internal(context, hpacked, flmemory, htable);
}

/*
Phase 7 _context() wrappers below call _hdb variants which derive v6/v7
format from (**hdb).headerLength rather than from context->mode.  This is
safe because db_context_for_destination() always constructs context->mode
to match the destination database's format.  If this invariant is ever
violated (mismatched mode vs headerLength), the _hdb path will use the
authoritative headerLength and the mode field will be ignored.
*/

boolean dbassignhandle_context(const db_context *context, Handle h, dbaddress *adr) {
    /*
    Phase 7: Explicit context — no global mutation. Threads database handle
    directly through dbassignhandle_hdb, bypassing databasedata entirely.
    For NULL context or nil database, falls through to legacy path.
    */
    if (context != NULL && context->database != nil) {
        return dbassignhandle_hdb(h, adr, context->database);
    }
    return dbassignhandle(h, adr);
}

boolean dbrefhandle_context(const db_context *context, dbaddress adr, Handle *h) {
    /*
    Explicit context — no global mutation at all. Threads file number and
    header size directly from context, bypassing databasedata entirely.
    No save/restore needed (unlike sibling _context() functions that
    temporarily mutate and restore the global).

    2026-02-03: During migration, global mode may be locked to v7.
    Use explicit header size from context instead of global mode.

    2026-02-17: Thread file number explicitly through the read chain instead
    of mutating global databasedata. Fixes guest database external access
    (scripts, outlines, WP text, menus, pictures) where the global assignment
    didn't persist through nested calls and reads hit the wrong file.
    */
    if (context != NULL) {
        long header_size = context->mode.use_64bit_format ? sizeheader_v7 : sizeheader_v6;

        if (context->database != nil) {
            /* fnumdatabase is stored as long for cross-platform struct size
               consistency (see tydatabaserecord in db.h for rationale), but
               the actual file descriptor fits in hdlfilenum (typedef short).
               The cast is safe because POSIX file descriptors are small
               non-negative integers. */
            hdlfilenum fnum = (hdlfilenum) (**context->database).fnumdatabase;
            return dbrefhandle_fnum(adr, h, header_size, fnum);
        }

        return dbrefhandle_with_header_size(adr, h, header_size);
    }
    return dbrefhandle(adr, h);
}

boolean dbcopy_context(const db_context *context, dbaddress src, dbaddress *dest) {
    /*
    Phase 7: Explicit context — no global mutation. Threads database handle
    directly through dbcopy_hdb, bypassing databasedata entirely.
    For NULL context or nil database, falls through to legacy path.
    */
    if (context != NULL && context->database != nil) {
        return dbcopy_hdb(src, dest, context->database);
    }
    return dbcopy_internal(src, dest);
}

boolean dbassign_context(const db_context *context, dbaddress *padr, long newsize, ptrvoid pdata) {
    /*
    Context-aware dbassign. For non-NULL contexts with a database handle,
    calls dbassign_hdb directly — no global mutation needed.
    For NULL context, delegates to dbassign_internal (uses global state).
    */

    if (context != NULL && context->database != nil) {
        return dbassign_hdb(padr, newsize, pdata, context->database);
    }
    return dbassign_internal(padr, newsize, pdata);
}

boolean dbreference_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata) {
    /*
    Context-aware dbreference. For non-NULL contexts with a database handle,
    passes fnum explicitly via dbreference_fnum — no global mutation needed.
    For NULL context, delegates to dbreference_internal (uses global state).
    */

    if (context != NULL) {
        long header_size = context->mode.use_64bit_format ? sizeheader_v7 : sizeheader_v6;

        if (context->database != nil) {
            /* Thread fnum explicitly — no databasedata mutation */
            return dbreference_fnum(adr, ctbytes, pdata, header_size, db_context_fnum(context));
        }

        /* FALLBACK: non-NULL context with nil database — reads from global
           databasedata via legacy path.  This case occurs when the caller
           constructs a context to override mode only (e.g. force v6 header
           interpretation) while keeping the current database.  It is NOT a
           thread-safe path; Phase 7+ should require all contexts to carry a
           non-nil database handle once the allocation subsystem is converted. */
        return dbreference_with_header_size(adr, ctbytes, pdata, header_size);
    }

    /* NULL context: use current global databasedata and mode as-is */
    return dbreference_internal(adr, ctbytes, pdata);
}

boolean dbreference_handle_context(const db_context *context, dbaddress adr, Handle *h) {
    /*
    Phase 7: Explicit context — no global mutation. Threads database handle
    directly through dbrefhandle_hdb, bypassing databasedata entirely.
    For NULL context or nil database, falls through to legacy path.
    */
    if (context != NULL && context->database != nil) {
        return dbrefhandle_hdb(adr, h, context->database);
    }
    return dbrefhandle(adr, h);
}

boolean dbread_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata) {
    /*
    Context-aware dbread. For non-NULL contexts with a database handle,
    passes fnum explicitly via dbread_fnum — no global mutation needed.
    For NULL context or nil database, delegates to legacy dbread.
    */

    if (context != NULL && context->database != nil) {
        return dbread_fnum(adr, ctbytes, pdata, db_context_fnum(context));
    }

    return dbread(adr, ctbytes, pdata);
}

boolean dbwrite_context(const db_context *context, dbaddress adr, long ctbytes, ptrvoid pdata) {
    /*
    Context-aware dbwrite. For non-NULL contexts with a database handle,
    passes fnum explicitly via dbwrite_fnum — no global mutation needed.
    For NULL context or nil database, delegates to legacy dbwrite.
    */

    if (context != NULL && context->database != nil) {
        return dbwrite_fnum(adr, ctbytes, pdata, db_context_fnum(context), context->database);
    }

    return dbwrite(adr, ctbytes, pdata);
}

boolean dbsavehandle_context(const db_context *context, Handle h, dbaddress *adr) {
    /*
    Phase 7: Explicit context — no global mutation. Threads database handle
    directly through dbsavehandle_hdb, bypassing databasedata entirely.
    For NULL context or nil database, falls through to legacy path.
    */
    if (context != NULL && context->database != nil) {
        return dbsavehandle_hdb(h, adr, context->database);
    }
    return dbsavehandle(h, adr);
}

boolean dbgeteof_context(const db_context *context, long *eof) {
    /*
    Context-aware dbgeteof. For non-NULL contexts with a database handle,
    passes fnum explicitly via dbgeteof_fnum — no global mutation needed.
    For NULL context or nil database, delegates to legacy dbgeteof.
    */

    if (context != NULL && context->database != nil) {
        return dbgeteof_fnum(eof, db_context_fnum(context));
    }

    return dbgeteof(eof);
}
