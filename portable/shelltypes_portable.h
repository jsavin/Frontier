#ifndef SHELLTYPES_PORTABLE_H
#define SHELLTYPES_PORTABLE_H

/* Portable version of shelltypes.h for the language engine */
/* This provides only the essential types needed by the language engine */

#include "portable_types.h"

/* Font name length constant */
#define diskfontnamelength 32 /*number of bytes for a font name stored on disk*/

/* Font string type */
typedef char diskfontstring [diskfontnamelength + 1];

/* Essential types for the language engine */
typedef struct tyfilespec {
    struct {
        boolean flvolume; /* true: ref points to volume -- false: ref points to parent */
    } flags;
    FSRef ref;
    tyfsname name;
} tyfilespec;

typedef tyfilespec *ptrfilespec, **hdlfilespec;

/* Handle types for arrays and other data structures */
typedef struct { void* data; } hdlintarray;
typedef struct { void* data; } hdlstringarray;
typedef struct { void* data; } hdlbooleanarray;
typedef struct { void* data; } hdlrealarray;
typedef struct { void* data; } hdlptrarray;

/* Note: tyfunctype, tyvaluetype, and tytokentype are defined in the core language headers */
/* Do not redefine them here to avoid conflicts */

/* Process ID type */
#ifndef TYPROCESSID_DEFINED
typedef ProcessSerialNumber typrocessid;
#define TYPROCESSID_DEFINED 1
#endif

/* Button status type */
typedef struct tybuttonstatus {
    boolean fldisplay; /* should button be displayed at all? */
    boolean flenabled; /* if displayed, is it active (or dimmed)? */
    boolean flbold; /* if displayed, should text style be bold? */
} tybuttonstatus;

/* Text display info type */
typedef struct tytextdisplayinfo {
    short h; /* the horizontal pen position for all lines */
    short v; /* the vertical pen position for the first line */
    short lh; /* the uniform lineheight of all lines */
    short screenlines; /* the number of lines that fit within the current window's rectangle */
    Rect r; /* the rectangle within which everything is displayed */
    short horizscrollpixels; /* number of pixels to scroll by for each horiz scroll */
} tytextdisplayinfo;

/* Popup record type */
typedef struct typopuprecord {
    Rect popuprect; /* where the whole popup structure is displayed */
} typopuprecord, *ptrpopuprecord, **hdlpopuprecord;

/* Disk types */
typedef struct diskrect {
    short top, left, bottom, right;
} diskrect;

typedef struct diskrgb {
    short red, green, blue;
} diskrgb;

/* Scrollbar and menu handles */
typedef ControlHandle hdlscrollbar;
typedef MenuHandle hdlmenu;

/* Script type */
typedef OSType tyscraptype;

/* Click and key flags */
typedef enum clickflags {
    clicknormal = 0,
    clickextend = 0x0001, /* extend the selection */
    clickwords = 0x0002, /* select whole words only */
    clickparas = 0x0004, /* select whole paragraphs only */
    clicklines = 0x0008, /* highlight whole lines */
    clickvertical = 0x0010, /* allow vertical selection */
    clickdiscontiguous = 0x0020, /* enable discontiguous selection */
    clickstyle = 0x0040, /* select whole style range */
    clickcontrol = 0x0200, /* word advance for arrows, Home-End to doc top and bottom */
    clickoption = 0x0400, /* option key held down */
    clickcommand = 0x0800 /* alt key (Windows) or command key (Mac) */
} tyclickflags;

typedef enum tykeyflags {
    keynormal = 0,
    keyshift = 0x0001,
    keycontrol = 0x0200,
    keyoption = 0x0400,
    keycommand = 0x0800
} tykeyflags;

#endif /* SHELLTYPES_PORTABLE_H */
