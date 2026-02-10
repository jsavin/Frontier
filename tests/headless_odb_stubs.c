#include <stdio.h>

#include "frontier.h"
#include "standard.h"
#include "dialogs.h"
#include "file.h"
#include "odbinternal.h"
#include "tablestructure.h"
#include "db_format.h"
#include "db.h"

/* Minimal headless stubs to satisfy ODB engine dependencies in tests */

boolean alertdialog (bigstring s) { (void)s; return true; }

WindowPtr shellfindfilewindow (const ptrfilespec fs) { (void)fs; return (WindowPtr)0; }

boolean shellsave (WindowPtr w) { (void)w; return true; }

/* ADR-005: Forward declare for thread globals - real definition elsewhere */
typedef struct tythreadglobals tythreadglobals, *ptrthreadglobals, **hdlthreadglobals;

/* hthreadglobals is the global pointer to current thread's globals (headless_threadglobals.c) */
extern hdlthreadglobals hthreadglobals;

hdlthreadglobals getcurrentthreadglobals (void) { return hthreadglobals; }

void copythreadglobals (hdlthreadglobals ht) { (void)ht; }

void swapinthreadglobals (hdlthreadglobals ht) { (void)ht; }

boolean tablevalidate (hdlhashtable ht, boolean fl) { (void)ht; (void)fl; return true; }

/* db_migrate_reopen_if_legacy is now provided by dbverbs.c */
