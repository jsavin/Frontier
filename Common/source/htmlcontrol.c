
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

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

#include "frontier.h"
#include "standard.h"

#define NEEDMOREWIN 1

#include "standard.h"

#include "htmlcontrol.h"

#include "WinLand.h"
#include "dialogs.h"
#include "file.h"
#include "font.h"
#include "kb.h"
#include "menu.h"
#include "mouse.h"
#include "quickdraw.h"
#include "scrap.h"
#include "strings.h"
#include "frontierwindows.h"
#include "cancoon.h"
#include "cancooninternal.h"
#include "shell.h"
#include "shellprivate.h"
#include "shellmenu.h"
#include "lang.h"
#include "wininet.h"

extern HWND hwndHTMLControl;

#if (FRONTIERWEB==1)
tywebappInfo gwebappInfo = {false, NULL};

HANDLE webappStartup () {

	gwebappInfo.flactive = false;

	gwebappInfo.hwebappModule = LoadLibrary ("WEBAPPDLL.DLL");
	
	if (gwebappInfo.hwebappModule == NULL)
		gwebappInfo.hwebappModule = LoadLibrary ("DLLS\\WEBAPPDLL.DLL");
		
	if (gwebappInfo.hwebappModule == NULL)
		gwebappInfo.hwebappModule = LoadLibrary ("..\\DLLS\\WEBAPPDLL.DLL");

	if (gwebappInfo.hwebappModule != NULL) {
		gwebappInfo.webappSetup = (tywebappSetup) GetProcAddress (gwebappInfo.hwebappModule, "webappSetup");
		gwebappInfo.webappWndProc = (tywebappWndProc) GetProcAddress (gwebappInfo.hwebappModule, "webappWndProc");
		gwebappInfo.webappBack = (tywebappNoParam) GetProcAddress (gwebappInfo.hwebappModule, "webappBack");
		gwebappInfo.webappForward = (tywebappNoParam) GetProcAddress (gwebappInfo.hwebappModule, "webappForward");
		gwebappInfo.webappHome = (tywebappNoParam) GetProcAddress (gwebappInfo.hwebappModule, "webappHome");
		gwebappInfo.webappStop = (tywebappNoParam) GetProcAddress (gwebappInfo.hwebappModule, "webappStop");
		gwebappInfo.webappRefresh = (tywebappNoParam) GetProcAddress (gwebappInfo.hwebappModule, "webappRefresh");
		gwebappInfo.webappNavigate = (tywebappNavigate) GetProcAddress (gwebappInfo.hwebappModule, "webappNavigate");
		gwebappInfo.webappVersion = (tywebappVersion) GetProcAddress (gwebappInfo.hwebappModule, "webappVersion");
		gwebappInfo.webappIsOffline = (tywebappIsOffline) GetProcAddress (gwebappInfo.hwebappModule, "webappIsOffline");
		gwebappInfo.webappSetOffline = (tywebappSetOffline) GetProcAddress (gwebappInfo.hwebappModule, "webappSetOffline");
		}

	return (gwebappInfo.hwebappModule);
	} /*webappStartup*/

boolean webappShutdown () {
//	if ((gwebappInfo.webappSetup != NULL) && (gwebappInfo.webappWndProc != NULL))
//		(*(gcomServerInfo.comclear)) ();

	if (gwebappInfo.hwebappModule != NULL)
		FreeLibrary (gwebappInfo.hwebappModule);

	gwebappInfo.webappSetup = NULL;
	gwebappInfo.webappWndProc = NULL;
	gwebappInfo.hwebappModule = NULL;
	return (TRUE);
	} /*webappShutdown*/

long doweb (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
//	long MDI;

//	MDI = GetWindowLong(hwndMDIClient, GWL_USERDATA);

//	if ((MDI != 0L) && (MDI == GetWindowLong(hwnd, GWL_USERDATA)))
		if (htmlcontrolactive())
			return ((*(gwebappInfo.webappWndProc))(hwnd, msg, wParam, lParam));
	return (1);
}

#endif

void htmlcontrolback () {
	#if (FRONTIERWEB==1)
		if (gwebappInfo.webappBack == NULL)
			return;

		//(*(gwebappInfo.webappBack))();

		releasethreadglobals ();

		SendMessage (hwndHTMLControl, WM_USER+500, 0, 0);

		grabthreadglobals ();
	#endif
	}

boolean htmlcontrolsetoffline (boolean floffline) {

	#if (FRONTIERWEB==1)

		/*releasethreadglobals ();

		SendMessage (hwndHTMLControl, WM_USER+501, floffline, 0);

		grabthreadglobals ();*/

	INTERNET_CONNECTED_INFO ci;

    memset(&ci, 0, sizeof(ci));
    if(floffline) {
        ci.dwConnectedState = INTERNET_STATE_DISCONNECTED_BY_USER;
        ci.dwFlags = ISO_FORCE_DISCONNECTED;
    } else {
        ci.dwConnectedState = INTERNET_STATE_CONNECTED;
    }

    InternetSetOption(NULL, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof(ci));

	#endif

	return (true);
	}

	
void htmlcontrolforward () {
	#if (FRONTIERWEB==1)
		if (gwebappInfo.webappForward == NULL)
			return;

		(*(gwebappInfo.webappForward))();
	#endif
	}

void htmlcontrolrefresh () {
	#if (FRONTIERWEB==1)
		if (gwebappInfo.webappRefresh == NULL)
			return;

		(*(gwebappInfo.webappRefresh))();
	#endif
	}

void htmlcontrolhome () {
	#if (FRONTIERWEB==1)
		if (gwebappInfo.webappHome == NULL)
			return;

		(*(gwebappInfo.webappHome))();
	#endif
	}

void htmlcontrolstop () {
	#if (FRONTIERWEB==1)
		if (gwebappInfo.webappStop == NULL)
			return;

		(*(gwebappInfo.webappStop))();
	#endif
	}

void htmlcontrolnavigate (Handle htext) {
	#if (FRONTIERWEB==1)
		unsigned long len;
		char buff[2048];

		if (gwebappInfo.webappNavigate == NULL)
			return;

		len = gethandlesize (htext);

		memmove (buff, *htext, len);

		buff[len] = 0;

		releasethreadglobalsnopriority();

		(*(gwebappInfo.webappNavigate))(buff);

		grabthreadglobalsnopriority();
	#endif
	}

boolean htmlcontrolversion (unsigned short * majorVersion, unsigned short * minorVersion) {
	#if (FRONTIERWEB==1)

		unsigned long version;

		if (gwebappInfo.webappVersion != NULL)
			version = (*(gwebappInfo.webappVersion))();
		else
			version = 0L;

		*majorVersion = HIWORD(version);
		*minorVersion = LOWORD(version);
		return (true);

	#else

		*majorVersion = 0;
		*minorVersion = 0;
		return (false);

	#endif
	}

boolean htmlcontrolpresent () {
	#if (FRONTIERWEB==1)

		if (gwebappInfo.hwebappModule != NULL)
			return (true);
		else
			return (false);

	#else

		return (false);

	#endif
	}

boolean htmlcontrolactive () {
	#if (FRONTIERWEB==1)

		return (gwebappInfo.flactive);

	#else

		return (false);

	#endif
	}

boolean htmlcontrolisoffline (boolean * fl) {

	#if (FRONTIERWEB==1)


    DWORD   dwState = 0, dwSize = sizeof(DWORD);
    BOOL    fRet = FALSE;

    if(InternetQueryOption(NULL, INTERNET_OPTION_CONNECTED_STATE, &dwState,
        &dwSize))
    {
        if(dwState & INTERNET_STATE_DISCONNECTED_BY_USER)
            fRet = TRUE;
    }

	*fl = fRet;

    return (true);


	/*	short bl;

		if (gwebappInfo.webappIsOffline == NULL)
			return (false);

		if ((*(gwebappInfo.webappIsOffline))(&bl)) {
			*fl = bl;
			return (true);
			}
		else {
			return (false);
			}*/
	#else
		return (false);
	#endif*/
	}

boolean htmlcontrolsetofflinehandler (boolean  fl) {
	#if (FRONTIERWEB==1)
		short bl;
		boolean floffline = false;

		if (gwebappInfo.webappSetOffline == NULL)
			return (false);

		bl = fl;
		releasethreadglobals ();

		floffline = (*(gwebappInfo.webappSetOffline)) (bl);
		
		grabthreadglobals ();

		return (floffline);

	#else
		return (false);
	#endif
	}


long CALLBACK htmlControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	
#if (FRONTIERWEB==1)
	
	doweb (hwnd, msg, wParam, lParam);


	if (msg == WM_USER+500)
		(*(gwebappInfo).webappBack) ();

	if (msg == WM_USER+501)
		htmlcontrolsetofflinehandler (wParam);

	switch (msg) {
		case WM_WINDOWPOSCHANGING:
			{
			WINDOWPOS * wp;

			if (hwndMDIClient != NULL) {

				wp = (WINDOWPOS *) lParam;

				wp->flags |= SWP_NOZORDER;
				
				}
			break;
			}
		}
#endif

	return (DefMDIChildProc (hwnd, msg, wParam, lParam));
	}
