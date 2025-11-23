/* This header file goes with pgParagraph.h     */

#ifndef PGPAR_H
#define PGPAR_H

#include "Paige.h"

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgGetScreenOffsets (paige_rec_ptr pg, long first_select,
		long base_to_use, rectangle_ptr bounds);
extern PG_PASCAL (pg_boolean) pgBasicallyEqualPars (par_info_ptr src, par_info_ptr target);

#ifdef __cplusplus
	}
#endif

#endif
