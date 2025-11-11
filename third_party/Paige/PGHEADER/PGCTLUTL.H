/* Header file for functions locally contained in the Custom Control library.
Unlike the application that only uses the blackbox control, these definitions
know all about the lower-level PAIGE functions and structs. */

#ifndef PGCTLUTL_H
#define PGCTLUTL_H

#include "Paige.h"
#include "pgEmbed.h"
#include "pgCntl.h"
#include "pgScrap.h"

// Default settings for doc_info:

#define DEF_REPEAT_SLOP		32
#define DEF_SCROLL_APPEND_V	24
#define DEF_SCROLL_APPEND_H 16
#define DEF_REPEAT_OFFSET	16
#define DEF_BREAK_WIDTH		4

#define MAX_READ_SIZE		4096			// Max buffer for reading raw text

// General-purpose maintenance flags:

#define SCROLLBARS_DIRTY_FLAG		0x0001	// Scrollbar values need updating
#define AUTOSCROLL_ENABLE			0x0002  // Forced mousemove message if on
#define HAS_CONTAINERS				0x0004	// Document has containers
#define DOUBLECLICK_FLAG			0x0008	// Last mouse-down was double click
#define PARENT_IS_MDI				0x0010	// Parent is MDI window

#define SAVE_FILE_AS_TEXT			1
#define SAVE_FILE_AS_UNICODE		2

#ifdef MAC_PLATFORM
#define MovePenTo(unused, h, v) MoveTo(h, v)
#define PenLineTo(unused, h, v) LineTo(h, v)
#define SendMessage(hWnd, message, wParam, lParam) ControlWinProc(hWnd, message, wParam, lParam)
#endif
#ifdef WINDOWS_PLATFORM
#define MovePenTo(hdc, h, v) MoveToEx(hdc, h, v, NULL)
#define PenLineTo(hdc, h, v) LineTo(hdc, h, v)
#endif

// Structure that we keep around for each control:

typedef struct {
	pg_ref			pg;				// The pg_ref itself
	undo_ref		undo;			// The undo buffer
	HWND			parent;			// Original parent window
	HWND			myself;			// My own window handle
	WORD			control_id;		// ID in hMenu for control
	short			modifiers;		// Mouse/key-down and drag modifiers
	short			wParam;			// Original wParam value in button-down
	short			mouse_loc;		// Mouse-action state (since last call)
	short			paint_mode;		// Drawing mode for WM_PAINT response
	short			update_mode;	// Drawing mode for text-changing functions
	short			scroll_mode;	// Drawing mode for scrolling
	short			insert_mode;	// Key insertion mode3
	short			print_mode;		// Drawing mode for printing
	short			flags;			// general flags for maintenance (internal)
	short			left_margin;	// Left printer margin
	short			right_margin;	// Right printer margin
	short			top_margin;		// Top printer margin
	short			bottom_margin;	// Bottom printer margin
	short			columns;		// Number of columns
	short			column_gap;		// Distance betweet columns
	short			frame_pen_width; // Width of pen for frame draw
	short			page_pen_width;	// Width of pen for page draw
	short			break_pen_width; // Width of pen for page breaks
	short			page_slop;		// Page repeat slop value
	rectangle		page_rect;		// Original page rect given from app
	rectangle		vis_rect;		// Current vis rect
	long			win_flags;		// Original window-style flags
	long			notification;	// Notification-enable flags
	long			ex_styles;		// Extended window flags (set by app)
	long			changes;		// Changes counter (for changes outside of pg_ref)
	HPEN			frame_pen;		// Pen color for drawing frame(s)
	HPEN			page_pen;		// Pen color for drawing page(s)
	HPEN			page_break_pen;	// Pen color for drawing page beaks
	long			max_input;		// If non-zero, maximum chars insert allowed
	short			extra_scroll_h; // Amount for scroll-to-cursor slop (horizontal)
	short			extra_scroll_v; // Amount for scroll-to-cursor slop (vertical)
	memory_ref		user_data;		// User extra struct (or MEM_NULL)
	memory_ref		embed_click;	// Non-zero if tracking an embed_ref
} paige_control, PG_FAR *paige_control_ptr;


extern HINSTANCE	libInstance;
extern pg_globals	paige_globals;
extern pgm_globals	mem_globals;
extern HCURSOR		arrow_cursor;
extern HCURSOR		ibeam_cursor;
extern pg_os_type	clipboard_format;
extern int 			dll_inited;


extern long REGISTER_PREFIX pascal ControlWinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal UserMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal StyleMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal ParMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal EditMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal IOMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal EmbedMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal DocMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern long pascal ShapeMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

extern long pascal SubrefMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern int pascal HandleSubrefArrows (paige_control_ptr pg_stuff, pg_char the_char);

extern paige_control_ptr pascal GetPGStuff (HWND hWnd, memory_ref PG_FAR *ref);
extern void pascal RepaintControl (HWND hWnd, paige_control_ptr pg_stuff, int draw_everything);
extern void pascal CToPString (PGSTR src_str, pg_char_ptr target_str);
extern void pascal PrepareStyleUndo(paige_control_ptr pg_stuff);
extern void pascal UpdateScrollbars (HWND hWnd, paige_control_ptr pg_stuff, pg_boolean scroll_to_cursor);
extern long pascal SendNotification (paige_control_ptr pg_stuff, short message,
		long lParam1, void PG_FAR *lParam2);
extern void pascal NotifyOverflow (paige_control_ptr pg_stuff);
extern void pascal SplitIntoLongs (long input, long PG_FAR *hiword, long PG_FAR *loword);
extern int pascal SetPageArea (paige_control_ptr pg_stuff);
extern void pascal SetContainerColumns (paige_control_ptr pg_stuff);
extern void pascal InvalidateShape (paige_rec_ptr pg_rec, int exclude_changed);
extern void pascal SetupDragInfo (paige_control_ptr pg_stuff, co_ordinate_ptr pg_mouse,
		PAIGEMOUSESTRUCT PG_FAR *mouse_info);
extern int pascal SetExtendedStyleBits (paige_control_ptr pg_stuff, long new_flags);
extern PG_PASCAL (long) EmbedCallbackProc (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
		long embed_type, short command, long user_refcon,
		long param1, long param2);
extern void pascal FillEmbedInfo (embed_ref ref, PAIGEOBJECTSTRUCT PG_FAR *info,
		long offset, style_info_ptr style);
extern PG_PASCAL (void) SubsetPostDraw (paige_rec_ptr pg, draw_points_ptr draw_position,
		rectangle_ptr draw_bounds, co_ordinate_ptr offset_extra, style_walk_ptr walker);

long AddPGStylesheet (pg_ref pg, long message, pg_boolean change_existing, LOGFONT PG_FAR *log_font);

#ifdef WINDOWS_PLATFORM
extern long pascal SendChangeCommand (paige_control_ptr pg_stuff);
#else

#define SendChangeCommand(pgstuff) SendNotification(pgstuff, PGN_CHANGE, 0, NULL)

void BeginPaint (HWND hWnd, PAINTSTRUCT *ps);
void EndPaint (HWND hWnd, PAINTSTRUCT *ps);
void InvalidateRect (HWND hWnd, RECT *r, short erase);
void SetScrollRange (HWND hWnd, short what_ctl, short min, short max, short redraw);
void SetScrollPos (HWND hWnd, short what_ctl, short value, short redraw);
long pascal DefWindowProc (HWND hWnd, short message, WORD wParam, LPARAM lParam);
long SelectObject (HDC hDC, long object);
void SetPGCNTLCursor (short id);
void SizePGWindow (HWND hWnd, long message, long PG_FAR *hvalue, long PG_FAR *vvalue);

#endif

#endif
