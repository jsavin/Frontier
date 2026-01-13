/*
 * headless_opattributes_verbs.h - Public API for opattributes processor stubs
 *
 * Exposes token enum and initialization function for opattributes verbs.
 * This header provides a single source of truth for the public API, avoiding
 * duplication between implementation and test files.
 *
 * Created: 2026-01-12
 * Reason: Bot review suggestion - expose enum in header for maintainability
 */

#ifndef HEADLESS_OPATTRIBUTES_VERBS_H
#define HEADLESS_OPATTRIBUTES_VERBS_H

#include "frontier.h"
#include "standard.h"

/* Token enum for all verbs in the opattributes processor */
enum {
    opav_addgroup = 0,
    opav_getall = 1,
    opav_getone = 2,
    opav_makeempty = 3,
    opav_setone = 4
};

/* Function declarations */
extern boolean opattributes_valueproc(short token, hdltreenode hparam1,
                                      tyvaluerecord *vreturned,
                                      bigstring bserror);

extern boolean opattributesinitverbs(void);

#endif /* HEADLESS_OPATTRIBUTES_VERBS_H */
