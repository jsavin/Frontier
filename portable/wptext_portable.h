#ifndef WPTEXT_PORTABLE_H
#define WPTEXT_PORTABLE_H

#if !defined(__FRONTIER_H__) && !defined(PORTABLE_FRONTIER_H)
#error "Include frontier.h before wptext_portable.h"
#endif

/*
 * Headless Paige bootstrap helpers.
 * These keep the WP engine alive for RTF conversions without pulling in the full UI stack.
 */

Boolean wp_portable_init(void);
void wp_portable_shutdown(void);

/*
 * Internal helpers used by the portable WP runtime to reach the Paige globals.
 * We return opaque pointers here to avoid forcing every includer to pull in PAIGE.H.
 */
#ifdef FRONTIER_HEADLESS
struct pg_globals;
struct pgm_globals;

struct pg_globals *wp_portable_pg_globals(void);
struct pgm_globals *wp_portable_mem_globals(void);
#endif

#ifdef FRONTIER_TESTS
Boolean wp_portable_pack_text_for_test(const char *utf8text, Handle *hpacked);
#endif

#endif /* WPTEXT_PORTABLE_H */
