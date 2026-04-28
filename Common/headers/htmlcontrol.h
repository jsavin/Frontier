
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

#if (FRONTIERWEB==1)
typedef long (WINAPI * tywebappSetup) (HINSTANCE hInstance, HWND hwnd, HWND hwndStatus, char * initialURL);
typedef void (WINAPI * tywebappNoParam) ();
typedef void (WINAPI * tywebappNavigate) (char * nameto);
typedef long (WINAPI * tywebappVersion) ();
typedef short (WINAPI * tywebappIsOffline) (short * fl);
typedef short (WINAPI * tywebappSetOffline) (short fl);

typedef LRESULT (WINAPI * tywebappWndProc) (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct tywebappInfo {
	boolean flactive;
	HMODULE hwebappModule;
	tywebappSetup webappSetup;
	tywebappWndProc webappWndProc;
	tywebappNoParam webappBack;
	tywebappNoParam webappForward;
	tywebappNoParam webappHome;
	tywebappNoParam webappStop;
	tywebappNoParam webappRefresh;
	tywebappNavigate webappNavigate;
	tywebappVersion webappVersion;
	tywebappIsOffline webappIsOffline;
	tywebappSetOffline webappSetOffline;
	} tywebappInfo;

extern tywebappInfo gwebappInfo;


HANDLE webappStartup ();
boolean webappShutdown ();
long doweb (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif
long CALLBACK htmlControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void htmlcontrolback ();
void htmlcontrolforward ();
void htmlcontrolrefresh ();
void htmlcontrolhome ();
void htmlcontrolstop ();
void htmlcontrolnavigate (Handle htext);
boolean htmlcontrolversion (unsigned short * majorVersion, unsigned short * minorVersion);
boolean htmlcontrolpresent ();
boolean htmlcontrolactive ();
boolean htmlcontrolisoffline (boolean * fl);
boolean htmlcontrolsetoffline (boolean fl);
