/* This is the C interface code to C++ hypertext link objects. */

#include "Paige.h"
#include "pgOSUtl.h"
#include "pgEdit.h"
#include "pgUtils.h"
#include "pgText.h"
#include "PGHText.h"
#include "machine.h"

static void insert_hyperlink (paige_rec_ptr pg, memory_ref hyperlink_run, select_pair_ptr selection,
			pg_hyperlink_ptr hyperlink, pg_char_ptr URL, pg_char_ptr insert_option, short draw_mode);
static pg_boolean belongs_to_link (pg_hyperlink_ptr link, long offset);
static pg_char_ptr get_URL_ptr (pg_hyperlink_ptr link);
static pg_boolean compare_URL (pg_char_ptr str1, pg_char_ptr str2, pg_boolean partial_find, pg_boolean ignore_case);
static void set_link_state (pg_ref pg, pg_boolean for_target, long position, short state, pg_boolean redraw);
static void delete_link (pg_ref pg, pg_boolean for_target, long position, pg_boolean redraw);
static pg_boolean get_link_info (pg_ref pg, pg_boolean for_target, long position, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink);
static long pt_in_link (pg_ref pg, pg_boolean for_target, co_ordinate_ptr point);
static long find_link (pg_ref pg, pg_boolean for_target, long start_position, long PG_FAR *end_position,
		long use_ID, pg_char_ptr URL, pg_boolean partial_find, pg_boolean ignore_case, pg_boolean scroll_to,
		pg_hyperlink_ptr hyperlink_record);
static long get_update_beginning (paige_rec_ptr pg, long position);
static long find_id (memory_ref links_ref, long id);
static void delete_overlapping_links (paige_rec_ptr pg, memory_ref links_ref, select_pair_ptr overlap);
static void delete_link_from_ref (paige_rec_ptr pg, memory_ref links_ref, long index);


PG_PASCAL (long) pgSetHyperlinkSource (pg_ref pg, select_pair_ptr selection,
			pg_char_ptr URL, pg_char_ptr keyword_display, ht_callback callback, long type,
			long id_num, short state1_style, short state2_style, short state3_style, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink		link;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgFillBlock(&link, sizeof(pg_hyperlink), 0);
	
	if ((link.callback = callback) == NULL)
		link.callback = pgStandardSourceCallback;

	if ((link.state1_style = state1_style) == 0)
		link.state1_style = pgNewHyperlinkStyle(pg, 0, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);

	if ((link.state2_style = state2_style) == 0)
		link.state2_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, 0, X_UNDERLINE_BIT, FALSE);

	if ((link.state3_style = state3_style) == 0)
		link.state3_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);

	link.active_style = link.state1_style;
	link.type = type;
	link.unique_id = id_num;
	insert_hyperlink(pg_rec, pg_rec->hyperlinks, selection, &link, URL, keyword_display, draw_mode);
	
	pg_rec->change_ctr += 1;

	UnuseMemory(pg);
	
	return	id_num;
}

/* pgSetHyperlinkTarget is the same as pgSetHyperlinkSource except this deals with a destination
link. */

PG_PASCAL (long) pgSetHyperlinkTarget (pg_ref pg, select_pair_ptr selection,
			pg_char_ptr URL, ht_callback callback, long type, long id_num,
			short display_style, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink		link;
	long				generated_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgFillBlock(&link, sizeof(pg_hyperlink), 0);
	
	if ((link.callback = callback) == NULL)
		link.callback = pgStandardTargetCallback;
	
	if (display_style >= 0)
		if ((link.state1_style = display_style) == 0)
			link.state1_style = pgNewHyperlinkStyle(pg, 0xFC00, 0xFD00, 0x7C00, 0, TRUE);
	
	link.active_style = link.state1_style;
	
	if ((link.unique_id = id_num) == 0)
		link.unique_id = pgAssignLinkID(pg_rec->target_hyperlinks);
	
	generated_id = link.unique_id;

	link.type = type;
	insert_hyperlink(pg_rec, pg_rec->target_hyperlinks, selection, &link, URL, NULL, draw_mode);

	pg_rec->change_ctr += 1;

	UnuseMemory(pg);
	
	return	generated_id;
}

/* pgChangeHyperlinkSource changes the source link to the specified info. Only non-null and
non-zero values are changed. */

PG_PASCAL (void) pgChangeHyperlinkSource (pg_ref pg, long position,
			select_pair_ptr selection, pg_char_ptr URL, pg_char_ptr keyword_display,
			ht_callback callback, short state1_style, short state2_style, short state3_style,
			short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink		new_link;
	pg_hyperlink_ptr	links;
	long				index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	links = pgFindHypertextRun(pg_rec->hyperlinks, position, &index);
	new_link = *links;
	UnuseMemory(pg_rec->hyperlinks);
	
	if (belongs_to_link(&new_link, position)) {
		
		delete_link_from_ref(pg_rec, pg_rec->hyperlinks, index);

		if (keyword_display)
			pgDelete(pg, &new_link.applied_range, draw_none);

		if (selection)
			new_link.applied_range = *selection;

		if (URL) {
			
			pgFillBlock(new_link.URL, FONT_SIZE, 0);
			
			//QUALCOMM Begin
			//alt_URL already disposed in the delete_link_from_ref call above
			//so commenting out this call here else causes a crash
			//if (new_link.alt_URL)
			//	DisposeMemory(new_link.alt_URL);
			//QUALCOMM End	== Kusuma
			
			new_link.alt_URL = MEM_NULL;
		}
		
		if (callback)
			new_link.callback = callback;
		
		if (state1_style)
			new_link.state1_style = new_link.active_style = state1_style;
		if (state2_style)
			new_link.state2_style = state2_style;
		if (state3_style)
			new_link.state3_style = state3_style;

		insert_hyperlink(pg_rec, pg_rec->hyperlinks, &new_link.applied_range, &new_link,
				URL, keyword_display, draw_mode);
		
		pg_rec->change_ctr += 1;
	}
	
	UnuseMemory(pg);
}


PG_PASCAL (void) pgChangeHyperlinkTarget (pg_ref pg, long position,
			select_pair_ptr selection, pg_char_ptr URL, ht_callback callback,
			short display_style, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink		new_link;
	pg_hyperlink_ptr	links;
	long				index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	links = pgFindHypertextRun(pg_rec->target_hyperlinks, position, &index);
	new_link = *links;
	UnuseMemory(pg_rec->target_hyperlinks);
	
	if (belongs_to_link(&new_link, position)) {
		
		delete_link_from_ref(pg_rec, pg_rec->target_hyperlinks, index);
		
		if (selection)
			new_link.applied_range = *selection;

		if (URL) {
			
			pgFillBlock(new_link.URL, FONT_SIZE, 0);
			
			if (new_link.alt_URL)
				DisposeMemory(new_link.alt_URL);
			
			new_link.alt_URL = MEM_NULL;
		}
		
		if (callback)
			new_link.callback = callback;
		
		if (display_style)
			new_link.state1_style = new_link.active_style = display_style;

		insert_hyperlink(pg_rec, pg_rec->target_hyperlinks, &new_link.applied_range, &new_link,
				URL, NULL, draw_mode);
		
		pg_rec->change_ctr += 1;
	}

	UnuseMemory(pg);
}

/* pgSetHyperlinkTargetState changes the display state. If position is negative, all
links are set to this state. */

PG_PASCAL (void) pgSetHyperlinkTargetState (pg_ref pg, long position, short state, pg_boolean redraw)
{
	set_link_state(pg, TRUE, position, state, redraw);
}

PG_PASCAL (void) pgSetHyperlinkSourceState (pg_ref pg, long position, short state, pg_boolean redraw)
{
	set_link_state(pg, FALSE, position, state, redraw);
}


PG_PASCAL (void) pgDeleteHyperlinkSource (pg_ref pg, long position, pg_boolean redraw)
{
	delete_link(pg, FALSE, position, redraw);
}


PG_PASCAL (void) pgDeleteHyperlinkTarget (pg_ref pg, long position, pg_boolean redraw)
{
	delete_link(pg, TRUE, position, redraw);
}

/* pgGetHyperlinkSourceInfo returns the actual hypertext link record for position.
If closest_one is TRUE then we return the one that contains the position - or - the nearest
one to the right, whichever is closest. If id is non-zero it searchs the hyperlink by ID. */

PG_PASCAL (pg_boolean) pgGetHyperlinkSourceInfo (pg_ref pg, long position, long id, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink)
{
	if (id == 0)
		return	get_link_info(pg, FALSE, position, closest_one, hyperlink);

	return (pg_boolean)(find_link(pg, FALSE, 0, NULL, id, NULL, FALSE, FALSE, FALSE, hyperlink) >= 0);
}


/* pgGetHyperlinkTargetInfo is the same as pgGetHyperlinkSourceInfo except this is for the
source runs. If id is non-zero it searchs the hyperlink by ID. */

PG_PASCAL (pg_boolean) pgGetHyperlinkTargetInfo (pg_ref pg, long position, long id, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink)
{
	if (id == 0)
		return	get_link_info(pg, TRUE, position, closest_one, hyperlink);
		
	return (pg_boolean)(find_link(pg, TRUE, 0, NULL, id, NULL, FALSE, FALSE, FALSE, hyperlink) >= 0);
}

/* pgSetHyperlinkTargetInfo changes the whole source link. */

PG_PASCAL (void) pgSetHyperlinkSourceInfo (pg_ref pg, long position, long id,
			pg_hyperlink_ptr hyperlink)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	long				use_position;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (id)
		use_position = find_link(pg, FALSE, 0, NULL, id, NULL, FALSE, FALSE, FALSE, NULL);
	else
		use_position = position;
	
	if (use_position >= 0) {
	
		links = pgFindHypertextRun(pg_rec->hyperlinks, use_position, NULL);
	
		if (belongs_to_link(links, use_position)) {
			*links = *hyperlink;
			pg_rec->change_ctr += 1;
		}

		UnuseMemory(pg_rec->hyperlinks);
	}

	UnuseMemory(pg);
}


/* pgSetHyperlinkTargetInfo changes the whole target. */

PG_PASCAL (void) pgSetHyperlinkTargetInfo (pg_ref pg, long position, long id,
			pg_hyperlink_ptr hyperlink)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	long				use_position;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (id)
		use_position = find_link(pg, TRUE, 0, NULL, id, NULL, FALSE, FALSE, FALSE, NULL);
	else
		use_position = position;
	
	if (use_position >= 0) {
	
		links = pgFindHypertextRun(pg_rec->target_hyperlinks, use_position, NULL);
	
		if (belongs_to_link(links, use_position)) {
			*links = *hyperlink;
			pg_rec->change_ctr += 1;
		}
	
		UnuseMemory(pg_rec->target_hyperlinks);
	}

	UnuseMemory(pg);
}


/* pgPtInHyperlinkSource returns the offset of a hyperlink containing point. If none found,
-1 is returned. */

PG_PASCAL (long) pgPtInHyperlinkSource(pg_ref pg, co_ordinate_ptr point)
{
	return	pt_in_link(pg, FALSE, point);
}


/* pgPtInHyperlinkTarget returns the offset of a hyperlink containing point. If none found,
-1 is returned. */

PG_PASCAL (long) pgPtInHyperlinkTarget(pg_ref pg, co_ordinate_ptr point)
{
	return	pt_in_link(pg, TRUE, point);
}


/* pgIsHyperlinkSource returns TRUE if position is within a source hypertext link. */

PG_PASCAL (pg_boolean) pgIsHyperlinkSource (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	link;
	long				use_offset;
	pg_boolean			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	use_offset = pgFixOffset(pg_rec, position);
	link = pgFindHypertextRun(pg_rec->hyperlinks, use_offset, NULL);
	result = belongs_to_link(link, use_offset);
	UnuseMemory(pg_rec->hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}


/* pgIsHyperlinkTarget returns TRUE if position is within a target hypertext link. */

PG_PASCAL (pg_boolean) pgIsHyperlinkTarget (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	link;
	long				use_offset;
	pg_boolean			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	use_offset = pgFixOffset(pg_rec, position);
	link = pgFindHypertextRun(pg_rec->target_hyperlinks, use_offset, NULL);
	result = belongs_to_link(link, use_offset);
	UnuseMemory(pg_rec->target_hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}


/* pgGetSourceURL returns the URL string, if any, contained in the hyperlink at position.
The maximum buffer size for the string is in max_size.  If found, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetSourceURL (pg_ref pg, long position, pg_char_ptr URL, short max_size)
{
	return	pgGetLinkURL(pg, FALSE, position, URL, max_size);
}

/* pgGetTargetURL returns the URL string, if any, contained in the hyperlink at position.
The maximum buffer size for the string is in max_size.  If found, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetTargetURL (pg_ref pg, long position, pg_char_ptr URL, short max_size)
{
	return	pgGetLinkURL(pg, TRUE, position, URL, max_size);
}

/* pgSetSourceURL changes the URL string in the source link. TRUE is returned
if there was a link at specified position.*/

PG_PASCAL (pg_boolean) pgSetSourceURL (pg_ref pg, long position, pg_char_ptr URL)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	pg_boolean			result = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	links = pgFindHypertextRun(pg_rec->hyperlinks, position, NULL);
	
	if (belongs_to_link(links, position)) {
	
		pgInsertURL(pg_rec, links, URL);
		++pg_rec->change_ctr;
		result = TRUE;
	}
	
	UnuseMemory(pg_rec->hyperlinks);
	UnuseMemory(pg);
	
	return	result;
}

/* pgSetTargetURL changes the URL string in the specified target link. TRUE is returned
if there was a link at specified position. */

PG_PASCAL (pg_boolean) pgSetTargetURL (pg_ref pg, long position, pg_char_ptr URL)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	pg_boolean			result = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	links = pgFindHypertextRun(pg_rec->target_hyperlinks, position, NULL);
	
	if (belongs_to_link(links, position)) {
	
		pgInsertURL(pg_rec, links, URL);
		++pg_rec->change_ctr;
		result = TRUE;
	}
	
	UnuseMemory(pg_rec->target_hyperlinks);
	UnuseMemory(pg);
	
	return	result;
}


PG_PASCAL (long) pgGetSourceID (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	long				result = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	links = pgFindHypertextRun(pg_rec->hyperlinks, position, NULL);
	
	if ((result = belongs_to_link(links, position)) == TRUE)
		result = links->unique_id;
		
	UnuseMemory(pg_rec->hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}

PG_PASCAL (long) pgGetTargetID (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	long				result = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	links = pgFindHypertextRun(pg_rec->target_hyperlinks, position, NULL);
	
	if ((result = belongs_to_link(links, position)) == TRUE)
		result = links->unique_id;
		
	UnuseMemory(pg_rec->target_hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}


/* pgFindHyperlinkTarget locates the link containing URL, if any. An exact match must be
found (case-sensitive). If found and scroll_to is TRUE then we scroll to the new position. */

PG_PASCAL (long) pgFindHyperlinkTarget (pg_ref pg, long starting_position, long PG_FAR *end_position,
		pg_char_ptr URL, pg_boolean partial_find_ok, pg_boolean case_insensitive, pg_boolean scroll_to)

{
	return	find_link(pg, TRUE, starting_position, end_position, 0, URL, partial_find_ok,
						case_insensitive, scroll_to, NULL);
}

/* pgFindHyperlinkSource is the same as pgFindHyperlinkTarget except it is for the source links. */

PG_PASCAL (long) pgFindHyperlinkSource (pg_ref pg, long starting_position, long PG_FAR *end_position,
		pg_char_ptr URL, pg_boolean partial_find_ok, pg_boolean case_insensitive, pg_boolean scroll_to)

{
	return	find_link(pg, FALSE, starting_position, end_position, 0, URL, partial_find_ok,
						case_insensitive, scroll_to, NULL);
}


/* pgFindHyperlinkTargetByID is identical to pgFindHyperlinkTarget except the search is for unique_id. */

PG_PASCAL (long) pgFindHyperlinkTargetByID (pg_ref pg, long starting_position, long PG_FAR *end_position,
		long id, pg_boolean scroll_to)
{
	return	find_link(pg, TRUE, starting_position, end_position, id, NULL, FALSE, TRUE, scroll_to, NULL);
}


/* pgFindHyperlinkSourceByID is identical to pgFindHyperlinkSource except the search is for unique_id. */

PG_PASCAL (long) pgFindHyperlinkSourceByID (pg_ref pg, long starting_position, long PG_FAR *end_position,
		long id, pg_boolean scroll_to)
{
	return	find_link(pg, FALSE, starting_position, end_position, id, NULL, FALSE, TRUE, scroll_to, NULL);
}


/* pgSetHyperlinkCallback sets the callback(s) for all links to the function pointers
specified. If one is NULL the standard is used. */

PG_PASCAL (void) pgSetHyperlinkCallback (pg_ref pg, ht_callback source_callback, ht_callback target_callback)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	ht_callback			use_source, use_target;
	long				num_links;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((use_source = source_callback) == NULL)
		use_source = pgStandardSourceCallback;
	if ((use_target = target_callback) == NULL)
		use_target = pgStandardTargetCallback;

	for (links = (pg_hyperlink_ptr) UseMemory(pg_rec->hyperlinks), num_links = GetMemorySize(pg_rec->hyperlinks);
				num_links; ++links, --num_links)
		links->callback = use_source;
	
	UnuseMemory(pg_rec->hyperlinks);

	for (links = (pg_hyperlink_ptr) UseMemory(pg_rec->target_hyperlinks), num_links = GetMemorySize(pg_rec->target_hyperlinks);
				num_links; ++links, --num_links)
		links->callback = use_target;
	
	UnuseMemory(pg_rec->target_hyperlinks);

	UnuseMemory(pg);
}


PG_PASCAL (long) pgStandardSourceCallback(paige_rec_ptr pg, pg_hyperlink_ptr hypertext,
		short command, short modifiers, long position, pg_char_ptr URL)
{
	if (command == hyperlink_mousedown_verb) {

		set_link_state(pg->myself, FALSE, -1, 0, TRUE);
		set_link_state(pg->myself, FALSE, hypertext->applied_range.begin, 1, TRUE);
	}
	
	return	0;
}


PG_PASCAL (long) pgStandardTargetCallback(paige_rec_ptr pg, pg_hyperlink_ptr hypertext,
		short command, short modifiers, long position, pg_char_ptr URL)
{
	return	0;
}

/* pgFindHypertextRun locates the closest match it can find in the hypertext link run. The
pointer is returned (and if index is non-null it gets set to the record number). */

PG_PASCAL (pg_hyperlink_ptr) pgFindHypertextRun (memory_ref hyperlinks, long offset, long PG_FAR *index)
{
	pg_hyperlink_ptr		ht_ptr;
	long					record_num;
	
	ht_ptr = (pg_hyperlink_ptr) UseMemory(hyperlinks);
	record_num = 0;
	
	while (ht_ptr->applied_range.end <= offset) {
		
		++ht_ptr;
		++record_num;
	}
	
	if (index)
		*index = record_num;

	return	ht_ptr;
}


/* pgAdvanceHyperlinks advances the hyperlink run specified. */

PG_PASCAL (void) pgAdvanceHyperlinks (paige_rec_ptr pg, memory_ref hyperlinks, long offset, long length)
{
	pg_hyperlink_ptr		ht_ptr;
	long					index, num_recs, actual_recs;

	ht_ptr = pgFindHypertextRun(hyperlinks, offset, &index);
	num_recs = GetMemorySize(hyperlinks);
	actual_recs = num_recs - 1;

	while (index < num_recs) {
		
		if (length < 0) {
			
			if (index < actual_recs) {
				long		deleted_end;
				
				deleted_end = offset - length;

				if (ht_ptr->applied_range.begin >= offset && deleted_end > ht_ptr->applied_range.begin)
					ht_ptr->applied_range.begin = deleted_end;
			}
			
			if (offset < ht_ptr->applied_range.begin)
				ht_ptr->applied_range.begin += length;
			if (offset < ht_ptr->applied_range.end)
				ht_ptr->applied_range.end += length;

			if (index < actual_recs) {

				if (ht_ptr->applied_range.begin < 0)
					ht_ptr->applied_range.begin = 0;
				
				if (ht_ptr->applied_range.end <= ht_ptr->applied_range.begin) {
					
					UnuseMemory(hyperlinks);
					delete_link_from_ref(pg, hyperlinks, index);
					
					num_recs -= 1;
					actual_recs -= 1;
					ht_ptr = (pg_hyperlink_ptr) UseMemory(hyperlinks);
					ht_ptr += index;
				}
				else {
					
					ht_ptr += 1;
					index += 1;
				}
			}
			else {

				ht_ptr += 1;
				index += 1;
			}
		}
		else {

			if (offset <= ht_ptr->applied_range.begin)
				ht_ptr->applied_range.begin += length;
			if (offset < ht_ptr->applied_range.end)
				ht_ptr->applied_range.end += length;

			++index;
			++ht_ptr;
		}
	}
	
	UnuseMemory(hyperlinks);

// debug:

	ht_ptr = (pg_hyperlink_ptr) UseMemory(hyperlinks);
	num_recs = GetMemorySize(hyperlinks);
	UnuseMemory(hyperlinks);
}

/* pgPackHyperlinks packs all the links in hyperlinks. */

PG_PASCAL (long) pgPackHyperlinks (pack_walk_ptr walker, memory_ref hyperlinks, long PG_FAR *unpack_size)
{
	pg_hyperlink_ptr		links;
	pg_char_ptr				URL;
	long					qty, index, string_length;
	
	qty = GetMemorySize(hyperlinks);
	links = (pg_hyperlink_ptr) UseMemory(hyperlinks);
	
	if (unpack_size)
		*unpack_size = qty * sizeof(pg_hyperlink);

	for (index = 0; index < qty; ++index, ++links) {
		
		pgPackNum(walker, long_data, links->applied_range.begin);
		pgPackNum(walker, long_data, links->applied_range.end);

		pgPackNum(walker, short_data, links->active_style);
		pgPackNum(walker, short_data, links->state1_style);
		pgPackNum(walker, short_data, links->state2_style);
		pgPackNum(walker, short_data, links->state3_style);
		pgPackNum(walker, long_data, links->unique_id);
		pgPackNum(walker, long_data, links->type);
		pgPackNum(walker, long_data, links->target_id);
		pgPackNum(walker, long_data, links->refcon);

		if (links->alt_URL)
			URL = (pg_char_ptr) UseMemory(links->alt_URL);
		else
			URL = links->URL;
		
		string_length = pgCStrLength(URL);
		pgPackNum(walker, long_data, string_length);
		
		if (string_length)
			pgPackBytes(walker, (pg_bits8_ptr)URL, string_length * sizeof(pg_char));

		if (links->alt_URL)
			UnuseMemory(links->alt_URL);
	}

	UnuseMemory(hyperlinks);

	return		qty;
}

/* pgUnpackHyperlinks unpacks all the links in the walker. */

PG_PASCAL (void) pgUnpackHyperlinks (paige_rec_ptr pg, pack_walk_ptr walker, ht_callback callback,
			long qty, memory_ref hyperlinks)
{
	pg_hyperlink_ptr		links;
	pg_char_ptr				URL;
	long					index, string_length;
	
	SetMemorySize(hyperlinks, qty);
	
	links = (pg_hyperlink_ptr) UseMemory(hyperlinks);
	pgFillBlock(links, qty * sizeof(pg_hyperlink), 0);

	for (index = 0; index < qty; ++index, ++links) {

		links->applied_range.begin = pgUnpackNum(walker);
		links->applied_range.end = pgUnpackNum(walker);
		links->callback = callback;
		links->active_style = (short)pgUnpackNum(walker);
		links->state1_style = (short)pgUnpackNum(walker);
		links->state2_style = (short)pgUnpackNum(walker);
		links->state3_style = (short)pgUnpackNum(walker);
		
		if (pg->version >= KEY_REVISION20) {
		
			links->unique_id = pgUnpackNum(walker);
			links->type = pgUnpackNum(walker);
			
			if (pg->version >= KEY_REVISION21)
				links->target_id = pgUnpackNum(walker);
			if (pg->version >= KEY_REVISION31)
				links->refcon = pgUnpackNum(walker);
		}
		
		string_length = pgUnpackNum(walker) / sizeof(pg_char);

		if (string_length) {
			
			if (string_length > (FONT_SIZE - 1)) {
				
				links->alt_URL = MemoryAllocClearID(GetGlobalsFromRef(hyperlinks),
									sizeof(pg_char), string_length + 1, 0, pg->mem_id);
				URL = (pg_char_ptr) UseMemory(links->alt_URL);
			}
			else
				URL = links->URL;
			
			pgUnpackPtrBytes(walker, (pg_bits8_ptr)URL);

			if (links->alt_URL)
				UnuseMemory(links->alt_URL);
		}
	}

	UnuseMemory(hyperlinks);
}

/* pgNewHyperlinkStyle adds a new hypelink stylesheet, returning its ID. If it already exists
the ID is returned. The red through blue params indicate the color while stylebits indicates
the style (such as underline). If background is TRUE then the specified colors are applied
to the background color (otherwise the foreground is affected). */

PG_PASCAL (short) pgNewHyperlinkStyle (pg_ref pg, pg_short_t red, pg_short_t green,
		pg_short_t blue, long stylebits, pg_boolean background)
{
	paige_rec_ptr		pg_rec;
	style_info			style, mask;
	color_value_ptr		color;
	short				style_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgFillBlock(&style, sizeof(style_info), 0);
	style.procs = pg_rec->globals->def_style.procs;
	UnuseMemory(pg);
	
	if (background)
		color = &style.bk_color;
	else
		color = &style.fg_color;

	color->red = red;
	color->green = green;
	color->blue = blue;
	QDStyleToPaige(stylebits, &style);

	pgFillBlock(&mask, sizeof(style_info), 0);
	pgFillBlock(mask.styles, sizeof(short) * MAX_STYLES, -1);
	pgFillBlock(&mask.bk_color, sizeof(color_value), -1);
	pgFillBlock(&mask.fg_color, sizeof(color_value), -1);
	
	if ((style_id = pgFindStyleSheet(pg, &style, &mask)) > 0)
		return	style_id;

	return	pgNewStyle(pg, &style, NULL);
}

/* pgScrollToLink scrolls to the specified text position. */

PG_PASCAL (void) pgScrollToLink (pg_ref pg, long text_position)
{
	paige_rec_ptr		pg_rec;
	rectangle			char_rect, vis_bounds;
	short				retries;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgShapeBounds(pg_rec->vis_area, &vis_bounds);
	vis_bounds.top_left.v += 8;
	vis_bounds.bot_right.v = vis_bounds.top_left.v + 24;

	for (retries = 3; retries; --retries) {

		pgCharacterRect(pg, text_position, TRUE, FALSE, &char_rect);
		
		if (char_rect.top_left.v >= vis_bounds.top_left.v
			&& char_rect.bot_right.v <= vis_bounds.bot_right.v)
				break;

		pgScrollPixels(pg, 0, vis_bounds.top_left.v - char_rect.top_left.v, best_way);
	}

	for (retries = 3; retries; --retries) {

		pgCharacterRect(pg, text_position, TRUE, FALSE, &char_rect);
		
		if (char_rect.top_left.h >= vis_bounds.top_left.h
			&& char_rect.bot_right.h <= vis_bounds.bot_right.h)
				break;

		pgScrollPixels(pg, vis_bounds.top_left.h - char_rect.top_left.h, 0, best_way);
	}
	
	UnuseMemory(pg);
}

/* pgInitDefaultSource initializes the defaults for a source hyperlink. */

PG_PASCAL (void) pgInitDefaultSource (pg_ref pg, pg_hyperlink_ptr link)
{
	pgFillBlock(link, sizeof(pg_hyperlink), 0);

	link->state1_style = pgNewHyperlinkStyle(pg, 0, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);
	link->state2_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, 0, X_UNDERLINE_BIT, FALSE);
	link->state3_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);

	link->callback = pgStandardSourceCallback;
}

/* pgInitDefaultSource initializes the defaults for a target hyperlink. */

PG_PASCAL (void) pgInitDefaultTarget (pg_ref pg, pg_hyperlink_ptr link)
{
	pgFillBlock(link, sizeof(pg_hyperlink), 0);

	link->state1_style = pgNewHyperlinkStyle(pg, 0xFC00, 0xFD00, 0x7C00, 0, TRUE);
	link->callback = pgStandardTargetCallback;
}

/* pgAssignLinkID returns an ID that is not present in any hypertext linkin existing_links. */

PG_PASCAL (long) pgAssignLinkID (memory_ref existing_links)
{
	long				id;
	
	id = GetMemorySize(existing_links) + 1;
	return	pgValidateID(existing_links, id);
}

/* pgValidateID checks id to make sure it is unique.  If it is not then one is returned
that is unique. */

PG_PASCAL (long) pgValidateID (memory_ref existing_links, long id)
{
	while (find_id(existing_links, id))
		id += 1;
	
	return		id;
}

/* pgInsertURL builds a URL string in the link. */

PG_PASCAL (void) pgInsertURL (paige_rec_ptr pg, pg_hyperlink_ptr hyperlink, pg_char_ptr URL)
{
	long		length;

	length = pgCStrLength(URL);
	pgFillBlock(hyperlink->URL, FONT_SIZE, 0);

	if (hyperlink->alt_URL)
		DisposeMemory(hyperlink->alt_URL);
	hyperlink->alt_URL = MEM_NULL;

	if (length > (FONT_SIZE - 1)) {
	
		hyperlink->alt_URL = MemoryAllocClearID(pg->globals->mem_globals, sizeof(pg_char), length + 1, 0, pg->mem_id);
		pgBlockMove(URL, UseMemory(hyperlink->alt_URL), length);
		UnuseMemory(hyperlink->alt_URL);
	}
	else
		pgBlockMove(URL, hyperlink->URL, length + 1);
}


/*********************************** Local Functions **********************************/

static void insert_hyperlink (paige_rec_ptr pg, memory_ref hyperlink_run, select_pair_ptr selection,
			pg_hyperlink_ptr hyperlink, pg_char_ptr URL, pg_char_ptr insert_option, short draw_mode)
{
	pg_hyperlink_ptr	ht_ptr;
	long				insert_pos;

	if (selection == NULL)
		pgGetSelection(pg->myself, &hyperlink->applied_range.begin, &hyperlink->applied_range.end);
	else
		hyperlink->applied_range = *selection;
	
	delete_overlapping_links(pg, hyperlink_run, &hyperlink->applied_range);

	if (URL)
		pgInsertURL(pg, hyperlink, URL);

	if (insert_option) {
		long			keyword_length;

		keyword_length = pgCStrLength(insert_option);
		insert_pos = hyperlink->applied_range.begin;
		pgInsert(pg->myself, insert_option, keyword_length, insert_pos, data_insert_mode, 0, draw_none);
		hyperlink->applied_range.end = insert_pos + keyword_length;
	}
	
	pgFindHypertextRun(hyperlink_run, hyperlink->applied_range.begin, &insert_pos);
	ht_ptr = (pg_hyperlink_ptr) InsertMemory(hyperlink_run, insert_pos, 1);
	*ht_ptr = *hyperlink;
	UnuseMemory(hyperlink_run);
	
	pgInvalSelect(pg->myself, hyperlink->applied_range.begin, hyperlink->applied_range.end);
	
	if (draw_mode) {
		short		use_draw_mode = draw_mode;
		
		if (use_draw_mode = best_way)
			use_draw_mode = bits_copy;
		
		pgUpdateText(pg, NULL, 0, pg->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
	}
}

/* belongs_to_link returns TRUE if offset belongs to the link. */

static pg_boolean belongs_to_link (pg_hyperlink_ptr link, long offset)
{
	return	(pg_boolean)(offset >= link->applied_range.begin && offset < link->applied_range.end);
}

/* get_URL_ptr returns a pointer to the link's URL. This does a UseMemory() if necessary. */

static pg_char_ptr get_URL_ptr (pg_hyperlink_ptr link)
{
	if (link->alt_URL == MEM_NULL)
		return	link->URL;
	
	return (pg_char_ptr) UseMemory(link->alt_URL);
}

/* compare_URL compares two strings and returns TRUE if they match. */

static pg_boolean compare_URL (pg_char_ptr str1, pg_char_ptr str2, pg_boolean partial_find, pg_boolean ignore_case)
{
	register pg_char_ptr	string1, string2;
	
	string1 = str1;
	string2 = str2;
	
	if (ignore_case) {

		while (*string1 != 0 || *string2 != 0) {
			pg_char		char1, char2;
			
			char1 = *string1++;
			char2 = *string2++;
			
			if (char1 >= 'a')
				if (char1 <= 'z')
					char1 -= 0x20;
			if (char2 >= 'a')
				if (char2 <= 'z')
					char2 -= 0x20;

			if (char1 != char2)
				return	FALSE;
			
			if (partial_find)
				if (*string1 == 0)
					break;
		}
	}
	else {
	
		while (*string1 != 0 || *string2 != 0) {
			
			if (*string1++ != *string2++)
				return	FALSE;
			
			if (partial_find)
				if (*string1 == 0)
					break;
		}
	}

	return	TRUE;
}

/* set_link_state gets called by the global function to change the display state.
If the position is negative, the state of ALL are set. */

static void set_link_state (pg_ref pg, pg_boolean for_target, long position, short state, pg_boolean redraw)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	memory_ref			hyperlinks;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;
	
	if (position < 0) {
		long			num_links, index, first_update, last_update;
		short			new_style;
		pg_boolean		changed_some;

		links = (pg_hyperlink_ptr) UseMemory(hyperlinks);
		num_links = GetMemorySize(hyperlinks) - 1;
		changed_some = FALSE;
		first_update = last_update = links->applied_range.begin;

		for (index = 0; index < num_links; ++index, ++links) {

			switch (state) {
				
				case 0:
					new_style = links->state1_style;
					break;
					
				case 1:
					new_style = links->state2_style;
					break;

				case 2:
					new_style = links->state3_style;
					break;
			}
			
			if (new_style != links->active_style) {
				
				if (!changed_some) {
					
					first_update = links->applied_range.begin;
					last_update = links->applied_range.end;
				}
				
				changed_some = TRUE;
				links->active_style = new_style;
				
				if (links->applied_range.end > last_update)
					last_update = links->applied_range.end;
			}
		}

		if (changed_some && redraw)
			pgUpdateText(pg_rec, NULL, get_update_beginning(pg_rec, first_update), last_update,
				MEM_NULL, NULL, bits_copy, TRUE);
	}
	else {
	
		links = pgFindHypertextRun(hyperlinks, position, NULL);
		
		if (belongs_to_link(links, position)) {
			
			switch (state) {
				
				case 0:
					links->active_style = links->state1_style;
					break;
					
				case 1:
					links->active_style = links->state2_style;
					break;

				case 2:
					links->active_style = links->state3_style;
					break;
			}
			
			if (redraw)
				pgUpdateText(pg_rec, NULL, get_update_beginning(pg_rec, links->applied_range.begin),
						links->applied_range.end, MEM_NULL, NULL, bits_copy, TRUE);
		}
	}

	UnuseMemory(hyperlinks);
	UnuseMemory(pg);
}

/* delete_link gets called by the global delete-hyperlink function(s). */

static void delete_link (pg_ref pg, pg_boolean for_target, long position, pg_boolean redraw)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	pg_hyperlink		old_link;
	memory_ref			hyperlinks;
	long				index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;

	links = pgFindHypertextRun(hyperlinks, position, &index);
	old_link = *links;
	UnuseMemory(hyperlinks);
	
	if (belongs_to_link(&old_link, position)) {
		
		delete_link_from_ref(pg_rec, hyperlinks, index);
		
		pgInvalSelect(pg, old_link.applied_range.begin, old_link.applied_range.end);
		
		if (redraw)
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, bits_copy, TRUE);
	}

	UnuseMemory(pg);
}


/* get_link_info gets called by the "getinfo" global function(s). */

static pg_boolean get_link_info (pg_ref pg, pg_boolean for_target, long position, pg_boolean closest_one,
			pg_hyperlink_ptr hyperlink)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	memory_ref			hyperlinks;
	pg_boolean			result = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;

	links = pgFindHypertextRun(hyperlinks, position, NULL);
	
	if (links->applied_range.begin < pg_rec->t_length) {
		
		if (closest_one || belongs_to_link(links, position)) {
			
			result = TRUE;
			
			if (hyperlink)
				*hyperlink = *links;
		}
	}

	UnuseMemory(hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}

/* pt_in_link gets called by the global functions to learn if a point is in a link. */

static long pt_in_link (pg_ref pg, pg_boolean for_target, co_ordinate_ptr point)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	memory_ref			hyperlinks;
	long				position, result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;

	position = pgPtToChar(pg, point, NULL);
	links = pgFindHypertextRun(hyperlinks, position, NULL);
	
	if (belongs_to_link(links, position))
		result = links->applied_range.begin;
	else
		result = -1;
	
	UnuseMemory(hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}

/* pgGetLinkURL is called by the global functions that return a string URL. */

PG_PASCAL (pg_boolean) pgGetLinkURL (pg_ref pg, pg_boolean for_target, long position, pg_char_ptr URL, short max_size)
{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	pg_char_ptr			string;
	memory_ref			hyperlinks;
	pg_boolean			result;
	short				string_size;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;

	links = pgFindHypertextRun(hyperlinks, position, NULL);
	
	if ((result = belongs_to_link(links, position)) == TRUE) {
		
		string = get_URL_ptr(links);
		string_size = (short)pgCStrLength(string);

		if (string_size > (max_size - 1))
			string_size = max_size - 1;
		
		pgBlockMove(string, URL, string_size);
		URL[string_size] = 0;

		if (links->alt_URL)
			UnuseMemory(links->alt_URL);
	}

	UnuseMemory(hyperlinks);
	UnuseMemory(pg);
	
	return		result;
}

/* find_link gets called by the "Find" global functions. */
/* PDA:  Added start_position to begin search from. */

static long find_link (pg_ref pg, pg_boolean for_target, long start_position, long PG_FAR *end_position,
		long use_ID, pg_char_ptr URL, pg_boolean partial_find, pg_boolean ignore_case, pg_boolean scroll_to,
		pg_hyperlink_ptr hyperlink_record)

{
	paige_rec_ptr		pg_rec;
	pg_hyperlink_ptr	links;
	pg_char_ptr			url_ptr;
	memory_ref			hyperlinks;
	pg_boolean			comparison;
	long				result = -1;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (for_target)
		hyperlinks = pg_rec->target_hyperlinks;
	else
		hyperlinks = pg_rec->hyperlinks;

	links = (pg_hyperlink_ptr) UseMemory(hyperlinks);
   /* PDA:  fixed bug! */
	while (links->applied_range.begin < start_position)
      ++links;
   /* PDA */
	while (links->applied_range.begin < pg_rec->t_length) {
		
		if (use_ID)
			comparison = (pg_boolean)(links->unique_id == use_ID);
		else {
		
			url_ptr = get_URL_ptr(links);
			
			comparison = compare_URL(URL, url_ptr, partial_find, ignore_case);

			if (links->alt_URL)
				UnuseMemory(links->alt_URL);
		}

		if (comparison) {
		
			result = links->applied_range.begin;
			
			if (end_position)
				*end_position = links->applied_range.end;
			
			if (hyperlink_record)
				*hyperlink_record = *links;

			break;
		}
		
		++links;
	}

	UnuseMemory(hyperlinks);

	if (scroll_to && result >= 0)
		pgScrollToLink(pg, result);

	UnuseMemory(pg);

	return		result;
}


/* get_update_beginning returns the place we should update given the text position (i.e.
returns start of a line). */

static long get_update_beginning (paige_rec_ptr pg, long position)
{
	text_block_ptr			block;
	point_start_ptr			starts;
	long					first_update;
	pg_short_t				local_position;
	
	block = pgFindTextBlock(pg, position, NULL, TRUE, TRUE);
	starts = (point_start_ptr) UseMemory(block->lines);
	local_position = (pg_short_t)(position - block->begin);
	
	while (starts[1].offset <= local_position) {
		
		if (starts[1].flags == TERMINATOR_BITS)
			break;
		
		++starts;
	}
	
	while (!(starts->flags & NEW_LINE_BIT)) {
		
		if (starts->offset == 0)
			break;
		
		--starts;
	}
	
	first_update = (long)starts->offset;
	first_update += block->begin;
	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);
	
	return		first_update;
}

/* find_id returns the record number + 1 of links_ref if it contains id. */

static long find_id (memory_ref links_ref, long id)
{
	pg_hyperlink_ptr	ht_ptr;
	long				qty, index;
	
	qty = GetMemorySize(links_ref);
	ht_ptr = (pg_hyperlink_ptr) UseMemory(links_ref);
	
	for (index = 1; index <= qty; ++index, ++ht_ptr)
		if (ht_ptr->unique_id == id) {
		
		UnuseMemory(links_ref);
		
		return	index;
	}
	UnuseMemory(links_ref);
	
	return		0;
}

/* delete_overlapping_links removes all hyperlinks that will overlap with the will-be selection
for a new link in the overlap param. If necessary overlap->begin and-or overlap->end is adjusted
to "extend" the will-be new link. */

static void delete_overlapping_links (paige_rec_ptr pg, memory_ref links_ref, select_pair_ptr overlap)
{
	pg_hyperlink_ptr	ht_ptr;
	long				qty, index;

	qty = GetMemorySize(links_ref) - 1;
	ht_ptr = (pg_hyperlink_ptr) UseMemory(links_ref);
	index = 0;
	
	while (index < qty) {
		
		if (overlap->begin <= ht_ptr->applied_range.end && overlap->end >= ht_ptr->applied_range.begin) {
			
			if (ht_ptr->applied_range.begin < overlap->begin)
				overlap->begin = ht_ptr->applied_range.begin;
			if (ht_ptr->applied_range.end > overlap->end)
				overlap->end = ht_ptr->applied_range.end;
			
			UnuseMemory(links_ref);
			delete_link_from_ref(pg, links_ref, index);
			ht_ptr = (pg_hyperlink_ptr) UseMemory(links_ref);
			ht_ptr += index;
			
			qty -= 1;
		}
		else {
			
			++index;
			++ht_ptr;
		}
	}

	UnuseMemory(links_ref);
}

/* delete_link_from_ref deletes the hyperlink making the necessary callbacks. */

static void delete_link_from_ref (paige_rec_ptr pg, memory_ref links_ref, long index)
{
	pg_hyperlink	deleted_link;
	
	GetMemoryRecord(links_ref, index, &deleted_link);
	
	deleted_link.callback(pg, &deleted_link, hyperlink_delete_verb, 0,
				deleted_link.applied_range.begin, get_URL_ptr(&deleted_link));
	
	if (deleted_link.alt_URL)
		UnuseAndDispose(deleted_link.alt_URL);

	DeleteMemory(links_ref, index, 1);
}
