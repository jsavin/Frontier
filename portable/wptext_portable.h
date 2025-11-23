#ifndef WPTEXT_PORTABLE_H
#define WPTEXT_PORTABLE_H

#if !defined(__FRONTIER_H__) && !defined(PORTABLE_FRONTIER_H)
#error "Include frontier.h before wptext_portable.h"
#endif

#include "langexternal.h"

/* 2025-11-19 Codex: Removed Paige bootstrap exports; headless runtimes now treat Paige as absent. */

Boolean wp_portable_init(void);
void wp_portable_shutdown(void);

#ifdef FRONTIER_HEADLESS
Boolean wp_portable_external_should_drop(hdlexternalvariable hv);
void wp_portable_note_drop_logged(hdlexternalvariable hv, const char *name_hint);
Boolean wp_portable_external_was_legacy_ws(hdlexternalvariable hv);
void wp_portable_note_conversion_logged(hdlexternalvariable hv, const char *name_hint);
Boolean wp_portable_extract_plaintext(hdlexternalvariable hv, Handle *hout_utf8);
#endif

int wp_portable_get_last_error_line(void);
int wp_portable_get_last_errno(void);

#ifdef FRONTIER_TESTS
Boolean wp_portable_pack_text_for_test(const char *utf8text, Handle *hpacked);
Boolean wp_portable_load_portable_blob_for_test(const unsigned char *blob, long len);
#endif

#endif /* WPTEXT_PORTABLE_H */
