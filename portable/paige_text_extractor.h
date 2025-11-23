#ifndef PAIGE_TEXT_EXTRACTOR_H
#define PAIGE_TEXT_EXTRACTOR_H

#include "frontier.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	long text_key_count;
	long total_text_bytes;
	boolean returned_macroman;
	boolean dropped_controls;
} paige_extract_stats;

boolean paige_extract_text_and_styles(const uint8_t *bytes, long len, Handle *hout_utf8,
	paige_extract_stats *stats, char *errbuf, size_t errbuflen);

boolean wptext_emit_rtf_from_paige_blob(const uint8_t *bytes, long len, Handle *hout_rtf,
    long *out_char_count, paige_extract_stats *stats, char *errbuf, size_t errbuflen);

#ifdef __cplusplus
}
#endif

#endif /* PAIGE_TEXT_EXTRACTOR_H */
