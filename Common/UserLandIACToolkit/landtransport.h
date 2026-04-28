
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

/* file:	ipc.h
-- desc:	Public header file for IPCLib
-- version:	V1.0
-- by:		Don Park
-- when:	November 1988
-- hist:	30Nov88 V1.0 DDP	New.
*/

typedef Str255	PIN;	/* Process Name */
typedef	short	PID;	/* Process ID */

/* IPC specific error codes:
-- ipcErrBase:		Start of IPC error code range (expected to be negative).
-- ipcNSProcErr:	Specified Process (either by PID or PIN) is currently NOT
--					open.
-- ipcFullErr:		Too many IPC processes are open.
-- ipcBadPIDErr:	Given PID is NOT a valid PID.
-- ipcBadPINErr:	Given PIN is NOT a valid PIN.
-- ipcBadMsgErr:	Given Message is NOT a valid Message.
*/
#define	ipcErrBase		-2000
#define	ipcNSProcErr	(ipcErrBase - 0)
#define	ipcFullErr		(ipcErrBase - 1)
#define	ipcBadPIDErr	(ipcErrBase - 2)
#define	ipcBadPINErr	(ipcErrBase - 3)
#define	ipcBadMsgErr	(ipcErrBase - 4)

extern OSErr	ipcOpen			( PIN*, PID* );
extern OSErr	ipcClose		( PID );
extern OSErr	ipcFind			( PIN*, PID* );
extern OSErr	ipcWho			( PID, PIN* );

extern OSErr	ipcSend			( PID, PID, Handle );
extern OSErr	ipcReceive		( PID, PID*, Handle* );
extern OSErr	ipcClear		( PID );

#ifdef	GLOBALSOK
extern OSErr	ipcError		( void );
#endif
