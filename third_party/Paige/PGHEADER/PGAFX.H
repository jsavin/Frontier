#if defined(WIN32_COMPILE) && defined(USING_MFC)
#ifndef _AFX_NOFORCE_LIBS
#ifndef _MAC

/////////////////////////////////////////////////////////////////////////////
// Win32 libraries

#ifndef _AFXDLL
	#ifndef _UNICODE
		#ifdef _DEBUG
			#pragma comment(lib, "nafxcwd.lib")
		#else
			#pragma comment(lib, "nafxcw.lib")
		#endif
	#else
		#ifdef _DEBUG
			#pragma comment(lib, "uafxcwd.lib")
		#else
			#pragma comment(lib, "uafxcw.lib")
		#endif
	#endif
#else
	#ifndef _UNICODE
		#ifdef _DEBUG
			#pragma comment(lib, "mfc42d.lib")
			#pragma comment(lib, "mfcs42d.lib")
		#else
			#pragma comment(lib, "mfc42.lib")
			#pragma comment(lib, "mfcs42.lib")
		#endif
	#else
		#ifdef _DEBUG
			#pragma comment(lib, "mfc42ud.lib")
			#pragma comment(lib, "mfcs42ud.lib")
		#else
			#pragma comment(lib, "mfc42u.lib")
			#pragma comment(lib, "mfcs42u.lib")
		#endif
	#endif
#endif

#ifdef _DLL
	#if !defined(_AFX_NO_DEBUG_CRT) && defined(_DEBUG)
		#pragma comment(lib, "msvcrtd.lib")
	#else
		#pragma comment(lib, "msvcrt.lib")
	#endif
#else
#ifdef _MT
	#if !defined(_AFX_NO_DEBUG_CRT) && defined(_DEBUG)
		#pragma comment(lib, "libcmtd.lib")
	#else
		#pragma comment(lib, "libcmt.lib")
	#endif
#else
	#if !defined(_AFX_NO_DEBUG_CRT) && defined(_DEBUG)
		#pragma comment(lib, "libcd.lib")
	#else
		#pragma comment(lib, "libc.lib")
	#endif
#endif
#endif

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#else  //!_MAC

//////////////////////////////////////////////////////////////////////////////
// Macintosh libraries

#ifndef _AFXDLL
	#ifdef _DEBUG
		#ifdef _68K_
			#pragma comment(lib, "nafxcmd.lib")
		#else
			#pragma comment(lib, "nafxcpd.lib")
		#endif
	#else
		#ifdef _68K_
			#pragma comment(lib, "nafxcm.lib")
		#else
			#pragma comment(lib, "nafxcp.lib")
		#endif
	#endif
#else
	#ifdef _DEBUG
		#ifdef _68K_
			#error _AFXDLL is not supported for 68K Macintosh
		#else
			#pragma comment(lib, "mfc42pd.lib")
			#pragma comment(lib, "mfcs42pd.lib")
		#endif
	#else
		#ifdef _68K_
			#error _AFXDLL is not supported for 68K Macintosh
		#else
			#pragma comment(lib, "mfc42p.lib")
			#pragma comment(lib, "mfcs42p.lib")
		#endif
	#endif
#endif

#if !defined(_AFXDLL) && !defined(_USRDLL)
	#ifdef _DEBUG
		#pragma comment(lib, "wlmd.lib")
		#pragma comment(lib, "comctld.lib")
	#else
		#pragma comment(lib, "wlm.lib")
		#pragma comment(lib, "comctl.lib")
	#endif
	#pragma comment(linker, "/macres:wlm.rsc /macres:commdlg.rsc")
	#pragma comment(linker, "/macres:comctl.rsc")
	#ifdef _MPPC_
		#pragma comment(lib, "privint.lib")
		#pragma comment(lib, "threads.lib")
		#pragma comment(lib, "math.lib")
	#endif
#else
	#ifdef _DEBUG
		#pragma comment(lib, "msvcwlmd.lib")
		#pragma comment(lib, "msvcctld.lib")
	#else
		#pragma comment(lib, "msvcwlm.lib")
		#pragma comment(lib, "msvcctl.lib")
	#endif
#endif

#ifdef _68K_
	#ifdef _DEBUG
		#pragma comment(lib, "swapd.lib")
	#else
		#pragma comment(lib, "swap.lib")
	#endif
	#pragma comment(lib, "lsanes.lib")
	#pragma comment(lib, "llibcs.lib")
	#pragma comment(lib, "osl.lib")
#endif

#ifdef _MPPC_
	#ifdef _DLL
		#if !defined(_AFX_NO_DEBUG_CRT) && defined(_DEBUG)
			#pragma comment(lib, "msvcrtd.lib")
		#else
			#pragma comment(lib, "msvcrt.lib")
		#endif
	#else
		#if !defined(_AFX_NO_DEBUG_CRT) && defined(_DEBUG)
			#pragma comment(lib, "libcd.lib")
		#else
			#pragma comment(lib, "libc.lib")
		#endif
	#endif
#endif

#endif //_MAC

// force inclusion of NOLIB.OBJ for /disallowlib directives
#pragma comment(linker, "/include:__afxForceEXCLUDE")

// force inclusion of DLLMODUL.OBJ for _USRDLL
#ifdef _USRDLL
#pragma comment(linker, "/include:__afxForceUSRDLL")
#endif

// force inclusion of STDAFX.OBJ for precompiled types
#ifdef _AFXDLL
#pragma comment(linker, "/include:__afxForceSTDAFX")
#endif

#endif //!_AFX_NOFORCE_LIBS

#endif // Wind32_COMPILE
