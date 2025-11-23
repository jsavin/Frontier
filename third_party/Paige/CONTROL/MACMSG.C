

/******************************* Macintosh Specific ***************************/

#include "CPUDefs.h"

#ifdef MAC_PLATFORM

#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "pgWinDef.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"

typedef PG_FN_PASCAL(void, control_proc) (ControlHandle control, short part_code);

static short last_cursor_id = 0;
static pg_window_ptr	focus = NULL;		// PAIGE control of focus
static unsigned long	last_ticks = 0;
static pg_window_ptr	scrolling_doc = NULL;
static ControlActionUPP		action_proc = NULL; // Function for TrackControl  //ее TRS/OITC

static pascal void scroll_action_proc (ControlHandle control, short part_code);
static void init_scroll_proc (void);

STATIC_PASCAL (long) def_callback_proc (HWND hWnd, UINT message, WORD wParam, LPARAM lParam);
static int remove_control_from_window (WindowPtr w_ptr, ControlHandle ctl);
static void put_user_zoom_rect (WindowPeek window, Rect *bounds);
static pg_window_ptr find_paige_window (WindowPtr w_ptr, long *index);
static long handle_click (EventRecord *event);
static int tracked_scrollbars (pg_window_ptr window, Point mouse);
static void invalidate_control (ControlHandle ctl, short offset_h, short offset_v,
		short inset_right, short inset_bottom);



/* NewPaigeControl creates a new PAIGE control that exists within inWindow. The bounds param
is where you want the box, the flags indicate window styles (like scrolling, etc.).
The callback param is an optional callback function for notifications, and refCon can be
anything. This returns a long that you should save, send messages to, and eventually destroy.
Note, wTitle is ignored if inWindow is non-NULL, but if NULL inWindow, a new window is
created with wTitle titlebar. */

long pascal NewPaigeControl (WindowPtr inWindow, Str255 wTitle, Rect *bounds, Rect *page,
		long winflags, long ext_winflags, pg_control_callback callback, long refCon)
{
	GrafPtr			w_ptr, old_port;
	CREATESTRUCT	create_info;
	Rect			ctl_rect, w_bounds;
	pg_window_ptr	window, *window_list;

	GetPort(&old_port);
	
	w_bounds = *bounds;

	if (!(w_ptr = (GrafPtr)inWindow)) {
		// New window to be created per bounds.
		
		w_ptr = NewCWindow(NULL, &w_bounds, wTitle, ((winflags & WS_VISIBLE) != 0),
				zoomDocProc, (WindowPtr) -1, TRUE, 0);
		
		InsetRect(&w_bounds, -bounds->left, -bounds->top);

		ctl_rect = w_bounds;
		InsetRect(&ctl_rect, 32, 32);
		put_user_zoom_rect((WindowPeek)w_ptr, &ctl_rect);
	}
	
	SetPort(w_ptr);
	
	window = (pg_window_ptr)NewPtrClear(sizeof(pg_window_rec));
	
	window->w_ptr = w_ptr;
	window->refCon = refCon;
	window->style = winflags;
	window->vis_bounds = window->full_vis_bounds = w_bounds;
	window->window_width = w_ptr->portRect.right - w_ptr->portRect.left;
	window->window_height = w_ptr->portRect.bottom - w_ptr->portRect.top;

	if (!(window->callback = callback))
		window->callback = def_callback_proc;
	
// Create the global list of "controls" if not created already:

	if (!libInstance)
		libInstance = (HINSTANCE)MemoryAlloc(&mem_globals, sizeof(pg_window_ptr), 0, 4);
	
	window_list = AppendMemory((memory_ref)libInstance, 1, FALSE);
	*window_list = window;
	UnuseMemory((memory_ref)libInstance);

	create_info.refCon = refCon;
	create_info.style = winflags;
	create_info.ext_winflags = ext_winflags;
	create_info.bounds = w_bounds;
	create_info.page = *page;
	create_info.w_ptr = w_ptr;

	if (create_info.style & WS_VSCROLL) {
		
		ctl_rect = create_info.bounds;
		ctl_rect.left = ctl_rect.right - 16;
		OffsetRect(&ctl_rect, 1, -1);
		ctl_rect.bottom += 2;
		window->vis_bounds.bottom = ctl_rect.bottom;

		if (create_info.style & WS_GROWBOX)
			ctl_rect.bottom -= 15;

		window->v_scroll = NewControl(w_ptr, &ctl_rect, "\p", TRUE, 0, 0, 0, scrollBarProc, 0);
		create_info.bounds.right = ctl_rect.left;
	}

	if (create_info.style & WS_HSCROLL) {
		
		ctl_rect = create_info.bounds;
		ctl_rect.top = ctl_rect.bottom - 16;
		OffsetRect(&ctl_rect, -1, 1);
		ctl_rect.right += 2;
		
		if (ctl_rect.right > window->vis_bounds.right)
			window->vis_bounds.right = ctl_rect.right;

		window->h_scroll = NewControl(w_ptr, &ctl_rect, "\p", TRUE, 0, 0, 0, scrollBarProc, 0);
		create_info.bounds.bottom = ctl_rect.top;
	}

	ControlWinProc((HWND)window, WM_CREATE, 0, (LPARAM)&create_info);

	SetPort(old_port);
	
	return	(long)window;
}


/* DisposePaigeControl kills the control. */

void pascal DisposePaigeControl (long pg)
{
	pg_window_ptr		window;
	long				*the_list;
	long				ctr, qty;

	window = (pg_window_ptr)pg;
	
	if (window == focus)
		focus = NULL;

	ControlWinProc((HWND)pg, WM_DESTROY, 0, 0);
	
	if (window->h_scroll)
		if (remove_control_from_window(window->w_ptr, window->h_scroll))
			DisposeControl(window->h_scroll);

	if (window->v_scroll)
		if (remove_control_from_window(window->w_ptr, window->v_scroll))
			DisposeControl(window->v_scroll);

	DisposePtr((Ptr)pg);
	
	the_list = UseMemory((memory_ref)libInstance);
	qty = GetMemorySize((memory_ref)libInstance);
	
	for (ctr = 0; ctr < qty; ++ctr) {
		
		if (the_list[ctr] == pg)
			break;
	}

	UnuseMemory((memory_ref)libInstance);
	
	if (ctr < qty)
		DeleteMemory((memory_ref)libInstance, ctr, 1);
}

/* BeginPaint actually performs a "fake" update. We have to do it this way so we don't
gobble up an entire update event. */

void BeginPaint (HWND hWnd, PAINTSTRUCT *ps)
{
	pg_window_ptr			window;
	RgnHandle				update_rgn;
	WindowPeek				update_window;
	Point					offset;

	window = (pg_window_ptr)hWnd;
	GetPort(&ps->old_port);
	ps->port = window->w_ptr;
	SetPort(window->w_ptr);

	ps->visRgnSave = window->w_ptr->visRgn;
	update_window = (WindowPeek)window->w_ptr;
	
	window->w_ptr->visRgn = NewRgn();
	update_rgn = NewRgn();
	CopyRgn(update_window->updateRgn, update_rgn);
	offset.h = offset.v = 0;
	LocalToGlobal(&offset);
	OffsetRgn(update_rgn, -offset.h, -offset.v);
	CopyRgn(ps->visRgnSave, window->w_ptr->visRgn);
	SectRgn(ps->visRgnSave, update_rgn, window->w_ptr->visRgn);
	DisposeRgn(update_rgn);
	EraseRect(&window->vis_bounds);
}


// Endpaint is Windows emulation for EndUpdate

void EndPaint (HWND hWnd, PAINTSTRUCT *ps)
{
	pg_window_ptr			window;
	Rect					valid_rect;
	paige_control_ptr		pg_stuff;
	RgnHandle				visRgn, clipRgn;

	window = (pg_window_ptr)hWnd;
	SetPort(window->w_ptr);

	if (window->h_scroll)
		Draw1Control(window->h_scroll);
	if (window->v_scroll)
		Draw1Control(window->v_scroll);

	if (window->style & WS_GROWBOX) {
		Rect			port_rect;
		
		port_rect = window->w_ptr->portRect;
		port_rect.left = port_rect.right - 15;
		port_rect.top = port_rect.bottom - 15;
		
		clipRgn = NewRgn();
		GetClip(clipRgn);
		ClipRect(&port_rect);
		DrawGrowIcon(window->w_ptr);
		SetClip(clipRgn);
		DisposeRgn(clipRgn);
	}

	visRgn = window->w_ptr->visRgn;
	window->w_ptr->visRgn = ps->visRgnSave;
	DisposeRgn(visRgn);
	
	valid_rect = window->full_vis_bounds;
	pg_stuff = UseMemory((memory_ref)window->window_long);
	InsetRect(&valid_rect, -pg_stuff->frame_pen_width, -pg_stuff->frame_pen_width);
	UnuseMemory((memory_ref)window->window_long);

	ValidRect(&valid_rect);
	SetPort(ps->old_port);
}


// This one sets the cursor to a resource id if it is not already set to that
void SetPGCNTLCursor (short id)
{	
	if ( id == last_cursor_id )
		return;

	last_cursor_id = id;
	if ( id == 0 )
	#ifndef THINK_C
		SetCursor(&qd.arrow);
	#else
		SetCursor(&arrow);
	#endif

	else {
		CursHandle		curs;
		
		if ((curs = GetCursor(id)) != NULL) {
			char		state = HGetState((Handle)curs);
		
			HNoPurge((Handle)curs);
			HLock((Handle)curs);
			SetCursor(*curs);
			HSetState((Handle)curs, state);
		}
	}
}


/* SendPaigeMessage sends the PAIGE control a message to process. */

long pascal SendPaigeMessage (long pg, short message, WORD wParam, long lParam)
{
	long			result = 0;
	
	if (message == WM_SETFOCUS)
		SetPaigeFocus(pg);
	else
	if (message == WM_KILLFOCUS)
		SetPaigeFocus(0);
	else
		result = ControlWinProc((HWND)pg, message, wParam, lParam);

	return	result;
}


/* GetPaigeRefCon returns your refCon value given in NewPaigeControl(). */

long pascal GetPaigeRefCon (long pg)
{
	pg_window_ptr			window;
	
	window = (pg_window_ptr)pg;
	
	return	window->refCon;
}


/* IsPaigeEvent should be called after every event is obtained. If the event was meant for
the control it returns TRUE (which means you do nothing else). Otherwise FALSE is returned
(which means you must go ahead and process the control). */

long pascal IsPaigeEvent (EventRecord *event)
{
	pg_window_ptr			window;
	WindowPeek				update_window;
	UINT					the_key;
	long					message, index;
	long					result = 0;

	if (!libInstance)
		return	FALSE;

	switch (event->what)  {
		
		case nullEvent:
			PaigeControlIdle();
			break;
			
		case mouseDown:
			result = handle_click(event);
			break;
		
		case mouseUp:
			if (focus) {
				
				if (!focus->mouse_down)
					result = FALSE;
				else {
				
					ControlWinProc((HWND)focus, WM_LBUTTONUP, 0, 0);
					result = (long)focus;
				}
			}
			
			focus->mouse_down = FALSE;
			break;

		case keyDown:
		case autoKey:
			if (event->modifiers & cmdKey)
				break;

			if ((window = focus) != NULL) {
				
				result = (long)window;

				the_key = (UINT)event->message & charCodeMask;
				message = WM_CHAR;

				if (the_key < ' ' || the_key == VK_DELETE) {
					
					if (the_key == VK_LEFT
						|| the_key == VK_UP
						|| the_key == VK_RIGHT
						|| the_key == VK_DOWN
						|| the_key == VK_HOME
						|| the_key == VK_END
						|| the_key == VK_PRIOR
						|| the_key == VK_NEXT
						|| the_key == VK_DELETE)
						message = WM_KEYDOWN;
				}
				
				ControlWinProc((HWND)focus, message, the_key, (LPARAM)event->modifiers);
			}

			break;

		case updateEvt:
			if (event->message == 0)
				break;

			index = 0;

			while ((window = find_paige_window((WindowPtr)event->message, &index)) != NULL) {
				
				ControlWinProc((HWND)window, WM_PAINT, 0, 0);
				index += 1;
			}

			update_window = (WindowPeek)event->message;
			
			if (update_window->updateRgn)
				if (EmptyRgn(update_window->updateRgn))
					result = (long)window;

			break;

		case activateEvt:
			index = 0;

			while ((window = find_paige_window((WindowPtr)event->message, &index)) != NULL) {
				
				if (event->modifiers & 1) {
				
					if (!focus)
						SetPaigeFocus((long)window);
					else
					if (focus == window)
						ControlWinProc((HWND)window, WM_SETFOCUS, 0, 0);
				}
				else
				if (focus == window)
					SetPaigeFocus(0);

				index += 1;
			}

			break;

		case osEvt:
			break;
	}

	return	result;
}


/* PaigeControlIdle should be called if you do *NOT* get null events from the event loop. This
is necessary to blink the caret and process mouse-movements. */

void pascal PaigeControlIdle (void)
{
	pg_window_ptr			window;
	paige_control_ptr		pg_stuff;
	GrafPtr					old_port;
	Point					mouse;
	long					lParam;

	if ((window = focus) != NULL) {
		
		GetPort(&old_port);
		SetPort(window->w_ptr);
		
		if (window->window_long) {

			pg_stuff = UseMemory((memory_ref)window->window_long);
			
			if (pgIdle(pg_stuff->pg)) {
				
				SendChangeCommand(pg_stuff);
				UpdateScrollbars((HWND)focus, pg_stuff, TRUE);
				NotifyOverflow(pg_stuff);
			}
			
			UnuseMemory((memory_ref)window->window_long);
		}

		GetMouse(&mouse);
		lParam = mouse.v;
		lParam <<= 16;
		lParam |= (unsigned short)mouse.h;

		if (mouse.h != window->last_mouse.h || mouse.v != window->last_mouse.v)
			ControlWinProc((HWND)focus, WM_MOUSEMOVE, 0, lParam);

		SetPort(old_port);
	}
}


// SetPaigeFocus changes the focus to pg. If pg is zero, the current item of focus is cleared.

void pascal SetPaigeFocus (long pg)
{
	if (focus == (pg_window_ptr)pg)
		return;

	if (focus)
		ControlWinProc((HWND)focus, WM_KILLFOCUS, 0, 0);
	
	focus = (pg_window_ptr)pg;

	if (focus)
		ControlWinProc((HWND)focus, WM_SETFOCUS, 0, 0);
	else
		SetPGCNTLCursor(0);
}


// GetPaigeFocus returns the item of focus, if any.

long pascal GetPaigeFocus (void)
{
	return	(long)focus;
}


/* SizePGWindow sizes the controlHandles for WM_SIZE and WM_MOVE. */

void SizePGWindow (HWND hWnd, long message, long PG_FAR *hvalue, long PG_FAR *vvalue)
{
	ControlHandle			ctl_h, ctl_v;
	memory_ref				pgstuff_ref;
	paige_control_ptr		pg_stuff;
	pg_window_ptr			window;
	GrafPtr					old_port;
	long					h_val, v_val, h, v;

	window = (pg_window_ptr)hWnd;

	ctl_h = window->h_scroll;
	ctl_v = window->v_scroll;
	GetPort(&old_port);
	SetPort(window->w_ptr);
	
	h = *hvalue;
	v = *vvalue;

	if (message == WM_MOVE) {
		
		v_val = v - window->vis_bounds.top;
		h_val = h - window->vis_bounds.left;
		
		OffsetRect( &window->full_vis_bounds,h_val,v_val );
		OffsetRect( &window->vis_bounds,h_val,v_val );

		if (ctl_h)
			invalidate_control(ctl_h, h_val, v_val, 0, 0);
		if (ctl_v)
			invalidate_control(ctl_v, h_val, v_val, 0, 0);
	}
	else {
		
		window->full_vis_bounds.right = window->full_vis_bounds.left + h;
		window->full_vis_bounds.bottom = window->full_vis_bounds.top + v;

		v_val = v - window->window_height;
		h_val = h - window->window_width;
		
		if (ctl_h) {
			
			*hvalue -= ((**ctl_h).contrlRect.bottom - (**ctl_h).contrlRect.top);
			invalidate_control(ctl_h, 0, v_val, h_val, 0);
		}
		
		if (ctl_v) {

			*vvalue -= ((**ctl_v).contrlRect.right - (**ctl_v).contrlRect.left);
			invalidate_control(ctl_v, h_val, 0, 0, v_val);
		}

		window->window_height = v;
		window->window_width = h;
		
		window->vis_bounds.bottom += v_val;
		window->vis_bounds.right += h_val;
	}
	
	SetPort(old_port);
}



/* SelectObject changes the pen characteristics in the specified port. */

long SelectObject (HDC hDC, long object)
{
	GrafPtr			old_port, new_port;
	long			component;
	short			pensize;
	RGBColor		fg_color;

	GetPort(&old_port);
	new_port = (GrafPtr)hDC;
	SetPort(new_port);
	
	PenNormal();

	component = (object & HPEN_RED) >> 8;
	fg_color.red = (unsigned short)(component * 4369);
	component = (object & HPEN_GREEN) >> 4;
	fg_color.green = (unsigned short)(component * 4369);
	component = object & HPEN_BLUE;
	fg_color.blue = (unsigned short)(component * 4369);
	RGBForeColor(&fg_color);
	pensize = HiWord(object);
	pensize &= 0x00FF;
	PenSize(pensize, pensize);
	
	SetPort(old_port);

	return	HPEN_NORMAL;
}



long pascal DefWindowProc (HWND hWnd, short message, WORD wParam, LPARAM lParam)
{
#pragma unused (hWnd, message, wParam, lParam)

	return			0;
}


void InvalidateRect (HWND hWnd, RECT *r, short erase)
{
	GrafPtr			old_port;
	pg_window_ptr	window;

#pragma unused (erase)

	window = (pg_window_ptr)hWnd;
	GetPort(&old_port);
	SetPort(window->w_ptr);
	InvalRect((Rect *)r);
	SetPort(old_port);
}


/* SetScrollRange sets the scrollable range of vertical or horizontal scrollbar. */

void SetScrollRange (HWND hWnd, short what_ctl, short min, short max, short redraw)
{
	pg_window_ptr			window;
	ControlHandle			ctl;
	GrafPtr					cur_port;
	RgnHandle				vis_rgn;
	Rect					clip;

#pragma unused (min)

	window = (pg_window_ptr)hWnd;
	GetPort(&cur_port);
	SetPort(window->w_ptr);
	
	if (redraw)
		vis_rgn = NULL;
	else {
		
		vis_rgn = NewRgn();
		CopyRgn(window->w_ptr->visRgn, vis_rgn);
		SetRect(&clip, 0, 0, 0, 0);
		RectRgn(window->w_ptr->visRgn, &clip);
	}

	if (what_ctl == SB_VERT)
		ctl = window->v_scroll;
	else
		ctl = window->h_scroll;
	
	if (ctl)
		SetCtlMax(ctl, max);

	if (vis_rgn) {
		
		CopyRgn(vis_rgn, window->w_ptr->visRgn);
		DisposeRgn(vis_rgn);
	}

	SetPort(cur_port);
}


/* SetScrollRange sets the scroll position of vertical or horizontal scrollbar. */

void SetScrollPos (HWND hWnd, short what_ctl, short value, short redraw)
{
	pg_window_ptr			window;
	ControlHandle			ctl;
	GrafPtr					cur_port;
	RgnHandle				vis_rgn;
	Rect					clip;

	window = (pg_window_ptr)hWnd;
	GetPort(&cur_port);
	SetPort(window->w_ptr);
	
	if (redraw)
		vis_rgn = NULL;
	else {
		
		vis_rgn = NewRgn();
		CopyRgn(window->w_ptr->visRgn, vis_rgn);
		SetRect(&clip, 0, 0, 0, 0);
		RectRgn(window->w_ptr->visRgn, &clip);
	}

	if (what_ctl == SB_VERT)
		ctl = window->v_scroll;
	else
		ctl = window->h_scroll;
	
	if (ctl)
		SetCtlValue(ctl, value);
	
	if (vis_rgn) {
		
		CopyRgn(vis_rgn, window->w_ptr->visRgn);
		DisposeRgn(vis_rgn);
	}

	SetPort(cur_port);
}


/* ActivatePGScrollbars changes the hilite state of the scrollbars. */

void ActivatePGScrollbars (HWND hWnd, long message)
{
	ControlHandle			ctl_h, ctl_v;
	pg_window_ptr			window;
	GrafPtr					old_port;
	short					hilite_verb;

	window = (pg_window_ptr)hWnd;
	ctl_h = window->h_scroll;
	ctl_v = window->v_scroll;
	GetPort(&old_port);
	SetPort(window->w_ptr);

	if (message == WM_SETFOCUS)
		hilite_verb = 0;
	else
		hilite_verb = 255;
	
	if (ctl_h || ctl_v) {
	
		if (ctl_h)
			HiliteControl(ctl_h, hilite_verb);
		if (ctl_v)
			HiliteControl(ctl_v, hilite_verb);

		if (window->style & WS_GROWBOX)
			DrawGrowIcon(window->w_ptr);
	}

	SetPort(old_port);
}



/************************************* Local functions **********************************/


STATIC_PASCAL (long) def_callback_proc (HWND hWnd, UINT message, WORD wParam, LPARAM lParam)
{
#pragma unused (hWnd, message, wParam, lParam)

	return	0;
}


/* find_paige_window searchs the list of windows and if a match is found, returns the
PAIGE window ptr record. Search begins at *index and *index is updated. */

static pg_window_ptr find_paige_window (WindowPtr w_ptr, long *index)
{
	pg_window_ptr			result, *windows;
	long					window_qty;
	
	window_qty = GetMemorySize((memory_ref)libInstance);
	windows = UseMemory((memory_ref)libInstance);
	windows += *index;
	window_qty -= *index;

	result = NULL;
	
	while (window_qty) {
		
		result = *windows++;
		
		if (result->w_ptr == w_ptr)
			break;
		
		--window_qty;
		*index += 1;
	}
	
	UnuseMemory((memory_ref)libInstance);
	
	if (window_qty)
		return	result;
	
	return	NULL;
}



/* remove_control_from_window finds the control in w_ptr and, if found, returns TRUE
and removes it from the window list. */

static int remove_control_from_window (WindowPtr w_ptr, ControlHandle ctl)
{
	WindowPeek			owner;
	ControlHandle		next_control, previous_control;
	
	owner = (WindowPeek) w_ptr;
	next_control = owner->controlList;
	previous_control = NULL;

	while (next_control) {
		
		if (next_control == ctl) {
			
			if (previous_control)
				(**previous_control).nextControl = (**next_control).nextControl;
			else
				owner->controlList = (**next_control).nextControl;
			
			return	TRUE;
		}
		
		previous_control = next_control;
		next_control = (**next_control).nextControl;
	}
	
	return	FALSE;
}


// This function places a user rectangle for the "zoom" status
static void put_user_zoom_rect (WindowPeek window, Rect *bounds)
{
	Handle		data_handle;
	Rect		*user_rect;
	
	data_handle = window->dataHandle;
	user_rect = (Rect*) *data_handle;
	*user_rect = *bounds;
}



/* handle_click processess the mouse down event. */

static long handle_click (EventRecord *event)
{
	short			window_item;
	long			index;
	pg_window_ptr	window;
	GrafPtr			old_port;
	Point			mouse;
	WindowPtr		clicked_window;
	long			lParam, message;

	if (!(window_item = FindWindow(event->where, &clicked_window)))
		return	0;
	
	GetPort(&old_port);
	window = NULL;
	
	if (window_item == inContent || window_item == inGrow) {
		
		index = 0;
		
		while ((window = find_paige_window(clicked_window, &index)) != NULL) {
			
			if (!(window->style & WS_GROWBOX) || window_item != inGrow) {
			
				SetPort(window->w_ptr);
				mouse = event->where;
				GlobalToLocal(&mouse);

				if (PtInRect(mouse, &window->full_vis_bounds))
					break;
			}

			++index;
		}
		
		if (window) {
			
			window->mouse_down = TRUE;

			if (window != focus) {
			
				SetPaigeFocus((long)window);
			}
			else
			if (!tracked_scrollbars(window, mouse)) {
				
				lParam = mouse.v;
				lParam <<= 16;
				lParam |= mouse.h;
				
				if ((event->when - last_ticks) <= GetDblTime())
					message = WM_LBUTTONDBLCLK;
				else
					message = WM_LBUTTONDOWN;
				
				last_ticks = event->when;
				ControlWinProc((HWND)window, message, (UINT)event->modifiers, lParam);
			}
		}
		
		SetPort(old_port);
	}	

	return	(long)window;
}


/* tracked_scrollbars checks for scrollbar tracking and, if so, tracks it and returns TRUE. */

static int tracked_scrollbars (pg_window_ptr window, Point mouse)
{
	Point			start_pt;
	short			part_code;
	ControlHandle	the_control;
	LPARAM			lParam;
	long			message;

	start_pt = mouse;
	
	if ((part_code = FindControl(start_pt, window->w_ptr, &the_control)) != 0) {
		
		if ((the_control != window->h_scroll) && (the_control != window->v_scroll))
			return	FALSE;

		scrolling_doc = window;
		window->mouse_down = FALSE;

		if (part_code == inThumb) {
			
			if (TrackControl(the_control, start_pt, NULL)) {
				
				if (the_control == window->v_scroll)
					message = WM_VSCROLL;
				else
					message = WM_HSCROLL;
				
				lParam = GetCtlValue(the_control);
				ControlWinProc((HWND)window, message, SB_THUMBPOSITION, lParam);
			}
		}
		else {
			
			init_scroll_proc();
			TrackControl(the_control, start_pt, action_proc);
		}
	}

	return (part_code != 0); 
}


// This is the scrolling action proc, called by Control Manager as a scrollbar
// continues to be clicked.

static pascal void scroll_action_proc (ControlHandle control, short part_code)
{
	long			message;
	WORD			wParam;

	if (control == scrolling_doc->v_scroll)
		message = WM_VSCROLL;
	else
		message = WM_HSCROLL;

	switch (part_code) {
		
		case inUpButton:
			wParam = SB_LINEUP;
			break;
			
		case inDownButton:
			wParam = SB_LINEDOWN;
			break;
			
		case inPageUp:
			wParam = SB_PAGEUP;
			break;
			
		case inPageDown:
			wParam = SB_PAGEDOWN;
			break;
	}
	
	ControlWinProc((HWND)scrolling_doc, message, wParam, 0);
}


/* init_scroll_proc initializes the action proc control for PowerMac (and Mac 68K). */

static void init_scroll_proc (void)
{
	if (!action_proc)
		action_proc = NewControlActionProc(scroll_action_proc);
}


/* invalidate_control invalidates the control rectangle, and offsets it by the two amounts
then invalidats it again. */

static void invalidate_control (ControlHandle ctl, short offset_h, short offset_v,
		short inset_right, short inset_bottom)
{
	Rect			ctl_rect;
	
	ctl_rect = (**ctl).contrlRect;
	InvalRect(&ctl_rect);
	
	OffsetRect(&ctl_rect, offset_h, offset_v);
	ctl_rect.right += inset_right;
	ctl_rect.bottom += inset_bottom;
	InvalRect(&ctl_rect);
	(**ctl).contrlRect = ctl_rect;
}


#endif
// endif MAC_PLATFORM
