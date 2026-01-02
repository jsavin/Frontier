/*
 * fileverbs_portable.h - Portable file verb implementations for headless mode
 *
 * This file provides portable implementations of Frontier's 86 file verbs
 * for headless builds. It replaces the Mac-specific fileverbs.c which uses
 * CoreFoundation and Carbon APIs that don't compile in headless mode.
 *
 * Architecture:
 * - Tier 1 (Critical): 20 verbs - Basic file operations, paths, creation
 * - Tier 2 (File I/O): 14 verbs - File handles, read/write operations
 * - Tier 3 (Optional): 16 verbs - Locking, visibility, date setting
 * - Tier 4 (Mac-specific): 36 verbs - UI dialogs, Finder metadata (stubbed)
 *
 * Created: 2026-01-01 - Portable file verb implementation for headless mode
 */

#ifndef fileverbs_portable_h
#define fileverbs_portable_h

#include "standard.h"
#include "lang.h"

/*
 * portable_filefunctionvalue - Main dispatcher for portable file verbs
 *
 * Implements 86 file verbs in portable/headless mode. Called by
 * headless_file_verbs_callback() in tests/headless_file_verbs.c.
 *
 * Parameters:
 *   token: Verb token (0-85, matches tyfiletoken enum)
 *   hparam1: Parameter tree node
 *   vreturned: Return value record
 *   bserror: Error message string (output)
 *
 * Returns:
 *   true if verb executed successfully, false on error
 *
 * Thread-safety: Thread-safe (uses thread-local working directory)
 */
extern boolean portable_filefunctionvalue(short token, hdltreenode hparam1,
                                         tyvaluerecord *vreturned, bigstring bserror);

/*
 * init_file_handle_cleanup - Initialize file handle cleanup on startup
 *
 * Registers atexit() hook to close leaked file handles on process exit.
 * MUST be called once from main() before any threads are spawned.
 *
 * Thread-safety: NOT thread-safe (must be called before multi-threading)
 */
extern void init_file_handle_cleanup(void);

#endif /* fileverbs_portable_h */
