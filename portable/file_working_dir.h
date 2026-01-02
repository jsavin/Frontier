/*
 * file_working_dir.h - Thread-local working directory API for portable builds
 *
 * Manages process-level and thread-local working directories for file operations.
 * Each UserTalk thread has its own working directory that can be changed via
 * file.setPath() without affecting other threads.
 *
 * Created: 2026-01-01 - Thread-local working directory system
 */

#ifndef file_working_dir_h
#define file_working_dir_h

#include "standard.h"

/*
 * Initialize process-level default working directory
 *
 * Called once at startup from main(). Sets the default working directory
 * that new threads will inherit.
 *
 * Parameters:
 *   path: Absolute path (C string) to set as default cwd
 *         NULL or empty string sets an empty default
 *
 * Thread-safety: Not thread-safe. Must be called during single-threaded
 *                initialization before any UserTalk threads are created.
 */
extern void init_default_working_dir(const char *path);

/*
 * Get thread-local working directory
 *
 * Returns the current thread's working directory as a bigstring.
 * If no thread context exists, returns the process default.
 *
 * Parameters:
 *   out: Output bigstring to receive the working directory path
 *
 * Returns:
 *   true if successful, false if out parameter is NULL
 *
 * Thread-safety: Thread-safe. Reads from current thread's tythreadglobals.
 */
extern boolean get_thread_working_dir(bigstring out);

/*
 * Set thread-local working directory
 *
 * Updates the current thread's working directory. Does not affect other
 * threads or the process default.
 *
 * Parameters:
 *   path: New working directory (bigstring)
 *
 * Returns:
 *   true if successful, false if no thread context or path is NULL
 *
 * Thread-safety: Thread-safe. Writes to current thread's tythreadglobals.
 *
 * Note: Does not validate that path exists or is a directory. Invalid
 *       paths will cause file operations to fail naturally.
 */
extern boolean set_thread_working_dir(const bigstring path);

/*
 * Get process-level default working directory
 *
 * Returns the process default working directory that was set at startup.
 * New threads inherit this value for their thread-local cwd.
 *
 * Parameters:
 *   out: Output bigstring to receive the default working directory
 *
 * Thread-safety: Thread-safe (read-only after initialization).
 */
extern void get_process_default_cwd(bigstring out);

#endif /* file_working_dir_h */
