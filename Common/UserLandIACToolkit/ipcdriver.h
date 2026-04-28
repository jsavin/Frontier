
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

/* file:	IPCDriver.h
-- desc:	Public header file for IPCDriver
-- version:	V1.0
-- by:		Don Park
-- when:	November 1988
-- hist:	30Nov88 V1.0 DDP	New.
*/

#ifdef GLOBALSOK
#define	_IPCDrvrName		"\p.IPC Manager © 1988 UserLand"
#define	_IPCFileName		"\pIPC Manager"
#endif

/* Control selector codes understood by IPC driver */
enum {
	csOpnIPCPrc = 256,	/* register a named Process */
	csClsIPCPrc,		/* unregister a Process */
	csFndIPCPrc,		/* find a named Process */
	csWhoIPCPrc,		/* return name of a Process */
	csSndIPCMsg,		/* send a Message from a Process to another Process */
	csRcvIPCMsg,		/* receive a Message from any Process to a Process */
	csClrIPCMsg			/* flush all Messages sent to a Process */
};

/* Structure passed through csParam field of CntrlParam block when
-- a Control code is sent.
*/
typedef struct	{		/* Process Parameter Block */
	PIN*	pin;		/* Process Name */
	PID		pid;		/* Process ID */
	PID		sender;		/* Sender Process ID */
	PID		receiver;	/* Receiver Process ID */
	Handle	message;	/* Message handle */
} _IPCParam;

