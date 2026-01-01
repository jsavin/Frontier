/*
 * headless_stubs.c
 *
 * Global variable definitions for headless mode stubs.
 * Declarations are in headless_stubs.h.
 */

#include "frontier.h"

/* Cancoon (About Window) stub - not used in headless mode
 * odbengine.c has its own static cancoonglobals, so this is only
 * defined when ODBENGINE_PROVIDES_CANCOONGLOBALS is not set */
#ifndef ODBENGINE_PROVIDES_CANCOONGLOBALS
hdlcancoonrecord cancoonglobals = NULL;
#endif
