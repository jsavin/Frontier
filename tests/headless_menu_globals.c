/*
 * headless_menu_globals.c - Menu editor globals for headless build
 *
 * These globals are normally defined in menueditor.c, which is a GUI-only file.
 * In headless mode, menuverbs.c and menupack.c need these globals but don't
 * need the full GUI implementation.
 */

#ifdef FRONTIER_HEADLESS

#include "frontier.h"
#include "standard.h"
#include "menueditor.h"

/* Globals normally defined in menueditor.c (GUI file) */
hdlmenurecord menudata = nil;
WindowPtr menuwindow = nil;
hdlwindowinfo menuwindowinfo = nil;

#endif /* FRONTIER_HEADLESS */
