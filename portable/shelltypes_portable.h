#ifndef SHELLTYPES_PORTABLE_H
#define SHELLTYPES_PORTABLE_H

/*
 * Portable substitute for shelltypes.h.
 * When FRONTIER_PORTABLE is defined we provide lightweight versions of the
 * GUI/FS structures the core expects. When it's not defined, we simply ensure
 * the legacy headers know the types are already available.
 */

#if defined(FRONTIER_PORTABLE)

#include "../Common/headers/osincludes_portable.h"
#include "portable_types.h"

/* Font name length constant */
#define diskfontnamelength 32
typedef char diskfontstring[diskfontnamelength + 1];

typedef struct tyfilespec {
    struct {
        boolean flvolume;
    } flags;
    FSRef ref;
    tyfsname name;
} tyfilespec;

typedef tyfilespec *ptrfilespec, **hdlfilespec;

/* Array handle types - aligned with production definitions
 * Changed from opaque struct to match actual memory.c usage */
typedef short **hdlintarray;

/* TODO: These types appear unused and may be candidates for removal (see issue for audit) */
typedef struct { void *data; } hdlstringarray;
typedef struct { void *data; } hdlbooleanarray;
typedef struct { void *data; } hdlrealarray;
typedef struct { void *data; } hdlptrarray;

#ifndef TYPROCESSID_DEFINED
typedef ProcessSerialNumber typrocessid;
#define TYPROCESSID_DEFINED 1
#endif

typedef struct tybuttonstatus {
    boolean fldisplay;
    boolean flenabled;
    boolean flbold;
} tybuttonstatus;

typedef struct tytextdisplayinfo {
    short h;
    short v;
    short lh;
    short screenlines;
    Rect r;
    short horizscrollpixels;
} tytextdisplayinfo;

typedef struct typopuprecord {
    Rect popuprect;
} typopuprecord, *ptrpopuprecord, **hdlpopuprecord;

typedef struct diskrect {
    short top, left, bottom, right;
} diskrect;

typedef struct diskrgb {
    short red, green, blue;
} diskrgb;

typedef ControlHandle hdlscrollbar;
typedef MenuHandle hdlmenu;
typedef OSType tyscraptype;

typedef enum tyclickflags {
    clicknormal = 0,
    clickextend = 0x0001,
    clickwords = 0x0002,
    clickparas = 0x0004,
    clicklines = 0x0008,
    clickvertical = 0x0010,
    clickdiscontiguous = 0x0020,
    clickstyle = 0x0040,
    clickcontrol = 0x0200,
    clickoption = 0x0400,
    clickcommand = 0x0800
} tyclickflags;

typedef enum tykeyflags {
    keynormal = 0,
    keyshift = 0x0001,
    keycontrol = 0x0200,
    keyoption = 0x0400,
    keycommand = 0x0800
} tykeyflags;

#ifndef PORTABLE_WINDOWINFO_FORWARD
typedef struct tywindowinfo tywindowinfo;
typedef tywindowinfo *ptrwindowinfo;
typedef tywindowinfo **hdlwindowinfo;
#define PORTABLE_WINDOWINFO_FORWARD 1
#endif

#ifndef PORTABLE_FILEINFO_DEFINED
typedef enum tyfolderview {
    viewbysmallicon = 0,
    viewbyicon = 1,
    viewbyname = 2,
    viewbydate = 3,
    viewbysize = 4,
    viewbykind = 5,
    viewbycomment = 6,
    viewbycolor = 7,
    viewbyversion = 8
} tyfolderview;

typedef struct tyfileinfo {
    OSErr errcode;
    short vnum;
    long dirid;
    boolean flfolder;
    boolean fllocked;
    boolean flbundle;
    boolean flbusy;
    boolean flalias;
    boolean flinvisible;
    boolean flvolume;
    boolean flejectable;
    boolean flstationery;
    boolean flshared;
    boolean flnamelocked;
    boolean flcustomicon;
    boolean flhardwarelock;
    boolean flremotevolume;
    boolean flsystem;
    boolean flarchive;
    boolean flcompressed;
    boolean fltemp;
    OSType filecreator, filetype;
    unsigned long timecreated, timemodified, timeaccessed;
    unsigned long long sizedataforkhigh, sizedatafork, sizeresourcefork;
    short ixlabel;
    Point iconposition;
    unsigned long ctfiles;
    unsigned long ctfolders;
    tyfolderview folderview;
    unsigned long ctfreebytes;
    unsigned long cttotalbytes;
    unsigned long blocksize;
} tyfileinfo;
#define PORTABLE_FILEINFO_DEFINED 1
#endif

#ifndef SHELLTYPES_DEFINED
#define SHELLTYPES_DEFINED 1
#endif

#else /* !FRONTIER_PORTABLE */

#ifndef SHELLTYPES_DEFINED
#define SHELLTYPES_DEFINED 1
#endif

#endif /* FRONTIER_PORTABLE */

#endif /* SHELLTYPES_PORTABLE_H */
