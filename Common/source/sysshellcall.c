
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "threads.h"
#include "error.h"
#include "langinternal.h" /*for error strings*/
#include "resources.h" /*for error strings*/
#include "shell.h"
#include "sysshellcall.h"



#include "lang.h"
#include "logging.h"
#include "CallMachOFramework.h"
#include <fcntl.h> /* 2006-01-10 creedon */
#include <unistd.h> /* 2025-12-27: for mkstemp, unlink */
#include <sys/wait.h> /* 2025-12-27: for WEXITSTATUS macro */
#include <stdio.h> /* 2025-12-28: for P_tmpdir */

/* 2006-01-29 creedon - define the following for CodeWarrior compilation because it doesn't have these defined in its headers, as Xcode does */
#ifdef __MWERKS__
	#define	F_GETFL		3		/* get file status flags */
	#define	F_SETFL		4		/* set file status flags */
	#define	O_NONBLOCK	0x0004	/* no delay */
#endif //__MWERKS__

/* Maximum length for shell command with redirection. 64KB is a practical limit
   for command strings on modern systems. */
#define MAX_SHELL_COMMAND_LENGTH 65536

/* Maximum length for temporary file path. 256 bytes is sufficient for temp directory
   path plus filename template on all modern systems. */
#define TMPFILE_PATH_MAX 256

/*System.framework functions: popen, pclose, fread, fcntl, feof, and fileno.*/

typedef FILE* (*popenptr) (const char* command, const char *type);
typedef int (*pcloseptr) (FILE* f);
typedef size_t (*freadptr) (void *ptr, size_t size, size_t nobj, FILE* f);
typedef int (*fcntlptr) (int fd, int cmd, int arg); /* 2006-01-10 creedon */
typedef int (*feofptr) (FILE *f); /* 2006-01-10 creedon */
typedef int (*filenoptr) (FILE *f); /* 2006-01-10 creedon */

static popenptr popenfunc;
static pcloseptr pclosefunc;
static freadptr freadfunc;
static fcntlptr fcntlfunc; /* 2006-01-10 creedon */
static feofptr feoffunc; /* 2006-01-10 creedon */
static filenoptr filenofunc; /* 2006-01-10 creedon */

static boolean unixshellcallinited = false;
static CFBundleRef sysBundle = nil;


static boolean unixshellcallinit (void) {

	/*
	2006-01-10 creedon: added fcntlfunc, feoffunc and filenofunc

	7.0b51 PBS: load the bundle and get the function pointers the first time called.
	*/

	if (unixshellcallinited) /*already inited*/
		return (true);

#ifdef FRONTIER_HEADLESS
	/* In headless mode, use standard library functions directly */
	popenfunc = popen;
	freadfunc = fread;
	pclosefunc = pclose;
	fcntlfunc = (fcntlptr) fcntl;
	feoffunc = feof;
	filenofunc = fileno;
#else
	if (sysBundle == nil)
		 if (LoadFrameworkBundle (CFSTR ("System.framework"), &sysBundle) != noErr)
		 	return (false);

	popenfunc = (popenptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("popen"));

	if (popenfunc == nil)
		return (false);

	freadfunc = (freadptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("fread"));

	if (freadfunc == nil)
		return (false);

	pclosefunc = (pcloseptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("pclose"));

	if (pclosefunc == nil)
		return (false);

	fcntlfunc = (fcntlptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("fcntl"));

	if (fcntlfunc == nil)
		return (false);

	feoffunc = (feofptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("feof"));

	if (feoffunc == nil)
		return (false);

	filenofunc = (filenoptr) CFBundleGetFunctionPointerForName (sysBundle, CFSTR ("fileno"));

	if (filenofunc == nil)
		return (false);
#endif

	unixshellcallinited = true;

	return (true);
	} /*unixshellcallinit*/


static boolean unixshellcallbackgroundtask (void) {

	/*
	2005-10-02 creedon: created, cribbed from OpenTransportNetEvents.c: fwsbackgroundtask
	*/

	boolean fl = true;

#ifndef FRONTIER_HEADLESS
	if (inmainthread ()) {
		EventRecord ev;
		EventMask mask = osMask|activMask|mDownMask|keyDownMask; // |highLevelEventMask|updateMask
		long sleepTime = 6;	// 1/10 of a second by default

		if (WaitNextEvent (mask, &ev, sleepTime, nil)) /* might return false to indicate a null event, but that's not an error */
			fl = shellprocessevent (&ev);
		}
	else
#endif
		fl = langbackgroundtask (true);

	return (fl);
	}/* unixshellcallbackgroundtask */


static boolean unixshellcall_read_stream (FILE *f, Handle houtput) {

	/*
	2025-12-27: Helper function to read from a file stream and accumulate data into a handle.
	Reads data from stream in non-blocking mode, calling background task while waiting.
	*/

	char buf [1024];
	long ct = 0;

	while (true) {

		ct = freadfunc (buf, 1, sizeof buf, f); /*fread*/

		if (ct > 0)
			if (!enlargehandle (houtput, ct, buf))
				return (false);

		if (feoffunc (f))
			break;

		if (!unixshellcallbackgroundtask ())
			return (false);

		}

	return (true);
	} /*unixshellcall_read_stream*/


boolean unixshellcall (Handle hcommand, Handle hreturn) {

	/*
	2006-01-29 creedon: change buf from 32 to 1024 to increase performance of function,
		we can do this now because fread is now in non-blocking mode, kernel remains repsonsive

	2006-01-10 creedon: laid down the groundwork for a timeoutsecs parameter, what is a good default?
		longinfinity? several minutes? I know that some commands I've done have taken 15 - 30 minutes
		fread now reads in non-blocking mode, no error checking

	2005-10-02 creedon: changed buf size from 256 to 32, makes envrionment more responsive
		added call to new unixshellcallbackgroundtask function so kernel doesn't lock up
		while waiting for a lot of data to be read

	7.0b51: Call the UNIX popen command, which evaluates a string as if it were typed
		on the command line. Verb: sys.unixShellCommand.
		Code adapted by Timothy Paustian from Apple sample code.
		This routine by PBS.
	*/

	FILE *f;

	if (!unixshellcallinit ())
		return (false);

	if (!enlargehandle (hcommand, 1, "\0"))
		return (false);

	lockhandle (hcommand);

	f = popenfunc (*hcommand, "r"); /*popen*/

	unlockhandle (hcommand);

	if (f == nil) {
		log_error(LOG_COMP_GENERAL, "Failed to execute command via popen");
		return (false);
	}

	fcntlfunc (filenofunc (f), F_SETFL, fcntlfunc (filenofunc (f), F_GETFL, 0) | O_NONBLOCK);

	if (!unixshellcall_read_stream (f, hreturn)) {
		log_error(LOG_COMP_GENERAL, "Failed to read stdout from command");
		pclosefunc (f);
		return (false);
	}

	pclosefunc (f); /*pclose*/

	return (true);
	} /*unixshellcall*/


boolean unixshellcall_separatestderr (Handle hcommand, Handle hstdout, Handle hstderr, int *exit_status) {

	/*
	2025-12-27: Enhanced version that captures both stdout and stderr to separate handles.
	Uses shell redirection to send stderr to a temporary file, then reads both streams.

	Strategy: Run "cmd 2>tmpfile" to separate streams, then read stdout from pipe
	and stderr from the temp file. Uses dynamic allocation for large commands and
	fdopen() to avoid TOCTOU race condition.

	If exit_status is not NULL, stores the exit status of the command there.
	*/

	FILE *f;
	char *cmd_with_redirect;
	long cmd_len;
	char tmpfile_template[TMPFILE_PATH_MAX];
	int tmpfd;
	FILE *stderr_file;
	boolean fl = true;

	/* Use P_tmpdir for portable temporary directory. Falls back to /tmp if not defined. */
	#ifndef P_tmpdir
	#define P_tmpdir "/tmp"
	#endif
	snprintf(tmpfile_template, sizeof(tmpfile_template), "%s/frontier_stderr_XXXXXX", P_tmpdir);

	if (!unixshellcallinit ())
		return (false);

	if (!enlargehandle (hcommand, 1, "\0"))
		return (false);

	lockhandle (hcommand);

	/* Calculate space needed for command + redirection + null terminator */
	cmd_len = gethandlesize (hcommand) + strlen (" 2>") + strlen (tmpfile_template) + 1;

	if (cmd_len > MAX_SHELL_COMMAND_LENGTH) {
		log_error(LOG_COMP_GENERAL, "Command too long for shell execution (%ld bytes)", cmd_len);
		unlockhandle (hcommand);
		return (false);
	}

	/* Create a temporary file for stderr */
	tmpfd = mkstemp (tmpfile_template);

	if (tmpfd < 0) {
		log_error(LOG_COMP_GENERAL, "Failed to create temporary file for stderr capture");
		unlockhandle (hcommand);
		return (false);
	}

	/* Allocate memory for command with redirection */
	cmd_with_redirect = (char *) malloc (cmd_len);
	if (cmd_with_redirect == nil) {
		log_error(LOG_COMP_GENERAL, "Failed to allocate memory for command string");
		unlockhandle (hcommand);
		close (tmpfd);
		unlink (tmpfile_template);
		return (false);
	}

	/* Build command with stderr redirection */
	snprintf (cmd_with_redirect, cmd_len, "%s 2>%s", *hcommand, tmpfile_template);

	unlockhandle (hcommand);

	/* Open stdout pipe */
	f = popenfunc (cmd_with_redirect, "r");

	if (f == nil) {
		log_error(LOG_COMP_GENERAL, "Failed to execute command: %s", cmd_with_redirect);
		close (tmpfd);
		unlink (tmpfile_template);
		free (cmd_with_redirect);
		return (false);
	}

	fcntlfunc (filenofunc (f), F_SETFL, fcntlfunc (filenofunc (f), F_GETFL, 0) | O_NONBLOCK);

	/* Read stdout */
	if (!unixshellcall_read_stream (f, hstdout)) {
		log_error(LOG_COMP_GENERAL, "Failed to read stdout");
		pclosefunc (f);
		close (tmpfd);
		unlink (tmpfile_template);
		free (cmd_with_redirect);
		return (false);
	}

	/* Capture pclose() return value and extract exit status using WEXITSTATUS macro.
	   Per POSIX spec, must check WIFEXITED() before WEXITSTATUS(). */
	int pclose_status = pclosefunc (f);
	int cmd_exit_status = 0;

	if (WIFEXITED(pclose_status)) {
		cmd_exit_status = WEXITSTATUS(pclose_status);
	} else if (WIFSIGNALED(pclose_status)) {
		/* Process was killed by signal - return 128 + signal number per convention */
		cmd_exit_status = 128 + WTERMSIG(pclose_status);
	} else {
		/* Unexpected status - return -1 to indicate error */
		cmd_exit_status = -1;
	}

	if (exit_status != NULL)
		*exit_status = cmd_exit_status;

	/* CRITICAL FIX: Seek to beginning of stderr temp file before fdopen.
	   The pclose() above may have flushed remaining data to the temp file,
	   but we need to ensure the file descriptor is positioned at the start
	   before we read. Without this lseek(), we may read partial/truncated stderr.
	   This prevents a subtle race condition where the file exists but fd position
	   is not at the beginning. */
	if (lseek(tmpfd, 0, SEEK_SET) < 0) {
		log_error(LOG_COMP_GENERAL, "Failed to seek to beginning of stderr temp file");
		close(tmpfd);
		unlink(tmpfile_template);
		free(cmd_with_redirect);
		return (false);
	}

	/* Read stderr from temp file - use fdopen to avoid TOCTOU race condition.
	   Instead of close(tmpfd) then fopen(tmpfile_template), we use fdopen() to directly
	   consume the file descriptor. This prevents a window where another process could
	   create a file with the same name before we open it. */
	stderr_file = fdopen (tmpfd, "r");
	if (stderr_file != nil) {
		/* fdopen succeeded - fd is now owned by FILE* stream, do not close separately */
		fcntlfunc (filenofunc (stderr_file), F_SETFL, fcntlfunc (filenofunc (stderr_file), F_GETFL, 0) | O_NONBLOCK);

		if (!unixshellcall_read_stream (stderr_file, hstderr)) {
			log_error(LOG_COMP_GENERAL, "Failed to read stderr");
			fl = false;
		}

		fclose (stderr_file); /* Closes the underlying fd that was transferred from tmpfd */
	}
	else {
		/* fdopen failed - fd still owned by us, must close it explicitly */
		log_error(LOG_COMP_GENERAL, "Failed to open stderr temp file with fdopen");
		close (tmpfd); /* Close fd since fdopen didn't take ownership */
		fl = false;
	}

	/* Clean up temp file and memory */
	unlink (tmpfile_template);
	free (cmd_with_redirect);

	return (fl);
	} /*unixshellcall_separatestderr*/
