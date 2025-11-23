/* The straight C interface header for PAIGE HYPERTEXT links. */

#ifndef PGHTEXT_H
#define PGHTEXT_H

#include "Paige.h"
#include "pgFiles.h"

/* Callback verbs: */

enum {
	hyperlink_mousedown_verb,
	hyperlink_doubleclick_verb,
	hyperlink_mouseup_verb,
	hyperlink_delete_verb
};


#define STYLE_COLOR_INTENSITY	0xE000

/* Hyperlink types */

#define HYPERLINK_NORMAL				0x00000001	/* Hyperlink normal */
#define HYPERLINK_INDEX					0x00000002	/* Hyperlink is an index */
#define HYPERLINK_TOC					0x00000004	/* Hyperlink is TOC. */
#define HYPERLINK_SUBJECT				0x00000008	/* Hyperlink is target subject */
#define HYPERLINK_SUMMARY				0x00000010	/* Summary link (and all those > 0x10) */

// BOG: eudora hyperlink support
#define HYPERLINK_EUDORA_ATTACHMENT 0x00000020  // hyperlink is an eudora attachment
#define HYPERLINK_EUDORA_PLUGIN     0x00000040  // hyperlink is an eudora plugin
#define HYPERLINK_EUDORA_AUTOURL    0x00000080	// hyperlink is an auto-generated url
#define HYPERLINK_EUDORA_URL		0x00000100	// hyperlink is inserted from menu
#define HYPERLINK_EUDORA_INFO		0x00000200	// hyperlink is for eudora special info attachment (reg info, profile, etc.)

#define UNRESOLVED_LINK					0x40000000	/* Flag marking source links as not yet pointing to a target */


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (long) pgSetHyperlinkSource (pg_ref pg, select_pair_ptr selection,
			pg_char_ptr URL, pg_char_ptr keyword_display, ht_callback callback, long type,
			long id_num, short state1_style, short state2_style, short state3_style, short draw_mode);

extern PG_PASCAL (long) pgSetHyperlinkTarget (pg_ref pg, select_pair_ptr selection,
			pg_char_ptr URL, ht_callback callback, long type, long id_num, short display_style,
			short draw_mode);

extern PG_PASCAL (void) pgSetHyperlinkTargetState (pg_ref pg, long position, short state, pg_boolean redraw);

extern PG_PASCAL (void) pgSetHyperlinkSourceState (pg_ref pg, long position, short state, pg_boolean redraw);

extern PG_PASCAL (void) pgChangeHyperlinkSource (pg_ref pg, long position,
			select_pair_ptr selection, pg_char_ptr URL, pg_char_ptr keyword_display,
			ht_callback callback, short state1_style, short state2_style, short state3_style,
			short draw_mode);

extern PG_PASCAL (void) pgChangeHyperlinkTarget (pg_ref pg, long position,
			select_pair_ptr selection, pg_char_ptr URL, ht_callback callback,
			short display_style, short draw_mode);

extern PG_PASCAL (void) pgDeleteHyperlinkSource (pg_ref pg, long position, pg_boolean redraw);

extern PG_PASCAL (void) pgDeleteHyperlinkTarget (pg_ref pg, long position, pg_boolean redraw);

extern PG_PASCAL (pg_boolean) pgGetHyperlinkSourceInfo (pg_ref pg, long position, long id, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink);

extern PG_PASCAL (pg_boolean) pgGetHyperlinkTargetInfo (pg_ref pg, long position, long id, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink);

extern PG_PASCAL (void) pgSetHyperlinkSourceInfo (pg_ref pg, long position, long id,
			pg_hyperlink_ptr hyperlink);

extern PG_PASCAL (void) pgSetHyperlinkTargetInfo (pg_ref pg, long position, long id,
			pg_hyperlink_ptr hyperlink);

extern PG_PASCAL (long) pgPtInHyperlinkSource(pg_ref pg, co_ordinate_ptr point);

extern PG_PASCAL (long) pgPtInHyperlinkTarget(pg_ref pg, co_ordinate_ptr point);

extern PG_PASCAL (pg_boolean) pgIsHyperlinkSource (pg_ref pg, long position);

extern PG_PASCAL (pg_boolean) pgIsHyperlinkTarget (pg_ref pg, long position);

extern PG_PASCAL (pg_boolean) pgGetSourceURL (pg_ref pg, long position, pg_char_ptr URL, short max_size);

extern PG_PASCAL (pg_boolean) pgGetTargetURL (pg_ref pg, long position, pg_char_ptr URL, short max_size);

extern PG_PASCAL (pg_boolean) pgSetSourceURL (pg_ref pg, long position, pg_char_ptr URL);

extern PG_PASCAL (pg_boolean) pgSetTargetURL (pg_ref pg, long position, pg_char_ptr URL);

extern PG_PASCAL (long) pgGetSourceID (pg_ref pg, long position);

extern PG_PASCAL (long) pgGetTargetID (pg_ref pg, long position);

extern PG_PASCAL (long) pgFindHyperlinkTarget (pg_ref pg, long starting_position, long PG_FAR *end_position,
		pg_char_ptr URL, pg_boolean partial_find_ok, pg_boolean case_insensitive, pg_boolean scroll_to);

extern PG_PASCAL (long) pgFindHyperlinkSource (pg_ref pg, long starting_position, long PG_FAR *end_position,
		pg_char_ptr URL, pg_boolean partial_find_ok, pg_boolean case_insensitive, pg_boolean scroll_to);

extern PG_PASCAL (long) pgFindHyperlinkTargetByID (pg_ref pg, long starting_position, long PG_FAR *end_position,
		long id, pg_boolean scroll_to);

extern PG_PASCAL (long) pgFindHyperlinkSourceByID (pg_ref pg, long starting_position, long PG_FAR *end_position,
		long id, pg_boolean scroll_to);

extern PG_PASCAL (void) pgSetHyperlinkCallback (pg_ref pg, ht_callback source_callback, ht_callback target_callback);

extern PG_PASCAL (long) pgStandardSourceCallback (paige_rec_ptr pg, pg_hyperlink_ptr hypertext,
		short command, short modifiers, long position, pg_char_ptr URL);

extern PG_PASCAL (long) pgStandardTargetCallback (paige_rec_ptr pg, pg_hyperlink_ptr hypertext,
		short command, short modifiers, long position, pg_char_ptr URL);

extern PG_PASCAL (short) pgNewHyperlinkStyle (pg_ref pg, pg_short_t red, pg_short_t green,
		pg_short_t blue, long stylebits, pg_boolean background);

extern PG_PASCAL (void) pgScrollToLink (pg_ref pg, long text_position);

extern PG_PASCAL (void) pgInitDefaultSource (pg_ref pg, pg_hyperlink_ptr link);

extern PG_PASCAL (void) pgInitDefaultTarget (pg_ref pg, pg_hyperlink_ptr link);

/* Lower-level utilities */

extern PG_PASCAL (pg_hyperlink_ptr) pgFindHypertextRun (memory_ref hyperlinks, long offset, long PG_FAR *index);

extern PG_PASCAL (void) pgAdvanceHyperlinks (paige_rec_ptr pg, memory_ref hyperlinks, long offset, long length);

extern PG_PASCAL (long) pgPackHyperlinks (pack_walk_ptr walker, memory_ref hyperlinks, long PG_FAR *unpack_size);

extern PG_PASCAL (void) pgUnpackHyperlinks (paige_rec_ptr pg, pack_walk_ptr walker, ht_callback callback,
			long qty, memory_ref hyperlinks);

extern PG_PASCAL (long) pgAssignLinkID (memory_ref existing_links);

extern PG_PASCAL (long) pgValidateID (memory_ref existing_links, long id);

extern PG_PASCAL (void) pgInsertURL (paige_rec_ptr pg, pg_hyperlink_ptr hyperlink, pg_char_ptr URL);

extern PG_PASCAL (pg_boolean) pgGetLinkURL (pg_ref pg, pg_boolean for_target, long position, pg_char_ptr URL, short max_size);

#ifdef __cplusplus
	}
#endif

#endif

