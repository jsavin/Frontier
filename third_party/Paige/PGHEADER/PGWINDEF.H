/************ Pseudo "Windows" definitions for Macintsoh. ************/
#ifndef PGWINDEF_H
#define PGWINDEF_H

#ifdef WINDOWS_PLATFORM
#define SendPaigeMessage(hwnd, msg, wparam, lparam) SendMessage(hwnd, msg, wparam, lparam)

#ifndef __QTW_H__
struct RGBColor {
	unsigned short		red;
	unsigned short		green;
	unsigned short		blue;
};
typedef struct RGBColor RGBColor;
#endif

struct PG_LOGFONT {
	pg_char			fontName[64];		// Name of font
	long			pointsize;			// Pointsize
	long			relPointsize;		// Relative pointsize
	long			styles;				// Stylebits (PAIGE styles)
	RGBColor		textColor;			// Foreground color of text
};
typedef struct PG_LOGFONT PG_LOGFONT;

#else
// Macintosh platform

#include <Types.h>
#include <Controls.h>

/* The purpose of this header file is to define Windows-specific items so we can run
the PAIGE Control on Macintsoh.

DO NOT INCLUDE THIS FOR WINDOWS PLATFORMS. */

#define CNTL_FAR
#define __loadds
#define _export

#define GetWindowLong(hwnd, unused)   ((pg_window_ptr)hwnd)->window_long
#define SetWindowLong(hwnd, unused, value) ((pg_window_ptr)hwnd)->window_long = value

#define LOWORD(val)  LoWord(val)
#define HIWORD(val)  HiWord(val)

// Window style flags

#define ES_AUTOVSCROLL		0x00020000
#define ES_AUTOHSCROLL		0x00010000
#define ES_NOHIDESEL		0x00008000
#define ES_CENTER			0x00004000
#define ES_RIGHT			0x00002000
#define ES_MULTILINE		0x00001000
#define ES_READONLY			0x00000800
#define ES_LOWERCASE		0x00000400
#define ES_UPPERCASE		0x00000200
#define EN_CHANGE			0x00000100
#define WS_HSCROLL			0x00000080
#define WS_VSCROLL			0x00000040
#define WS_VISIBLE			0x00000020
#define WS_MINIMIZEBOX		0x00000010
#define WS_GROWBOX			0x00000008			// Mac only
#define WS_NONSIZEABLE		0x00000004			// Mac only, box is non sizable with GrowWindow

// Scroll definitions

#define SB_VERT				0
#define SB_HORZ				1

#define SB_PAGEDOWN			2
#define SB_LINEDOWN			3
#define SB_PAGEUP			4
#define SB_LINEUP			5
#define SB_THUMBPOSITION	6
#define SB_THUMBTRACK		7
#define SB_TOP				8
#define SB_BOTTOM			9

// Virtual key definition

#define VK_CONTROL			0x000F
#define VK_SHIFT			0x0010
#define VK_LEFT				0x001C
#define VK_UP				0x001E
#define VK_RIGHT			0x001D
#define VK_DOWN				0x001F
#define VK_HOME				0x0001
#define VK_END				0x0004
#define VK_PRIOR			0x000B
#define VK_NEXT				0x000C
#define VK_DELETE			0x007F

// Mousedown modifiers:

#define	MK_SHIFT			shiftKey
#define	MK_CONTROL			controlKey

#ifndef WM_USER

// Window Message definitions (or roll your own if WM_USER already defined)

#define WM_USER				0x0400

#define WM_CHAR				0x0020
#define WM_CLEAR			0x0024
#define WM_CUT				0x0026
#define WM_COPY				0x0028
#define WM_COMMAND			0x002C
#define WM_CREATE			0x0030
#define WM_DESTROY			0x0034
#define WM_GETTEXT			0x0038
#define WM_GETDLGCODE		0x003A
#define WM_GETFONT			0x003C
#define WM_HSCROLL			0x0040
#define WM_VSCROLL			0x0044
#define WM_KEYDOWN			0x0048
#define WM_KEYUP			0x004A
#define WM_KILLFOCUS		0x004C
#define WM_LBUTTONDBLCLK	0x0050
#define WM_LBUTTONDOWN		0x0054
#define WM_LBUTTONUP		0x0058
#define WM_MOVE				0x005C
#define WM_MOUSEMOVE		0x0060
#define WM_PAINT			0x0064
#define WM_PASTE			0x0068
#define WM_SETFOCUS			0x006C
#define WM_SETFONT			0x0070
#define WM_SIZE				0x0074
#define WM_SETTEXT			0x0078
#define WM_UNDO				0x007C

#endif

typedef Rect RECT;

// Pensize is highword, low byte.

#define VOID 			void
#define CONST 			const

typedef long 			HPEN;
typedef short 			HCURSOR;
typedef long 			HDC;
typedef long			HWND;
typedef char *			HINSTANCE;
typedef char *			LPSTR;
typedef char *			LPCSTR;
typedef long			INT;
typedef unsigned long	UINT;
typedef INT             BOOL;
typedef char 			CHAR;
typedef unsigned char   BYTE;
typedef unsigned char   UCHAR;
typedef short 			SHORT;
typedef unsigned short	USHORT;
typedef unsigned long	WORD;
typedef long			LONG;
typedef unsigned long	ULONG;
typedef unsigned long	DWORD;
typedef UINT			WPARAM;
typedef LONG 			LPARAM;
typedef LONG 			LRESULT;
typedef float           FLOAT;
typedef FLOAT *			PFLOAT;
typedef BOOL *          PBOOL;
typedef BOOL *          LPBOOL;
typedef BYTE *          PBYTE;
typedef BYTE *          LPBYTE;
typedef UCHAR *			PUCHAR;
typedef INT *           PINT;
typedef unsigned long * PUINT;
typedef INT *           LPINT;
typedef WORD *          PWORD;
typedef WORD *          LPWORD;
typedef USHORT *		PUSHORT;
typedef long *          LPLONG;
typedef ULONG *			PULONG;
typedef DWORD *         PDWORD;
typedef DWORD *         LPDWORD;
typedef void *          LPVOID;
typedef CONST void *    LPCVOID;
typedef pg_char			TCHAR;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))

// The callback function

typedef PG_FN_PASCAL(long, pg_control_callback) (HWND pg, UINT message, WORD wParam, LPARAM lParam);

// The "window record" to emulate the hWnd:

struct pg_window_rec {
	GrafPtr				w_ptr;				// The window ptr
	Rect				vis_bounds;			// Visible contents (excluding scrollbars)
	Rect				full_vis_bounds;	// Visible contents (including scrollbars)
	short				window_height;		// Window height
	short				window_width;		// Window width
	pg_control_callback	callback;			// Callback function
	long				window_long;		// Long ref stored
	long				refCon;				// App's refCon
	long				style;				// Original stylebits
	ControlHandle		h_scroll;			// Horizontal scrollbar
	ControlHandle		v_scroll;			// Vertical scrollbar
	Point				last_mouse;			// Previous mouse point
	int					mouse_down;			// Flag for mouse-down
};
typedef struct pg_window_rec pg_window_rec;
typedef pg_window_rec PG_FAR *pg_window_ptr;


// Dummy paint struct to do fake update

struct PAINTSTRUCT {
	GrafPtr				port;
	GrafPtr				old_port;
	RgnHandle			visRgnSave;
};
typedef struct PAINTSTRUCT PAINTSTRUCT;


// An HPEN object (we just look at the bits):

#define HPEN_RED		0x00000F00		// Red color
#define HPEN_GREEN		0x000000F0		// Green indicator
#define HPEN_BLUE		0x0000000F		// Blue color
#define HPEN_SOLID		0x01000000		// Solid line
#define HPEN_GRAY		0x02000000		// Gray
#define HPEN_LTGRAY		0x04000000		// Light gray

#define HPEN_NORMAL		0x01010000		// Normal, black pen

// Mac definition of LOGFONT

struct LOGFONT {
	Str255			fontName;			// Name of font
	long			pointsize;			// Pointsize
	long			relPointsize;		// Relative pointsize
	long			styles;				// Stylebits (PAIGE styles)
	long			char_extra;			// Extra character spacing
	RGBColor		textColor;			// Foreground color of text
};
typedef struct LOGFONT LOGFONT;
typedef LOGFONT PG_LOGFONT;

struct CREATESTRUCT {
	GrafPtr					w_ptr;		// The port
	long					refCon;		// App's refcon
	long					style;		// Window style flags
	long					ext_winflags; // Extra PAIGE window styles
	Rect					bounds;		// Bounds for control
	Rect					page;		// Page bounds for control
};
typedef struct CREATESTRUCT CREATESTRUCT;

// Language definitions

/*
 *  Language IDs.
 *
 *  The following two combinations of primary language ID and
 *  sublanguage ID have special semantics:
 *
 *    Primary Language ID   Sublanguage ID      Result
 *    -------------------   ---------------     ------------------------
 *    LANG_NEUTRAL          SUBLANG_NEUTRAL     Language neutral
 *    LANG_NEUTRAL          SUBLANG_DEFAULT     User default language
 *    LANG_NEUTRAL          SUBLANG_SYS_DEFAULT System default language
 */

/*
 *  Primary language IDs.
 */
#define LANG_NEUTRAL                     0x00

#define LANG_ARABIC                      0x01
#define LANG_AFRIKAANS                   0x36
#define LANG_ALBANIAN                    0x1c
#define LANG_BASQUE                      0x2d
#define LANG_BULGARIAN                   0x02
#define LANG_BYELORUSSIAN                0x23
#define LANG_CATALAN                     0x03
#define LANG_CHINESE                     0x04
#define LANG_CROATIAN                    0x1a
#define LANG_CZECH                       0x05
#define LANG_DANISH                      0x06
#define LANG_DUTCH                       0x13
#define LANG_ENGLISH                     0x09
#define LANG_ESTONIAN                    0x25
#define LANG_FAEROESE                    0x38
#define LANG_FARSI                       0x29
#define LANG_FINNISH                     0x0b
#define LANG_FRENCH                      0x0c
#define LANG_GERMAN                      0x07
#define LANG_GREEK                       0x08
#define LANG_HEBREW                      0x0D
#define LANG_HUNGARIAN                   0x0e
#define LANG_ICELANDIC                   0x0f
#define LANG_INDONESIAN                  0x21
#define LANG_ITALIAN                     0x10
#define LANG_JAPANESE                    0x11
#define LANG_KOREAN                      0x12
#define LANG_LATVIAN                     0x26
#define LANG_LITHUANIAN                  0x27
#define LANG_NORWEGIAN                   0x14
#define LANG_POLISH                      0x15
#define LANG_PORTUGUESE                  0x16
#define LANG_ROMANIAN                    0x18
#define LANG_RUSSIAN                     0x19
#define LANG_SLOVAK                      0x1b
#define LANG_SLOVENIAN                   0x24
#define LANG_SORBIAN                     0x2e
#define LANG_SPANISH                     0x0a
#define LANG_SWEDISH                     0x1d
#define LANG_THAI                        0x1e
#define LANG_TURKISH                     0x1f
#define LANG_UKRAINIAN                   0x22

/*
 *  Sublanguage IDs.
 *
 *  The name immediately following SUBLANG_ dictates which primary
 *  language ID that sublanguage ID can be combined with to form a
 *  valid language ID.
 */
#define SUBLANG_NEUTRAL                  0x00    /* language neutral */
#define SUBLANG_DEFAULT                  0x01    /* user default */
#define SUBLANG_SYS_DEFAULT              0x02    /* system default */

#define SUBLANG_ARABIC                   0x01    /* Arabic (Saudi Arabia) */
#define SUBLANG_ARABIC_IRAQ              0x02    /* Arabic (Iraq) */
#define SUBLANG_ARABIC_EGYPT             0x03    /* Arabic (Egypt) */
#define SUBLANG_ARABIC_LIBYA             0x04    /* Arabic (Libya) */
#define SUBLANG_ARABIC_ALGERIA           0x05    /* Arabic (Algeria) */
#define SUBLANG_ARABIC_MOROCCO           0x06    /* Arabic (Morocco) */
#define SUBLANG_ARABIC_TUNISIA           0x07    /* Arabic (Tunisia) */
#define SUBLANG_ARABIC_OMAN              0x08    /* Arabic (Oman) */
#define SUBLANG_ARABIC_YEMEN             0x09    /* Arabic (Yemen) */
#define SUBLANG_ARABIC_SYRIA             0x10    /* Arabic (Syria) */
#define SUBLANG_ARABIC_JORDAN            0x11    /* Arabic (Jordan) */
#define SUBLANG_ARABIC_LEBANON           0x12    /* Arabic (Lebanon) */
#define SUBLANG_ARABIC_KUWAIT            0x13    /* Arabic (Kuwait) */
#define SUBLANG_ARABIC_UAE               0x14    /* Arabic (United Arab Emirates) */
#define SUBLANG_ARABIC_BAHRAIN           0x15    /* Arabic (Bahrain) */
#define SUBLANG_ARABIC_QATAR             0x16    /* Arabic (Qatar) */
#define SUBLANG_CHINESE_TRADITIONAL      0x01    /* Chinese (Taiwan) */
#define SUBLANG_CHINESE_SIMPLIFIED       0x02    /* Chinese (PR China) */
#define SUBLANG_CHINESE_HONGKONG         0x03    /* Chinese (Hong Kong) */
#define SUBLANG_CHINESE_SINGAPORE        0x04    /* Chinese (Singapore) */
#define SUBLANG_DUTCH                    0x01    /* Dutch */
#define SUBLANG_DUTCH_BELGIAN            0x02    /* Dutch (Belgian) */
#define SUBLANG_ENGLISH_US               0x01    /* English (USA) */
#define SUBLANG_ENGLISH_UK               0x02    /* English (UK) */
#define SUBLANG_ENGLISH_AUS              0x03    /* English (Australian) */
#define SUBLANG_ENGLISH_CAN              0x04    /* English (Canadian) */
#define SUBLANG_ENGLISH_NZ               0x05    /* English (New Zealand) */
#define SUBLANG_ENGLISH_EIRE             0x06    /* English (Irish) */
#define SUBLANG_ENGLISH_SAFRICA          0x07    /* English (South Africa) */
#define SUBLANG_ENGLISH_JAMAICA          0x08    /* English (Jamaica) */
#define SUBLANG_ENGLISH_CARRIBEAN        0x09    /* English (Carribean) */
#define SUBLANG_FRENCH                   0x01    /* French */
#define SUBLANG_FRENCH_BELGIAN           0x02    /* French (Belgian) */
#define SUBLANG_FRENCH_CANADIAN          0x03    /* French (Canadian) */
#define SUBLANG_FRENCH_SWISS             0x04    /* French (Swiss) */
#define SUBLANG_FRENCH_LUXEMBOURG        0x05    /* French (Luxembourg) */
#define SUBLANG_GERMAN                   0x01    /* German */
#define SUBLANG_GERMAN_SWISS             0x02    /* German (Swiss) */
#define SUBLANG_GERMAN_AUSTRIAN          0x03    /* German (Austrian) */
#define SUBLANG_GERMAN_LUXEMBOURG        0x04    /* German (Luxembourg) */
#define SUBLANG_GERMAN_LIECHTENSTEIN     0x05    /* German (Liechtenstein) */
#define SUBLANG_ITALIAN                  0x01    /* Italian */
#define SUBLANG_ITALIAN_SWISS            0x02    /* Italian (Swiss) */
#define SUBLANG_KOREAN                   0x01    /* Korean */
#define SUBLANG_KOREAN_JOHAB             0x02    /* Korean (Johab) */
#define SUBLANG_NORWEGIAN_BOKMAL         0x01    /* Norwegian (Bokmal) */
#define SUBLANG_NORWEGIAN_NYNORSK        0x02    /* Norwegian (Nynorsk) */
#define SUBLANG_PORTUGUESE               0x02    /* Portuguese */
#define SUBLANG_PORTUGUESE_BRAZILIAN     0x01    /* Portuguese (Brazilian) */
#define SUBLANG_SPANISH                  0x01    /* Spanish (Castilian) */
#define SUBLANG_SPANISH_MEXICAN          0x02    /* Spanish (Mexican) */
#define SUBLANG_SPANISH_MODERN           0x03    /* Spanish (Modern) */
#define SUBLANG_SPANISH_GUATEMALA        0x04    /* Spanish (Guatemala) */
#define SUBLANG_SPANISH_COSTARICA        0x05    /* Spanish (Costa Rica) */
#define SUBLANG_SPANISH_PANAMA           0x06    /* Spanish (Panama) */
#define SUBLANG_SPANISH_DOMINICAN        0x07    /* Spanish (Dominican Republic) */
#define SUBLANG_SPANISH_VENEZUELA        0x08    /* Spanish (Venezuela) */
#define SUBLANG_SPANISH_COLOMBIA         0x09    /* Spanish (Colombia) */
#define SUBLANG_SPANISH_PERU             0x10    /* Spanish (Peru) */
#define SUBLANG_SPANISH_ARGENTINA        0x11    /* Spanish (Argentina) */
#define SUBLANG_SPANISH_ECUADOR          0x12    /* Spanish (Ecuador) */
#define SUBLANG_SPANISH_CHILE            0x13    /* Spanish (Chile) */
#define SUBLANG_SPANISH_URUGUAY          0x14    /* Spanish (Uruguay) */
#define SUBLANG_SPANISH_PARAGUAY         0x15    /* Spanish (Paraguay) */
#define SUBLANG_SPANISH_BOLIVIA          0x16    /* Spanish (Bolivia) */

/*
 *  Sorting IDs.
 *
 */
#define SORT_DEFAULT                     0x0     /* sorting default */

#define SORT_JAPANESE_XJIS               0x0     /* Japanese XJIS order */
#define SORT_JAPANESE_UNICODE            0x1     /* Japanese Unicode order */

#define SORT_CHINESE_BIG5                0x0     /* Chinese BIG5 order */
#define SORT_CHINESE_UNICODE             0x1     /* Chinese Unicode order */

#define SORT_KOREAN_KSC                  0x0     /* Korean KSC order */
#define SORT_KOREAN_UNICODE              0x1     /* Korean Unicode order */


/*
 *  A language ID is a 16 bit value which is the combination of a
 *  primary language ID and a secondary language ID.  The bits are
 *  allocated as follows:
 *
 *       +-----------------------+-------------------------+
 *       |     Sublanguage ID    |   Primary Language ID   |
 *       +-----------------------+-------------------------+
 *        15                   10 9                       0   bit
 *
 *
 *  Language ID creation/extraction macros:
 *
 *    MAKELANGID    - construct language id from a primary language id and
 *                    a sublanguage id.
 *    PRIMARYLANGID - extract primary language id from a language id.
 *    SUBLANGID     - extract sublanguage id from a language id.
 */
#define MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))
#define PRIMARYLANGID(lgid)    ((WORD  )(lgid) & 0x3ff)
#define SUBLANGID(lgid)        ((WORD  )(lgid) >> 10)


/*
 *  A locale ID is a 32 bit value which is the combination of a
 *  language ID, a sort ID, and a reserved area.  The bits are
 *  allocated as follows:
 *
 *       +-------------+---------+-------------------------+
 *       |   Reserved  | Sort ID |      Language ID        |
 *       +-------------+---------+-------------------------+
 *        31         20 19     16 15                      0   bit
 *
 *
 *  Locale ID creation/extraction macros:
 *
 *    MAKELCID       - construct locale id from a language id and a sort id.
 *    LANGIDFROMLCID - extract language id from a locale id.
 *    SORTIDFROMLCID - extract sort id from a locale id.
 */
#define NLS_VALID_LOCALE_MASK  0x000fffff

#define MAKELCID(lgid, srtid)  ((DWORD)((((DWORD)((WORD  )(srtid))) << 16) |  \
                                         ((DWORD)((WORD  )(lgid)))))
#define LANGIDFROMLCID(lcid)   ((WORD  )(lcid))
#define SORTIDFROMLCID(lcid)   ((WORD  )((((DWORD)(lcid)) & NLS_VALID_LOCALE_MASK) >> 16))


/*
 *  Default System and User IDs for language and locale.
 */
#define LANG_SYSTEM_DEFAULT    (MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT))
#define LANG_USER_DEFAULT      (MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT))

#define LOCALE_SYSTEM_DEFAULT  (MAKELCID(LANG_SYSTEM_DEFAULT, SORT_DEFAULT))
#define LOCALE_USER_DEFAULT    (MAKELCID(LANG_USER_DEFAULT, SORT_DEFAULT))


#endif
#endif
