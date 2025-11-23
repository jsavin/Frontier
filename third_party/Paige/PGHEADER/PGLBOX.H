/* Header file for PAIGE Listboxes. */

#ifndef PGLBOX_H
#define PGLBOX_H

#include "Paige.h"

extern PG_PASCAL (pg_ref) pgNewListbox (pg_globals_ptr globals, generic_var window, rectangle_ptr bounds);
extern PG_PASCAL (void) pgSetListboxFont (pg_ref listbox, select_pair_ptr range,
			pg_char_ptr fontname, short pointsize);
extern PG_PASCAL (void) pgInsertListboxItem (pg_ref listbox, pg_char_ptr item,
			long insert_before, pg_boolean redraw);
extern PG_PASCAL (void) pgInsertListboxText (pg_ref listbox, pg_char_ptr text,
			long textsize, long insert_before, pg_boolean redraw);
extern PG_PASCAL (long) pgNumElements (pg_ref listbox);
extern PG_PASCAL (long) pgNumSelectedElements (pg_ref listbox, select_pair_ptr range_selected);
extern PG_PASCAL (memory_ref) pgGetElementText (pg_ref listbox, select_pair_ptr range);
extern PG_PASCAL (void) pgSetListboxElement (pg_ref listbox, long element_num, short modifiers);
extern PG_PASCAL (void) pgSetListboxTabs (pg_ref listbox, short PG_FAR *tabs, short num_tabs);
extern PG_PASCAL (void) pgElementToOffset (pg_ref listbox, long element_num, long PG_FAR *begin
			long PG_FAR *end);
extern PG_PASCAL (long) pgOffsetToElement (pg_ref listbox, long offset);


#endif
