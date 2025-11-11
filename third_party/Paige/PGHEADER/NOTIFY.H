#ifndef NOTIFY_H
#define NOTIFY_H

#ifndef PG_NOTIFYMESSAGES
#define PG_NOTIFYMESSAGES (WM_USER + 0x3200)
#endif

#define PGN_WAIT				(PG_NOTIFYMESSAGES)			// Called for "wait" processing
#define PGN_SETDC				(PG_NOTIFYMESSAGES + 1)		// Called to modify device context
#define PGN_RELEASEDC			(PG_NOTIFYMESSAGES + 2)		// Called to release DC
#define PGN_LINEDRAW			(PG_NOTIFYMESSAGES + 3)		// Called for line drawing ornaments
#define PGN_PAGEDRAW			(PG_NOTIFYMESSAGES + 4)		// Called after drawing page(s)
#define PGN_EXDRAW				(PG_NOTIFYMESSAGES + 5)		// Called for drawing exclusions
#define PGN_BACKDRAW			(PG_NOTIFYMESSAGES + 6) 	// Called to draw background
//QUALCOMM == Kusuma  -- Changed PGN_SCROLL to PGN_AUTOSCROLL
//to avoid redefinition in Eudora (defined in InetSDK)
//We don't use this in either the PaigeDLL or Eudora, only the Paige
//control uses it.
#define PGN_AUTOSCROLL			(PG_NOTIFYMESSAGES + 8)		// Called for auto-scrolling
#define PGN_CLICK				(PG_NOTIFYMESSAGES + 10)	// Called when mouse clicked in text or container item
#define PGN_DBLCLICK			(PG_NOTIFYMESSAGES + 11)	// Called when mouse double clicked in text item
#define PGN_CLICKMOUSEUP		(PG_NOTIFYMESSAGES + 12)	// Notify for clicks on mouse UP only
#define PGN_MOUSEMOVE			(PG_NOTIFYMESSAGES + 13)	// Called any time mouse moved
#define PGN_CURSOR				(PG_NOTIFYMESSAGES + 14)	// Called when PAIGE things cursor should change
#define PGN_PAGECLICK			(PG_NOTIFYMESSAGES + 15)	// Called when mouse-down anywhere inside page area
#define PGN_EXCLICK				(PG_NOTIFYMESSAGES + 16)	// Called when mouse-down anywhere inside exclusion area
#define PGN_UNDOPREP			(PG_NOTIFYMESSAGES + 17)	// Called after PrepareUndo
#define PGN_EMBEDPROC			(PG_NOTIFYMESSAGES + 18)	// Becomes embed_ref callback function
#define PGN_READFILE			(PG_NOTIFYMESSAGES + 19)	// Read handler callback
#define PGN_WRITEFILE			(PG_NOTIFYMESSAGES + 20)	// Write handler callback
#define PGN_ENDREADWRITE		(PG_NOTIFYMESSAGES + 21)	// Called right after read or write processed
#define PGN_XPAGEMODIFY			(PG_NOTIFYMESSAGES + 22)	// Called to give app a chance to modify page
#define PGN_YPAGEMODIFY			(PG_NOTIFYMESSAGES + 23)	// Called to give app a chance to modify page
#define PGN_CHAR				(PG_NOTIFYMESSAGES + 24)	// Called before processing WM_CHAR
#define PGN_CHANGE				(PG_NOTIFYMESSAGES + 25)	// Change has occurred
#define PGN_OVERFLOW			(PG_NOTIFYMESSAGES + 26)	// Text has overflowed
#define PGN_SUBSETDRAW			(PG_NOTIFYMESSAGES + 27)	// ** reserved for DSI **
#define PGN_FAILURE				(PG_NOTIFYMESSAGES + 28)	// Out-of-memory or other notification
#define PGN_GETCLIPBOARD		(PG_NOTIFYMESSAGES + 29)	// Read the clipboard
#define PGN_WRITECLIPBOARD		(PG_NOTIFYMESSAGES + 30)	// Write the clipboard
#define PGN_RCLICK				(PG_NOTIFYMESSAGES + 31)	// Called when right clicked in text or container item
#define PGN_TAB 				(PG_NOTIFYMESSAGES + 32)	// The TAB key has been pressed
#define PGN_SHIFT_TAB			(PG_NOTIFYMESSAGES + 33)	// The TAB key has been pressed and the SHIFT key is down
#define PGN_SELECT_CHANGE		(PG_NOTIFYMESSAGES + 34)	// The selection has changed
#define PGN_CTRLSHFT_F			(PG_NOTIFYMESSAGES + 35)	// Ctrl-Shift-F has been pressed to select font name combo //APG 1997.11.14.14.40
#define PGN_CTRLSHFT_P			(PG_NOTIFYMESSAGES + 36)	// Ctrl-Shift-P has been pressed to select font size combo //APG 1997.11.14.14.40
#define PGN_CTRLKEY_BOLD		(PG_NOTIFYMESSAGES + 37)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_ITALIC		(PG_NOTIFYMESSAGES + 38)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_UNDERLINE	(PG_NOTIFYMESSAGES + 39)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_JUSTLEFT	(PG_NOTIFYMESSAGES + 40)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_JUSTCENTER	(PG_NOTIFYMESSAGES + 41)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_JUSTRIGHT	(PG_NOTIFYMESSAGES + 42)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_SELECTALL	(PG_NOTIFYMESSAGES + 43)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_UNDO		(PG_NOTIFYMESSAGES + 44)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_CUT			(PG_NOTIFYMESSAGES + 45)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_COPY		(PG_NOTIFYMESSAGES + 46)	//APG inserted 97.12.09.13.41
#define PGN_CTRLKEY_PASTE		(PG_NOTIFYMESSAGES + 47)	//APG inserted 97.12.09.13.41

#define PGN_SETFOCUS		(PG_NOTIFYMESSAGES + 10000)		// Paige has received the focus from lParam
#define PGN_KILLFOCUS		(PG_NOTIFYMESSAGES + 20000)		// Paige will lose the focus to lParam

#endif
