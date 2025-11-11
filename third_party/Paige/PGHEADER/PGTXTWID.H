/* Header file for pgTxtWid (measuring text widths) */

#ifndef PGTXTWID_H
#define PGTXTWID_H

#include "Paige.h"

#define TEXT_PARTITION_SIZE		4096	/* Max text size for parsing char widths */
#define MAX_ACCUMULATED_WIDTHS	32500	/* Maximum accumulated widths */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (memory_ref) pgFindCharLocs (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t PG_FAR *found_index, memory_ref PG_FAR *types, pg_boolean must_be_exact,
		pg_boolean must_be_unlocked);
extern PG_PASCAL (long PG_FAR*) pgGetCharLocs (paige_rec_ptr pg_rec, text_block_ptr block,
		memory_ref PG_FAR *used_ref, memory_ref PG_FAR *types_ref);
extern PG_PASCAL (void) pgReleaseCharLocs (paige_rec_ptr pg, memory_ref locs_ref);
extern PG_PASCAL (void) pgFreeCharLocs(paige_rec_ptr pg, pg_short_t charloc_index);
extern PG_PASCAL (memory_ref) pgGetSpecialLocs (paige_rec_ptr pg_rec, text_block_ptr block,
		point_start_ptr related_start, pg_short_t text_size, long slop_extra,
		short scale_widths);
extern PG_PASCAL (void) pgPartialTextMeasure (paige_rec_ptr pg_rec, text_block_ptr block,
		pg_short_t measure_from, pg_short_t new_measure_length);
extern PG_PASCAL (void) pgTextReMeasure (paige_rec_ptr pg_rec, text_block_ptr block, size_t measure_from);
extern PG_PASCAL (void) pgDeleteTextMeasure (paige_rec_ptr pg_rec, text_block_ptr block,
		pg_short_t delete_from, pg_short_t delete_length);
extern PG_PASCAL (long) pgInvalTextBlock (paige_rec_ptr pg, text_block_ptr block,
		long insert_position, long ins_amount);
extern PG_PASCAL (void) pgInvalCharLocs (paige_rec_ptr pg);
extern PG_PASCAL (long) pgGetLineRealDescent (text_block_ptr block,
		point_start_ptr arbitrary_start, style_walk_ptr walker);
extern PG_PASCAL (pg_boolean) pgShouldBreakPage (paige_rec_ptr pg, pg_measure_ptr line_info,
		pg_short_t previous_flags, pg_short_t  starts_offset);
extern PG_PASCAL (void) pgAcceptEmptyLine (paige_rec_ptr pg, point_start_ptr the_start,
		pg_measure_ptr line_stuff);

#ifdef __cplusplus
	}
#endif

#endif

