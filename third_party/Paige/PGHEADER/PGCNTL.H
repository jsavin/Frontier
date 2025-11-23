/* This header file is platform-specific for Windows 3.x or greater.

Include this header file when you use PAIGE "edit" control window class.
Copyright 1995 by DataPak Software, Inc. All rights reserved. Users of this
software are subject to DataPak Software's license agreement.

Modified 6/28/95, include re-definitions for Macintosh version of the control. */

#ifndef PGCNTL_H
#define PGCNTL_H

#include "CPUDefs.h"
#include "pgMemMgr.h"
#include "pgWinDef.h"

#ifdef WINDOWS_PLATFORM
#include <windows.h>
#endif

// PAIGE text format bit definitions:

#define PGSTYLE_BOLD					0x00000001
#define	PGSTYLE_ITALIC					0x00000002
#define PGSTYLE_UNDERLINE				0x00000004
#ifdef MAC_PLATFORM
#define PGSTYLE_OUTLINE					0x00000008
#define PGSTYLE_SHADOW					0x00000010
#define PGSTYLE_CONDENSE				0x00000020
#define PGSTYLE_EXTEND					0x00000040
#endif
#define PGSTYLE_DBLUNDERLINE			0x00000080
#define PGSTYLE_WORDUNDERLINE			0x00000100
#define PGSTYLE_DOTTEDUNDERLINE			0x00000200
#define PGSTYLE_HIDDENTEXT				0x00000400
#define PGSTYLE_STRIKEOUT				0x00000800
#define PGSTYLE_SUPERSCRIPT				0x00001000
#define PGSTYLE_SUBSCRIPT				0x00002000
#define PGSTYLE_ALLCAPS					0x00008000
#define PGSTYLE_ALLLOWER				0x00010000
#define PGSTYLE_SMALLCAPS				0x00020000
#define PGSTYLE_OVERLINE				0x00040000
#define PGSTYLE_BOXED_BIT				0x00080000
#define PGSTYLE_NESTED_BIT				0x00800000  // ** FOR DSI ONLY, DO NOT USE

// Line info bits for line-draw notification

#define PGL_PARENDFLAG			0x4000	/* Paragraph ends here */
#define PGL_NEWPARFLAG			0x0200	/* New paragraph starts here */
#define PGL_CONTAINERBREAKFLAG	0x0008	/* Line breaks for next container */
#define PGL_PAGEBREAKFLAG		0x0004	/* Line broke for whole repeater shape */

// Indentation verbs:

enum {
	PGI_LEFT,							/* Left indent */
	PGI_RIGHT,							/* Right indent */
	PGI_FIRSTLINE						/* First line indent */
};


// Justification versb:

enum {
	PGJ_LEFTJUSTIFY,
	PGJ_CENTERJUSTIFY,
	PGJ_RIGHTJUSTIFY,
	PGJ_FULLJUSTIFY
};

// Verbs used in PG_SETCARET:

enum {
	PGCARET_ABSOLUTE,				/* Set to lParam text position */
	PGCARET_RELATIVE,				/* Set relative to text position in lParam */
	PGCARET_HOME,					/* Set to top of document */
	PGCARET_BOTTOM,					/* Set to bottom of document */
	PGCARET_BEGINLINE,				/* Set to beginning of current line */
	PGCARET_ENDLINE,				/* Set to end of current line */
	PGCARET_TOPVIS,					/* Set to top of visible area */
	PGCARET_BOTVIS					/* Set to bottom of visible area */
};


// PAIGE drawing modes (re-defined from PAIGE.H):

enum {
	PGD_DRAWNONE,						/* Do not draw at all */
	PGD_BESTWAY,						/* Use most effecient method(s) */
	PGD_OPAQUE,							/* Directly to screen, overwrite */
	PGD_TEXTOR,							/* Directly to screen, "OR" */
	PGD_TEXTXOR,						/* Directly to screen, "XOR" */
	PGD_BITSCOPY,						/* Copy offscreen */
	PGD_BITSOR,							/* Copy offscreen in "OR" mode */
	PGD_BITSXOR,						/* Copy offscreen in "XOR" mode */
	PGD_EMULATEBITSCOPY,				/* Fake bitmap, COPY */
	PGD_EMULATEBITSOR,					/* Fake bitmap, OR */
	PGD_EMULATEBITSXOR					/* Fake bitmap, XOR */
};

// Page break codes:

enum {
	PGBREAK_PAGE,				// Repeater shape jump
	PGBREAK_CONTAINER			// RECT within repeater jump
};


// Verbs for PG_COPYTEXT message:

#define COPY_ALL_BYTES			0		/* Copy all characters in selection */
#define COPY_NON_HIDDEN_BYTES	3		/* Copy all characters not set to hidden text style */

// Embedded graphic types:

#define PGEMBED_OLE           	7     	/* OLE */
#define PGEMBED_MACPICT			8		/* Macintosh picture (MAC only) */
#define PGEMBED_METAFILE		9       /* Metafile inserted as a char */
#define PGEMBED_ALTCHAR			11		/* Single character that substitutes */
#define PGEMBED_ALTSTRING		13		/* String that substitutes */
#define PGEMBED_USERDATA		12		/* User item that also holds any size data */
#define PGEMBED_USERBOX			14		/* Box to click and draw, user-defined */
#define PGEMBED_IMAGE_URL		33		/* Image via a URL */

#define PGEMBED_PGSUBSET		256		/* "Embed" type (handled differently) */

// Tab type verbs:

#define PGTAB_NONE				0		/* No tab (used to delete one) */
#define PGTAB_LEFT				1		/* Left tab */
#define PGTAB_CENTER			2		/* Center tab */
#define PGTAB_RIGHT				3		/* Right tab */
#define PGTAB_DECIMAL			4		/* Decimal tab */

// Line spacing verbs:

#define PGSP_AUTOSPACE			0
#define PGSP_ONEONEHALFSPACE	0x00018000
#define PGSP_DOUBLESPACE		0x00020000
#define PGSP_TRIPLESPACE		0x00030000

// Special undo verbs:

#define PGUNDO_NONE				0			/* No undo available */
#define PGUNDO_TYPING			1			/* Undo pg_char key insertion (except backspace) */
#define PGUNDO_BACKSPACE		2			/* Undo backspace key */
#define PGUNDO_DELETE			3			/* Undo clear/cut/delete */
#define PGUNDO_FWDDELETE		4			/* Undo forward delete */
#define PGUNDO_PASTE			5			/* Undo paste/insert */
#define PGUNDO_FORMAT			6			/* Undo text style or par format or font */
#define PGUNDO_INSERT			7			/* Undo some other form of insertion */
#define PGUNDO_PAGERECT			8			/* Undo page area change */
#define PGUNDO_VISRECT			9			/* Undo vis area change */
#define PGUNDO_EXCLUSION 		10			/* Undo exclusion area change */
#define PGUNDO_DOCUMENT			11			/* Undo setDocInfo change */
#define PGUNDO_OBJECT			12			/* Undo object insertion */

// Scaling values:

#define PGSCALE_NORMAL		0x00000000	/* 100% scaling (normal default) */
#define PGSCALE_HALF		0x00010002	/* 50% scaling */
#define PGSCALE_DOUBLE		0x00020001	/* 200% scaling */

// Bits set in LOWORD from PG_MOUSEINDOC message:

#define PGPT_INVISRECT	 	0x0001		/* Point is within vis region of Paige */
#define PGPT_INPAGERECT		0x0002		/* Point is within wrap shape of Paige */
#define PGPT_INEXCLUSION	0x0004		/* Point is within exclusion shape of Paige */
#define PGPT_INTEXT			0x0008		/* Point is in text area */
#define PGPT_INPAGEGAP		0x0010		/* Point is in repeating gap of page area */
#define PGPT_INLEFTDOC		0x0020		/* Point is left of document */
#define PGPT_INRIGHTDOC		0x0040		/* Point is right of document */
#define PGPT_INTOPDOC	 	0x0080		/* Point is above top of document */
#define PGPT_INBOTDOC		0x0100		/* Point is below bottom of document */
#define PGPT_INEMBED		0x0200		/* Poing is on an embedded object */

// PAIGE "wait" verbs:

#define PGWAIT_PAGINATE		0x0000		/* Line(s) being formatted */
#define PGWAIT_COPY			0x0001		/* Text being copied */
#define PGWAIT_INSERT		0x0002		/* Text being inserted */
#define PGWAIT_SAVE			0x0003		/* File being saved */
#define PGWAIT_READ			0x0004		/* File being opened (read) */

// Notification verbs for embedded objects:

#define PGOBJ_DRAW			0x0001			// Draw embed item
#define PGOBJ_MEASURE		0x0002			// (not implemented in control)
#define PGOBJ_SWAP			0x0004			// Swap out string data
#define PGOBJ_CURSOR		0x0005			// Mouse is over an embed item
#define PGOBJ_ACTIVATE		0x0006			// Item is active
#define PGOBJ_DEACTIVATE	0x0007			// Item is deactive
#define PGOBJ_MOUSEDOWN		0x0008			// Click in item
#define PGOBJ_MOUSEMOVE		0x0009			// Moused moved after mousedown
#define PGOBJ_MOUSEUP		0x000A			// Mouse up after mousedown
#define PGOBJ_DOUBLECLICK	0x000B			// Item received double click
#define PGOBJ_DESTROY		0x000C			// Embed_ref being destroyed
#define PGOBJ_WRITEDATA		0x000E          // Disk file writing data
#define PGOBJ_READDATA		0x000F			// Disk file reading data
#define PGOBJ_SUBSETFOCUS	0x0010			// Subref has the focus
#define PGOBJ_CLIPBOARDREAD	0x0011			// Called after initializing an embed from the clipboard
#define PGOBJ_CLIPBOARDSAVE	0x0012			// Called before saving the data to clipboard
#define PGOBJ_LOAD_IMAGE	0x0013			// Load image command
#define PGOBJ_PREPARE_IMAGE	0x0015			// Prepare image for future loading
#define PGOBJ_SAVE_IMAGE	0x0016			// Save image and-or return correct URL name

// Embedded object flag bits:

#define PGOBJFLAG_SHARED	0x00080000		// Embed_ref not shared in Copy, Paste
#define PGOBJFLAG_CONTROL	0x00100000		// Embed_ref acts like a control
#define PGOBJFLAG_SETFOCUS	0x00200000		// Embed_ref obtains focus (only with PGEMBED_PGSUBSET)
#define PGOBJFLAG_NOEDIT	0x00400000		// Embed_ref not editable
#define PGOBJFLAG_VCENTERED	0x00800000		// Embed_ref vertically centered to baseline
#define PGOBJFLAG_HCENTERED	0x01000000		// Embed_ref horizontally centered to baseline
#define PGOBJFLAG_RCENTERED	0x02000000		// Embed_ref vertically centered to baseline
#define PGOBJFLAG_CLICKDRAW	0x08000000		// Click-select redraws all
#define PGOBJFLAG_MATRIX	0x10000000		// Internal refs align as matrix (only PGEMBED_PGSUBSET)
#define PGOBJFLAG_PRINTING	0x40000000		// Document is printing
#define PGOBJFLAG_DRAWN		0x20000000		// (Used internally)
#define PGOBJFLAG_V_UP		0x00010000		// Vertically centered - LOWORD pixels
#define PGOBJFLAG_V_DOWN	0x00020000		// Vertically centered + LOWORD pixels
#define PGOBJFLAG_NTH_ALIGN	0x00040000		// The nth element (LOWORD) defines baseline
#define PGOBJFLAG_NTH_CENTER 0x00080000		// Same as above but composite subrefs will be centered vertically
#define PGOBJFLAG_NO_ID		0x10000000		// Do not force a unique ID

// Subset embed_ref alignment flags (** Used by DSI only**):

#define PGSUBREF_ALIGNTOP			0x00000001		// Align top with previous top
#define PGSUBREF_ALIGNTOPRIGHT		0x00000002		// Align top to next top
#define PGSUBREF_ALIGNMIDDLE		0x00000004		// Align to center of previous
#define PGSUBREF_ALIGNCELLRIGHT		0x00000008		// Align right inside a cell
#define PGSUBREF_ALIGNCELLDECIMAL	0x00000010		// Align decimal inside a cell
#define PGSUBREF_ALIGNSUBSCRIPT		0x00000020		// Align as subscript to previous
#define PGSUBREF_HASTOPALIGN		0x00010000		// Some other ref aligns to our top

// Shared pg_ref flags:

#define PGSHARED_FORMATS		0x0001	/* Style, font and par infos shared */
#define PGSHARED_GRAF_DEVICE	0x0002	/* Common graphics context */
#define PGSHARED_VIS_AREA		0x0004	/* Shared vis area */
#define PGSHARED_PAGE_AREA		0x0008	/* Shared page area */
#define PGSHARED_EXCLUDE_AREA	0x0010	/* Shared exclusion area */

#define PG_USER (WM_USER + 0x1000)
#define PG_STYLEMESSAGES (PG_USER + 0x1200)
#define PG_PARMESSAGES (PG_USER + 0x1400)
#define PG_EDITMESSAGES (PG_USER + 0x1800)
#define PG_IOMESSAGES (PG_USER + 0x1A00)
#define PG_EMBEDMESSAGES (PG_USER + 0x1C00)
#define PG_SUBMESSAGES (PG_USER + 0x1D00)
#define PG_DOCMESSAGES (PG_USER + 0x1E00)
#define PG_SHAPEMESSAGES (PG_USER + 0x2000)
#define PG_NOTIFYMESSAGES (PG_USER + 0x2200)

#ifdef MAC_PLATFORM
#define EM_SETSEL				(WM_USER + 1)
#define EM_GETRECT				(WM_USER + 2)
#define EM_SETRECT				(WM_USER + 3)
#define EM_SETRECTNP			(WM_USER + 4)
#define EM_LINESCROLL			(WM_USER + 5)
#define EM_GETMODIFY			(WM_USER + 6)
#define EM_SETMODIFY			(WM_USER + 7)
#define EM_GETLINECOUNT			(WM_USER + 8)
#define EM_LINEINDEX			(WM_USER + 9)
#define EM_LINELENGTH			(WM_USER + 10)
#define EM_SETHANDLE			(WM_USER + 11)
#define EM_GETHANDLE			(WM_USER + 12)
#define EM_REPLACESEL			(WM_USER + 13)
#define EM_GETLINE				(WM_USER + 14)
#define EM_LIMITTEXT			(WM_USER + 15)
#define EM_CANUNDO				(WM_USER + 16)
#define EM_FMTLINES				(WM_USER + 17)			
#define EM_LINEFROMCHAR			(WM_USER + 18)
#define EM_SETTABSTOPS			(WM_USER + 19)
#define EM_SETPASSWORDCHAR		(WM_USER + 20)
#define EM_EMPTYUNDOBUFFER		(WM_USER + 21)
#define EM_GETFIRSTVISIBLELINE	(WM_USER + 22)
#define EM_SETREADONLY			(WM_USER + 23)
#define EM_SETWORDBREAKPROC		(WM_USER + 24)
#define EM_GETWORDBREAKPROC		(WM_USER + 25)		
#define EM_UNDO					(WM_USER + 26)
#endif

// PAIGE-specific control messages:

#define PG_GETGLOBALS PG_USER
#define PG_SETGLOBALS (PG_USER + 1)
#define PG_GETEXSTYLES (PG_USER + 3)
#define PG_SETEXSTYLES (PG_USER + 4)
#define PG_GETNOTIFICATIONS (PG_USER + 5)
#define PG_SETNOTIFICATIONS (PG_USER + 6)
#define PG_SETINFO (PG_USER + 7)
#define PG_GETINFO (PG_USER + 8)
#define PG_GETPGREF (PG_USER + 9)
#define PG_VMENABLE (PG_USER + 10)
#define PG_MOUSEINDOC (PG_USER + 11)
#define PG_SETEXTRASTRUCT (PG_USER + 12)
#define PG_GETEXTRASTRUCT (PG_USER + 13)
#define PG_TEXTHEIGHT (PG_USER + 14)
#define PG_TEXTSIZE (PG_USER + 15)
#define PG_SCROLLPOSITION (PG_USER + 16)
#define PG_PAGINATENOW (PG_USER + 17)
#define PG_POINTPOSITION (PG_USER + 18)
#define PG_REPAINT (PG_USER + 19)
#define PG_GETCHANGES (PG_USER + 20)
#define PG_SETCHANGES (PG_USER + 21)
#define PG_SHAREREFS (PG_USER + 22)
#define PG_UPDATESCROLLBARS (PG_USER + 23)
#define PG_SETNOTIFYWINDOW (PG_USER + 24)

#define PG_SAVEFOCUS (PG_USER + 25)
#define PG_RESTOREFOCUS (PG_USER + 26)

// Text style changing & getting:

#define PG_SETFONT (PG_STYLEMESSAGES)
#define PG_SETFONTONLY (PG_STYLEMESSAGES + 1)
#define PG_SETUNIVERSALFONT (PG_STYLEMESSAGES + 22)
#define PG_GETFONT (PG_STYLEMESSAGES + 2)
#define PG_GETFONTNAME (PG_STYLEMESSAGES + 3)
#define PG_SETTEXTSTYLES (PG_STYLEMESSAGES + 4)
#define PG_GETTEXTSTYLES (PG_STYLEMESSAGES + 5)
#define PG_SETFONTSIZE (PG_STYLEMESSAGES + 6)
#define PG_GETFONTSIZE (PG_STYLEMESSAGES + 7)
#define PG_SETTEXTCOLOR (PG_STYLEMESSAGES + 8)
#define PG_GETTEXTCOLOR (PG_STYLEMESSAGES + 9)
#define PG_SETTEXTBKCOLOR (PG_STYLEMESSAGES + 10)
#define PG_GETTEXTBKCOLOR (PG_STYLEMESSAGES + 11)
#define PG_SETLEADING (PG_STYLEMESSAGES + 12)
#define PG_GETLEADING (PG_STYLEMESSAGES + 13)
#define PG_SETCHARSPACING (PG_STYLEMESSAGES + 14)
#define PG_GETCHARSPACING (PG_STYLEMESSAGES + 15)
#define PG_SETEXTRASPACE (PG_STYLEMESSAGES + 16)
#define PG_GETEXTRASPACE (PG_STYLEMESSAGES + 17)
#define PG_SETALTERNATEFONT (PG_STYLEMESSAGES + 18)
#define PG_MAKESTYLESHEET (PG_STYLEMESSAGES + 19)
#define PG_APPLYSTYLESHEET (PG_STYLEMESSAGES + 20)
#define PG_MAKEPORTABLESHEET (PG_STYLEMESSAGES + 21)
#define PG_MAKENAMEDSTYLE (PG_STYLEMESSAGES + 22)
#define PG_GETNAMEDSTYLE (PG_STYLEMESSAGES + 23)
#define PG_GETAPPLIEDNAMEDSTYLE (PG_STYLEMESSAGES + 24)
#define PG_NUMNAMEDSTYLES (PG_STYLEMESSAGES + 25)
#define PG_APPLYNAMEDSTYLE (PG_STYLEMESSAGES + 26)
#define PG_FINDNAMEDSTYLE (PG_STYLEMESSAGES + 27)
#define PG_RENAMESTYLE (PG_STYLEMESSAGES + 28)
#define PG_DELETENAMEDSTYLE (PG_STYLEMESSAGES + 29)

// Paragraph format changing & getting

#define PG_SETINDENT (PG_PARMESSAGES)
#define PG_GETINDENT (PG_PARMESSAGES + 1)
#define PG_SETJUSTIFICATION (PG_PARMESSAGES + 2)
#define PG_GETJUSTIFICATION (PG_PARMESSAGES + 3)
#define PG_SETLINESPACING (PG_PARMESSAGES + 4)
#define PG_GETLINESPACING (PG_PARMESSAGES + 5)
#define PG_GETPARTOPEXTRA (PG_PARMESSAGES + 6)
#define PG_SETPARTOPEXTRA (PG_PARMESSAGES + 7)
#define PG_GETPARBOTEXTRA (PG_PARMESSAGES + 8)
#define PG_SETPARBOTEXTRA (PG_PARMESSAGES + 9)
#define PG_SETTAB (PG_PARMESSAGES + 10)
#define PG_NUMTABS (PG_PARMESSAGES + 11)
#define PG_GETTABLIST (PG_PARMESSAGES + 12)
#define PG_GETTABLISTMASK (PG_PARMESSAGES + 13)
#define PG_SETPARSTOGETHER (PG_PARMESSAGES + 14)
#define PG_GETPARSTOGETHER (PG_PARMESSAGES + 15)

// "Edit menu" messages:

#define PG_GETSELECTION (PG_EDITMESSAGES)
#define PG_SETSELECTION (PG_EDITMESSAGES + 1)
#define PG_INSERTTEXT (PG_EDITMESSAGES + 2)
#define PG_GETTEXT (PG_EDITMESSAGES + 3)
#define PG_COPYTEXT (PG_EDITMESSAGES + 4)
#define PG_PREPAREUNDO (PG_EDITMESSAGES + 5)
#define PG_UNDO (PG_EDITMESSAGES + 6)
#define PG_UNDOTYPE (PG_EDITMESSAGES + 7)
#define PG_SELECTALL (PG_EDITMESSAGES + 8)
#define PG_CANCUT (PG_EDITMESSAGES + 9)
#define PG_CANPASTE (PG_EDITMESSAGES + 10)
#define PG_ENABLEUNDO (PG_EDITMESSAGES + 11)
#define PG_SETCARET (PG_EDITMESSAGES + 12)
#define PG_GETHILITERGN (PG_EDITMESSAGES + 13)
#define PG_NUMLINES (PG_EDITMESSAGES + 14)
#define PG_OFFSETTOLINENUM (PG_EDITMESSAGES + 15)
#define PG_LINENUMTOOFFSET (PG_EDITMESSAGES + 16)
#define PG_NUMPARS (PG_EDITMESSAGES + 17)
#define PG_OFFSETTOPARNUM (PG_EDITMESSAGES + 18)
#define PG_PARNUMTOOFFSET (PG_EDITMESSAGES + 19)
#define PG_CHARACTERRECT (PG_EDITMESSAGES + 20)
#define PG_OFFSETTOWORD (PG_EDITMESSAGES + 21)
#define PG_OFFSETTOPAR (PG_EDITMESSAGES + 22)
#define PG_STYLECOPY (PG_EDITMESSAGES + 23)
#define PG_STYLEPASTE (PG_EDITMESSAGES + 24)
#define PG_STYLEPASTETEXTONLY (PG_EDITMESSAGES + 25)
#define PG_STYLEPASTEDEFAULT (PG_EDITMESSAGES + 26)
#define PG_STYLEDISPOSE (PG_EDITMESSAGES + 27)
#define PG_INCREMENTSELECT (PG_EDITMESSAGES + 28)
#define PG_DECREMENTSELECT (PG_EDITMESSAGES + 29)

// File I/O and printing:

#define PG_SAVEDOC (PG_IOMESSAGES)
#define PG_CACHESAVEDOC (PG_IOMESSAGES + 1)
#define PG_SAVETEXT (PG_IOMESSAGES + 2)
#define PG_SAVEUNICODETEXT (PG_IOMESSAGES + 3)
#define PG_READDOC (PG_IOMESSAGES + 4)
#define PG_CACHEREADDOC (PG_IOMESSAGES + 5)
#define PG_READTEXT (PG_IOMESSAGES + 6)
#define PG_SETREADHANDLER (PG_IOMESSAGES + 7)
#define PG_WRITECUSTOMDATA (PG_IOMESSAGES + 8)
#define PG_VERIFYFILE (PG_IOMESSAGES + 9)
#define PG_PRINTPAGE (PG_IOMESSAGES + 10)
#define PG_PRINTDEFAULTS (PG_IOMESSAGES + 11)
#define PG_MATCHPRINTER (PG_IOMESSAGES + 12)
#define PG_SETPRINTERDC (PG_IOMESSAGES + 13)
#define PG_GETPRINTERDC (PG_IOMESSAGES + 14)
#define PG_SAVETOMEMORY (PG_IOMESSAGES + 15)
#define PG_READFROMMEMORY (PG_IOMESSAGES + 16)
#define PG_VERIFYMEMORYFILE (PG_IOMESSAGES + 17)
#define PG_REALIZEIMPORT (PG_IOMESSAGES + 18)

// Embedding:

#define PG_INSERTOBJECT (PG_EMBEDMESSAGES)
#define PG_APPLYOBJECT (PG_EMBEDMESSAGES + 1)
#define PG_NUMOBJECTS (PG_EMBEDMESSAGES + 2)
#define PG_GETOBJECTINFO (PG_EMBEDMESSAGES + 3)
#define PG_SETOBJECTINFO (PG_EMBEDMESSAGES + 4)
#define PG_GETOBJECTBOUNDS (PG_EMBEDMESSAGES + 5)
#define PG_SETOBJECTBOUNDS (PG_EMBEDMESSAGES + 6)
#define PG_PTINOBJECT (PG_EMBEDMESSAGES + 7)
#define PG_FINDOBJECT (PG_EMBEDMESSAGES + 8)
#define PG_REMOVEOBJECT (PG_EMBEDMESSAGES + 9)
#define PG_GETSELOBJECTINFO (PG_EMBEDMESSAGES + 10)
#define PG_SETSELOBJECTINFO (PG_EMBEDMESSAGES + 11)

// Embedding, valid within callbacks only:

#define PG_INSERTSUBREF (PG_SUBMESSAGES)
#define PG_GETSUBSETLINE (PG_SUBMESSAGES + 1)
#define PG_SETALTERNATE (PG_SUBMESSAGES + 2)
#define PG_GETALTERNATE (PG_SUBMESSAGES + 3)
#define PG_INITOBJECTWALK (PG_SUBMESSAGES + 4)
#define PG_GETNEXTOBJECT (PG_SUBMESSAGES + 5)
#define PG_CLOSEOBJECTWALK (PG_SUBMESSAGES + 6)
#define PG_SETFOCUS (PG_SUBMESSAGES + 7)
#define PG_UNSETFOCUS (PG_SUBMESSAGES + 8)
#define PG_SETNESTEDFOCUS (PG_SUBMESSAGES + 9)
#define PG_UNSETNESTEDFOCUS (PG_SUBMESSAGES + 10)
#define PG_GETSUBREFARRAY (PG_SUBMESSAGES + 11)
#define PG_GETSUBREFINFO (PG_SUBMESSAGES + 12)
#define PG_GETSELSUBREFINFO (PG_SUBMESSAGES + 13)
#define PG_SETSELSUBREFINFO (PG_SUBMESSAGES + 14)

// General Document messages:

#define PG_SETBKCOLOR (PG_DOCMESSAGES)
#define PG_GETBKCOLOR (PG_DOCMESSAGES + 1)
#define PG_SETFRAMEPEN (PG_DOCMESSAGES + 2)
#define PG_GETFRAMEPEN (PG_DOCMESSAGES + 3)
#define PG_SETPAGEPEN (PG_DOCMESSAGES + 4)
#define PG_GETPAGEPEN (PG_DOCMESSAGES + 5)
#define PG_SETBREAKPEN (PG_DOCMESSAGES + 6)
#define PG_GETBREAKPEN (PG_DOCMESSAGES + 7)
#define PG_SETCOLUMNS (PG_DOCMESSAGES + 8)
#define PG_GETCOLUMNS (PG_DOCMESSAGES + 9)
#define PG_SETSCALING (PG_DOCMESSAGES + 10)
#define PG_GETSCALING (PG_DOCMESSAGES + 11)
#define PG_SETMARGINS (PG_DOCMESSAGES + 12)
#define PG_GETMARGINS (PG_DOCMESSAGES + 13)
#define PG_ENABLEHIDDENTEXT (PG_DOCMESSAGES + 14)
#define PG_HIDDENTEXTSTATE (PG_DOCMESSAGES + 15)
#define PG_ENABLEINVISIBLES (PG_DOCMESSAGES + 16)
#define PG_INVISIBLESSTATE (PG_DOCMESSAGES + 17)
#define PG_INSERTBREAK (PG_DOCMESSAGES + 18)
#define PG_OFFSETTOPAGE (PG_DOCMESSAGES + 19)
#define PG_POINTTOPAGE (PG_DOCMESSAGES + 20)
#define PG_GETPAGEINFO (PG_DOCMESSAGES + 21)
#define PG_USEPALETTE (PG_DOCMESSAGES + 22)
#define PG_GETPALETTE (PG_DOCMESSAGES + 23)
#define PG_SETWIDOWCONTROL (PG_DOCMESSAGES + 24)
#define PG_GETWIDOWCONTROL (PG_DOCMESSAGES + 25)


// Shapes, Containers and Exclusions:

#define PG_SETPAGERECT (PG_SHAPEMESSAGES)
#define PG_GETPAGERECT (PG_SHAPEMESSAGES + 1)
#define PG_SETVISRECT (PG_SHAPEMESSAGES + 2)
#define PG_GETVISRECT (PG_SHAPEMESSAGES + 3)
#define PG_SETCONTAINERMODE (PG_SHAPEMESSAGES + 4)
#define PG_APPENDCONTAINER (PG_SHAPEMESSAGES + 5)
#define PG_NUMCONTAINERS (PG_SHAPEMESSAGES + 6)
#define PG_GETINDCONTAINER (PG_SHAPEMESSAGES + 7)
#define PG_DELETECONTAINER (PG_SHAPEMESSAGES + 8)
#define PG_MODIFYCONTAINER (PG_SHAPEMESSAGES + 9)
#define PG_POINTINCONTAINER (PG_SHAPEMESSAGES + 10)
#define PG_CLEARCONTAINERS (PG_SHAPEMESSAGES + 11)
#define PG_APPENDEXCLUSION (PG_SHAPEMESSAGES + 12)
#define PG_NUMEXCLUSIONS (PG_SHAPEMESSAGES + 13)
#define PG_GETINDEXCLUSION (PG_SHAPEMESSAGES + 14)
#define PG_DELETEEXCLUSION (PG_SHAPEMESSAGES + 15)
#define PG_MODIFYEXCLUSION (PG_SHAPEMESSAGES + 16)
#define PG_CLEAREXCLUSIONS (PG_SHAPEMESSAGES + 17)
#define PG_POINTINEXCLUSION (PG_SHAPEMESSAGES + 18)
#define PG_SETEXCLUSIONDATA (PG_SHAPEMESSAGES + 19)
#define PG_GETEXCLUSIONDATA (PG_SHAPEMESSAGES + 20)
#define PG_SWAPEXCLUSIONS (PG_SHAPEMESSAGES + 21)
#define PG_SETPAGEINDENT (PG_SHAPEMESSAGES + 22)

// Direct notification messages (sent to parent window):

#define PGN_WAIT			(PG_NOTIFYMESSAGES)			// Called for "wait" processing
#define PGN_SETDC			(PG_NOTIFYMESSAGES + 1)		// Called to modify device context
#define PGN_RELEASEDC		(PG_NOTIFYMESSAGES + 2)		// Called to release DC
#define PGN_LINEDRAW		(PG_NOTIFYMESSAGES + 3)		// Called for line drawing ornaments
#define PGN_PAGEDRAW		(PG_NOTIFYMESSAGES + 4)		// Called after drawing page(s)
#define PGN_EXDRAW			(PG_NOTIFYMESSAGES + 5)		// Called for drawing exclusions
#define PGN_BACKDRAW		(PG_NOTIFYMESSAGES + 6) 	// Called to draw background
#define PGN_SCROLL			(PG_NOTIFYMESSAGES + 8)		// Called for auto-scrolling
#define PGN_CLICK			(PG_NOTIFYMESSAGES + 10)	// Called when mouse clicked in text or container item
#define PGN_DBLCLICK		(PG_NOTIFYMESSAGES + 11)	// Called when mouse double clicked in text item
#define PGN_CLICKMOUSEUP	(PG_NOTIFYMESSAGES + 12)	// Notify for clicks on mouse UP only
#define PGN_MOUSEMOVE		(PG_NOTIFYMESSAGES + 13)	// Called any time mouse moved
#define PGN_CURSOR			(PG_NOTIFYMESSAGES + 14)	// Called when PAIGE things cursor should change
#define PGN_PAGECLICK		(PG_NOTIFYMESSAGES + 15)	// Called when mouse-down anywhere inside page area
#define PGN_EXCLICK			(PG_NOTIFYMESSAGES + 16)	// Called when mouse-down anywhere inside exclusion area
#define PGN_UNDOPREP		(PG_NOTIFYMESSAGES + 17)	// Called after PrepareUndo
#define PGN_EMBEDPROC		(PG_NOTIFYMESSAGES + 18)	// Becomes embed_ref callback function
#define PGN_READFILE		(PG_NOTIFYMESSAGES + 19)	// Read handler callback
#define PGN_WRITEFILE		(PG_NOTIFYMESSAGES + 20)	// Write handler callback
#define PGN_ENDREADWRITE	(PG_NOTIFYMESSAGES + 21)	// Called right after read or write processed
#define PGN_XPAGEMODIFY		(PG_NOTIFYMESSAGES + 22)	// Called to give app a chance to modify page
#define PGN_YPAGEMODIFY		(PG_NOTIFYMESSAGES + 23)	// Called to give app a chance to modify page
#define PGN_CHAR			(PG_NOTIFYMESSAGES + 24)	// Called before processing WM_CHAR
#define PGN_CHANGE			(PG_NOTIFYMESSAGES + 25)	// Change has occurred
#define PGN_OVERFLOW		(PG_NOTIFYMESSAGES + 26)	// Text has overflowed
#define PGN_SUBSETDRAW		(PG_NOTIFYMESSAGES + 27)	// ** reserved for DSI **
#define PGN_FAILURE			(PG_NOTIFYMESSAGES + 28)	// Out-of-memory or other notification
#define PGN_GETCLIPBOARD	(PG_NOTIFYMESSAGES + 29)	// Read the clipboard
#define PGN_WRITECLIPBOARD	(PG_NOTIFYMESSAGES + 30)	// Write the clipboard

// Message notification bits (when set your window gets PAIGE notifications):

#define PGN_WAITFLAG		0x00000001L	// Wait timer notification bit
#define PGN_SETDCFLAG		0x00000002L	// Called to modify device context
#define PGN_LINEDRAWFLAG	0x00000004L	// Called for line drawing ornaments
#define PGN_PAGEDRAWFLAG	0x00000008L	// Called after drawing page(s)
#define PGN_EXDRAWFLAG		0x00000010L	// Called for drawing exclusions
#define PGN_BACKDRAWFLAG	0x00000020L // Called before Blt'ing bitmap 
#define PGN_CHARFLAG		0x00000040L // Called before processing WM_CHAR
#define PGN_SCROLLFLAG		0x00000100L	// Called for auto-scrolling
#define PGN_CLICKFLAG		0x00000200L	// Called when mouse clicked in text or container item
#define PGN_DBLCLICKFLAG	0x00000400L	// Called when mouse double clicked in text item
#define PGN_CLICKMOUSEUPFLAG 0x00000800L	// Notify for clicks on mouse UP only
#define PGN_MOUSEMOVEFLAG	0x00001000L	// Called any time mouse moved
#define PGN_CURSORFLAG		0x00002000L	// Called when PAIGE thinks cursor should change
#define PGN_PAGECLICKFLAG	0x00004000L	// Called when mouse-down anywhere inside page area
#define PGN_EXCLICKFLAG		0x00008000L	// Called when mouse-down anywhere inside exclusion area
#define PGN_EMBEDPROCFLAG	0x00020000L	// Becomes embed_ref callback function
#define PGN_PAGEMODIFYFLAG	0x00040000L	// Called when app might want to modify page margins
#define PGN_CHANGEFLAG		0x00080000L	// Notify changes
#define PGN_OVERFLOWFLAG	0x00100000L // Notification that text has overflowed bottom
#define PGN_GETCLIPBOARDFLAG	0x00200000L	// Read the clipboard
#define PGN_WRITECLIPBOARDFLAG	0x00400000L	// Write the clipboard

// Flags for line drawing notification:

#define PGLINE_ENDPAR		0x4000	/* Paragraph ends after this line */
#define PGLINE_BEGINPAR		0x0200	/* New paragraph starts here */
#define PGLINE_SOFTHYPHEN	0x0010	/* Line breaks on soft hyphen */
#define PGLINE_PAGEBREAK	0x0004	/* Line ends on page break */
#define PGLINE_ENDDOC		0x2000	/* After current line, no more text in doc */
#define PGLINE_NOPREVIOUS	0x1000	/* There was no previous line drawn */

// Extended "window style" flags for PAIGE objects:

#define PGS_USELF			0x00000001L	// Include LF for CR-LF text
#define PGS_SMARTQUOTES		0x00000002L	// Do "smart quotes"
#define PGS_NOLEADING		0x00000004L // Do not add TEXTMETRIC external leading (default)
#define PGS_SMARTCUT		0x00000008L	// Do "smart cut" for cut/paste
#define PGS_VISPAGE			0x00000010L	// Page and "vis" always the same
#define PGS_MULTIPAGE		0x00000020L	// Repeating pages (WYSIWYG page breaks)
#define PGS_FIXEDPAGE		0x00000040L // Page is "fixed" height, does not grow
#define PGS_NOCRLFPASTE		0x00000080L	// Ignore single line CR-LF from clipboard
#define PGS_NOCRLFCOPY		0x00000100L // Do not send CR-LF for lines to clipboard
#define PGS_NOSETCURSOR		0x00000200L	// Do not set mouse cursor automatically
#define PGS_NOAUTOUNDO		0x00000400L // Do not automatically prepare Undo
#define PGS_HIDEMARGINS		0x00000800L	// Do not show margins WYSIWYG
#define PGS_LINEDITMODE		0x00001000L	// Line editor only mode
#define PGS_CARETVIEW		0x00002000L // The caret stays within vis area for scrolling
#define PGS_NONSIZEABLE		0x00004000L // View area is NOT re-sizable from WM messages
#define PGS_SCALEVISRGN		0x00008000L	// Scale vis area when scaling
#define PGS_COLORVISRGN		0x00010000L	// Fill whole vis area with bkcolor

// Extended properties info record:

typedef struct {
	long		ex_styles;		// Extended "window styles"
	long		notification;	// Items that require notification
	short		extra_scroll_h; // Amount for scroll-to-cursor slop (horizontal)
	short		extra_scroll_v; // Amount for scroll-to-cursor slop (vertical)
	RECT		page_rect;		// Text rectangle (if different than clipped area)
	RECT		vis_rect;		// Visual area
	RECT		print_margins;	// Printer margins (four sides)
	HPEN		page_pen;		// Pen to draw frame around page (NULL if none)
	HPEN		frame_pen;		// Pen to draw frame around vis area (NULL if none)		
	HPEN		page_break_pen;	// Pen to draw page break(s)
	short		columns;		// Number of columns
	short		column_gap;		// Gap between columns
	short		page_gap;		// Distance between pages, in pixes, if multippage
	short		page_slop;		// Minimum empty space at bottom before page repeats
	short		paint_mode;		// Default BeginPaint drawing mode
	short		update_mode;	// Default text-update drawing mode
	short		scroll_mode;	// Default drawing update mode for scrolling
	short		print_mode;		// Default drawing mode when printing
} PAIGEINFOSTRUCT;

// Tab setting structure:

typedef struct {
	long		type;			// Tab type
	long		leader;			// Leader (null if no leader)
	long		position;		// Relative position
	long		refcon;			// Can be used for whatever
} PAIGETABSTRUCT;


// Print record structure:

typedef struct {
	HDC			dc;				// Printer device context
	long		machineRef;		// Not used in Windows, THPrint for Mac
	RECT		page;			// Target page rectangle
	RECT		margins;		// Target gets inset by this much
	long		resolution;		// Printer resolution (Hi = x, Lo = y)
	short		from;			// Page range (from or current)
	short		to;				// Page range (to), 0 for end of doc
	long		position;		// Text position (USED INTERNALLY)
} PAIGEPRINTSTRUCT;

// Structure to handle custom read and write for file transfer:

typedef struct {
	long			position;	// Starting position (PG_WRITECUSTOMDATA)
	long			refCon;		// Value can be anything (saved along with the data)
	void PG_FAR		*data;		// Data pointer (used instead of shortData if non-NULL)
	long			dataSize;	// Size of data
} PAIGEIOSTRUCT;


typedef struct {
	long			beginPos;		// Beginning position of text
	long			maxSize;		// Maximum byte count to copy
	pg_char PG_FAR	*buffer;		// The data buffer
} PAIGEBUFFERSTRUCT;


// Structure to insert an arbitrary object into text as a "character":

typedef struct {
	short		command;		// Verb info (in notification messages only)
	short		objType;		// Type of embed object
	short		width;			// Object width
	short		minWidth;		// Minimum width
	short		matrixExtra;	// Extra amount generated from cell matrix
	short		height;			// Object height
	short		topExtra;		// Extra space on top
	short		leftExtra;		// Extra space on left
	short		botExtra;		// Extra space on bottom
	short		rightExtra;		// Extra space on right
	long		flags;			// Special modifying attributes
	long		descent;		// Distance from bottom to display item (negative = percent of height)
	long		emptyWidth;		// Width of item when empty (applies only to certain types)
	long		data;			// Object-specific data
	pg_char		altData[44];	// Alternate data (returned from "get info" messages if < 28 bytes)
	long		dataModifier;	// Object-specific additional data
	long		refCon;			// Value attaches to object (common to all occurrences)
	long		callbackRefCon;	// Value attaches only to individual occurrence
	long		position;		// Text position to insert (-1 for current insertion)
	long		alternate;		// ** used by DSI only **
	long		param1;			// Stylesheet item. For searching, the "AND" value, otherwise used in callbacks only
	long		param2;			// Used only in notification callbacks
	long		embedPtr;		// Low-level pointer for object settings
	long		ref_var;		// Used by PAIGE low-level
	long		subref;			// Actual subref involved
	HWND		editWindow;		// The PAIGE window that owns embed_ref (valid only in callbacks)
	memory_ref	pg;				// The pg_ref for the control
	HDC			hDC;			// Used only in notification callbacks
	long		mapping_mode;	// Mapping mode of metafiles (or other graphics)
	short		xExt;			// For metafiles, the original xExt
	short		yExt;			// For metafiles, the original yExt
	long		walk_stack;		// Used by PAIGE internally for walking through refs
	long		walk_restore;	// Used by PAIGE internally for restoring after ref walk
	long		textRef;		// Set to the text in object walk
	short		nest_level;		// Nested level of subref
	short		tightenAscent;	// Tighten ascent by this much
	short		tightenDescent;	// Tighten descent by this much
	short		reserved1;		// Used by DSI -- type of EQ
	long		reserved2;		// Ditto -- fence data
	double		reserved3;		// Ditto -- RESERVED EQ Value
	short		reserved4;		// Ditto -- max alt refs built
	short		reserved5;		// Ditto -- number of sig digits
	long		reserved6;		// mine
} PAIGEOBJECTSTRUCT;


// Structure given in PGN_WAIT:

typedef struct {
	HWND		editWindow;		// The PAIGE window calling
	short		waitVerb;		// Why "wait" is being called
	long		progress;		// Progress (relative to completion)
	long		completion;		// Maximum completion
} PAIGEWAITSTRUCT;

// PAIGE long-word rectangle struct:

typedef struct {
	long			top;
	long			left;
	long			bottom;
	long			right;
} PAIGERECT;

// Structure that returns page information:

typedef struct {
	long		query;			// A page number, or text offset, etc. (depending on function)
	long		top;			// Top side (can be > 32K + or -)
	long		left;			// Left side (can be > 32K + or -)
	long		bottom;			// Bottom side (can be > 32K + or -)
	long		right;			// Right side (can be > 32K + or -)
	long		baseline;		// Baseline (when dealing with embedded objects) */
	long		screen_y;		// Amount added to vertical position when displayed
	long		screen_x;		// Amount added to horizontal position when displayed
	HDC			hDC;			// Valid only when given in notification(s)
} PAIGERECTSTRUCT;

// Structure passed for line drawing notification:

typedef struct {
	HDC				hDC;			// Device context to where we are drawing
	RECT			currentBox;		// Current line bounds
	RECT			previousBox;	// Previous line bounds
	long			lineNum;		// Line number
	long			parNum;			// Paragraph number
	long			currentPos;		// Line's beginning position in text
	long			endingPos;		// Line's ending position in text
	unsigned short	flags;			// Info about this line
} PAIGELINESTRUCT;

// Struct passed in auto scroll message:

typedef struct {
	HDC				hDC;
	long			point_y;
	long			point_x;
	short			verb_y;
	short			verb_x;
} PAIGESCROLLSTRUCT;


// Struct that gets passed to page or exclusion click notification:

typedef struct {
	long			yMouse;		// The y-axis of point
	long			xMouse;		// The x-axis of point
	short			wParam;		// The wParam part of button-down message
	short			area;		// Box number (container or exclusion item) or clicked area
	short			dblClick;	// TRUE if double-click
	short			trplClick;	// TRUE if triple-click
	long			firstPos;	// First position in text (if applicable)
	long			endPos;		// Ending position in text (if applicable)
} PAIGEMOUSESTRUCT;


// Paragraph struct for stylesheets:

typedef struct {
	long			left_indent;
	long			right_indent;
	long			first_indent;
	short			justification;
	short			num_tabs;
	PAIGETABSTRUCT	tabs[32];
	short			space_after;
	short			space_before;
	short			spacing;
	unsigned short	flags;
} PAIGEPARSTRUCT;

// Struct for named stylesheets:

typedef struct {
	pg_char			name[64];
	PG_LOGFONT		style;
	PAIGEPARSTRUCT	par_style;
} PAIGENAMEDSTYLE;


#ifdef __cplusplus
extern	"C" {
#endif

#ifdef MAC_PLATFORM

extern PG_PASCAL (void) InitPaigeLibs (Str255 defFont, RGBColor *background);
extern PG_PASCAL (void) ClosePaigeLibs (void);
extern long pascal NewPaigeControl (WindowPtr inWindow, Str255 wTitle, Rect *bounds, Rect *page,
		long winflags, long ext_winflags, pg_control_callback callback, long refCon);
extern void pascal DisposePaigeControl (long pg);
extern long pascal SendPaigeMessage (long pg, short message, WORD wParam, long lParam);
extern long pascal GetPaigeRefCon (long pg);
extern long pascal IsPaigeEvent (EventRecord *event);
extern void pascal PaigeControlIdle (void);
extern void pascal SetPaigeFocus (long pg);
extern long pascal GetPaigeFocus (void);
extern void ActivatePGScrollbars (HWND hWnd, long message);
extern PG_PASCAL (memory_ref) CreateMemoryRef (long bytesize, pg_boolean zero_fill);

#else

extern PG_PASCAL (void) InitPaigeLibs (HANDLE hInstance, PGSTR defFont, HBRUSH background);
extern PG_PASCAL (void) ClosePaigeLibs (void);
extern PG_PASCAL (memory_ref) CreateMemoryRef (long bytesize, pg_boolean zero_fill);

#endif

#ifdef PAIGE_H
extern PG_PASCAL (void) GetPaigeGlobals (pg_globals_ptr globals);
extern PG_PASCAL (void) SetPaigeGlobals (pg_globals_ptr globals);
extern PG_PASCAL (pg_globals_ptr) GetPaigeGlobalsPtr (void);
#endif

#ifdef DOING_EQ
extern PG_PASCAL (void) InitDirectEQRef (memory_ref pg);
extern PG_PASCAL (void) UnInitDirectEQRef (memory_ref pg);
extern PG_PASCAL (long) SendDirectMessage (memory_ref pg, short message, WORD wParam, long lParam);
#endif

#ifdef __cplusplus
	}
#endif

#endif

