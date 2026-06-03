/*
 * ut_scan.h - Boot-time import-discovery scan for ODB<->.ut sync.
 *
 * Walks the .ut sync tree at boot (after full ODB materialization) and
 * auto-creates any ODB node whose .ut file exists on disk but is absent
 * from the in-memory hashtable chain. Intermediate tables are created as
 * needed; leaf script objects are built from the raw .ut body bytes using
 * the same kernel primitives the normal import hook uses.
 *
 * This module is compiled ONLY into the production CLI binary. It must NOT
 * be linked into any kernel-free test binary (e.g. tests/runtime_tests).
 *
 * See docs/ODB_UT_SYNC_DESIGN.md for the full sync contract.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#ifndef UT_SCAN_INCLUDE
#define UT_SCAN_INCLUDE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ut_sync_scan_and_create - walk sync_dir/<root_basename>/ and create any
 * orphan ODB nodes found there.
 *
 * Must be called after langhash_materialize_disk_values() has fully loaded
 * the tree into memory (so a hashtable miss genuinely means absent, not
 * lazy-unloaded) and after clear_post_hydration_dirty_flags() has cleared
 * all dirty bits (so the caller can re-dirty only the nodes it creates).
 * Each newly created path is recorded via cli_record_ut_import() so the
 * existing cli_redirty_ut_imported_paths() pass persists them on save.
 *
 * Runs on the main thread holding the GIL -- the same execution context as
 * the bulk hydrate walk, so all kernel hashtable/op/lang primitives are safe
 * to call directly.
 *
 * Parameters:
 *   sync_dir     - value of --ut-sync-dir (e.g. "usertalk_scripts"). Not NULL.
 *   root_basename - filename of the loaded .root (e.g. "Frontier.root"). Not NULL.
 *
 * Returns the number of ODB nodes created (>= 0), or -1 on a hard error
 * (e.g. sync directory not accessible).
 *
 * Designed as a clean reusable routine: the future repl.syncScan REPL verb
 * will call this after boot to pick up .ut drops that arrive while the
 * runtime is running.
 */
int ut_sync_scan_and_create(const char *sync_dir, const char *root_basename);

#ifdef __cplusplus
}
#endif

#endif /* UT_SCAN_INCLUDE */
