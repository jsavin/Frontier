/* Header file that goes with pgStyles.c.  */

#ifndef PGSTYLES_H
#define PGSTYLES_H

#include "Paige.h"

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgChangeStyleInfo (paige_rec_ptr pg_rec, select_pair_ptr selection,
		style_info_ptr info, style_info_ptr mask,  short draw_mode);
extern PG_PASCAL (pg_boolean) pgBasicallyEqualStyles (style_info_ptr src, style_info_ptr target);
extern PG_PASCAL (pg_boolean) pgBasicallyEqualFonts (font_info_ptr src, font_info_ptr target);

#ifdef __cplusplus
	};
#endif

#endif
