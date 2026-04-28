
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef dialogsinclude
#define dialogsinclude /*so other includes can tell if we've been loaded*/

#define okitem 1 /*can be used by any dialog if something more specific isn't needed*/
#define cancelitem 2

#define quitdialogid 254 /*standard �quit� dialog (shares items with Save)*/

#define savedialogid 256 /*standard �save� dialog*/
#define saveyesitem 1
#define savecancelitem 2
#define savenoitem 3
#define savefileprompt 4
#define savequitprompt 5

// #define replacedialogid 257 /* 2005-09-26 creedon - replaced the single occurrence use of this dialog with call to threewaydialog */
#define replacereplaceitem 1
#define replaceduplicateitem 2
#define replacecancelitem 3

#define askdialogid 262 /*standard �ask� dialog for UserLand language*/
#define askokitem 1
#define askcancelitem 2
#define askpromptitem 3
#define askansweritem 4

#define msgdialogid 263 /*standard �msg� dialog for UserLand language*/
#define msgokitem 1
#define msgcancelitem 2
#define msgmsgitem 3

#define twowaydialogid 263 /*standard �msg� dialog for UserLand language*/
#define twowayokitem 1
#define twowaycancelitem 2
#define twowaymsgitem 3

#define threewaydialogid 264 /*standard �msg� dialog for UserLand language*/
#define threewayyesitem 1
#define threewaynoitem 2
#define threewaycancelitem 3
#define threewaymsgitem 4

#define intdialogid 265 /*dialog for prompting for a single number*/
#define intokitem 1
#define intcancelitem 2
#define intpromptitem 3
#define intintitem 4

#define chardialogid 266 /*dialog for prompting for a single character*/
#define charokitem 1
#define charcancelitem 2
#define charpromptitem 3
#define charvalitem 4

#define sferrordialogid 260 /*error while std file is up*/

#define alertdialogid 261 /*standard �alert� dialog for UserLand language*/
#define alertokitem 1
#define alertmsgitem 3

#define revertdialogid 269 /*standard �revert� dialog for UserLand language*/
#define revertokitem 1

#define newvaluedialogid 255 /*zooming unknown value*/
#define valuenameitem 4
#define firstkinditem 5
#define lastkinditem 10

/* dialog button text */

	#define cancelbuttontext BIGSTRING ("\x06" "Cancel")
	#define duplicatebuttontext BIGSTRING ("\x09" "Duplicate")
	#define replacebuttontext BIGSTRING ("\x07" "Replace")



typedef boolean (*dialogcallback) (DialogPtr, short);


/*function prototypes*/

extern short dialogcountitems (DialogPtr);

extern void boldenbutton (DialogPtr, short);

extern void positiondialogwindow (DialogPtr);

extern void disabledialogitem (DialogPtr, short);

extern void enabledialogitem (DialogPtr, short);

extern void hidedialogitem (DialogPtr, short);

extern void showdialogitem (DialogPtr, short);

extern void setdefaultitem (DialogPtr, short);

extern boolean dialogitemisbutton (DialogPtr, short);

extern DialogPtr newmodaldialog (short, short);

extern void disposemodaldialog (DialogPtr);

extern void setdialogcheckbox (DialogPtr, short, boolean);

extern boolean getdialogcheckbox (DialogPtr, short);

extern void toggledialogcheckbox (DialogPtr, short);

extern boolean setdialogradiovalue (DialogPtr, short, short, short);

extern short getdialogradiovalue (DialogPtr, short, short);

extern void setdialogicon (DialogPtr, short, short);

extern void setdialogtext (DialogPtr, short, bigstring);

extern void getdialogtext (DialogPtr, short, bigstring);

extern void selectdialogtext (DialogPtr, short);

extern short getdialogint (DialogPtr, short);

extern void setdialogint (DialogPtr, short, short);

extern OSType getdialogostype (DialogPtr, short);

extern void setdialogostype (DialogPtr, short, OSType);

extern void setdialogbutton (DialogPtr, short, bigstring);

extern void dialoggetobjectrect (DialogPtr, short, Rect *);

extern void dialogsetobjectrect (DialogPtr, short, Rect);

extern boolean dialogsetfontsize (DialogPtr, short, short);

extern boolean ptinuseritem (Point, DialogPtr, short);

extern boolean setuseritemdrawroutine (DialogPtr, short, dialogcallback);

extern pascal boolean modaldialogcallback (DialogPtr, EventRecord *, short *);

extern void dialogupdate (EventRecord *, DialogPtr);

extern boolean dialogevent (EventRecord *, DialogPtr, short *);

extern boolean dialogidle (DialogPtr);

extern boolean dialogactivate (DialogPtr, boolean);

extern boolean dialogsetselect (DialogPtr, short, short);

extern boolean dialogselectall (DialogPtr);

extern boolean dialoggetselect (DialogPtr, short *, short *);

extern short savedialog (bigstring);

extern short replacevariabledialog (bigstring);

extern boolean revertdialog (bigstring);

extern boolean askdialog (bigstring, bigstring);

extern boolean askpassword (bigstring, bigstring);

extern boolean twowaydialog (bigstring, bigstring, bigstring);

extern short threewaydialog (bigstring, bigstring, bigstring, bigstring);

extern boolean initdialogs (void);

extern boolean intdialog (bigstring, short *);

extern boolean chardialog (bigstring, short *);

extern boolean msgdialog (bigstring);

extern boolean alertdialog (bigstring);

extern boolean alertstring (short);

extern short customalert (short, bigstring);

extern boolean customdialog (short, short, dialogcallback);

        #ifdef FRONTIER_PORTABLE
        #if !defined(FRONTIER_USE_PORTABLE_HANDLES)
        typedef unsigned char* StringPtr;
        #endif
        #endif
        char X0_p2cstrcpy(char *dst, StringPtr src);

#endif
