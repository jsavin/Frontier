
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

/*
	2004-10-23 aradke: From now on, add all system includes to this file.
		It is included from frontier.h, which is included in all source files.
*/

#ifndef __osincludes_h__
#define __osincludes_h__





	#define OTDEBUG	1	/* must define before including OpenTransport headers */


		#ifdef FRONTIER_FLAT_HEADERS /* building for Mach-O with flat header-style includes */

			#include <Carbon.h>
			#include <ApplicationServices.h>
			#include <Movies.h>

		#else
		
		#include <Carbon/Carbon.h>
		#include <ApplicationServices/ApplicationServices.h>
		// QuickTime removed - feature disabled in modern version

		#endif

		#define	ELASTERRNO	ELAST

		/* cribbed from pre-OSX StandardFile.h */
		struct StandardFileReply {
		  Boolean             sfGood;
		  Boolean             sfReplacing;
		  OSType              sfType;
		  FSSpec              sfFile;
		  ScriptCode          sfScript;
		  short               sfFlags;
		  Boolean             sfIsFolder;
		  Boolean             sfIsVolume;
		  long                sfReserved1;
		  short               sfReserved2;
		};
		typedef struct StandardFileReply        StandardFileReply;


	/* cribbed from pre-OSX AppleTalk.h -- for TargetID */
	struct EntityName {
	  Str32Field          objStr;
	  Str32Field          typeStr;
	  Str32Field          zoneStr;
	};
	typedef struct EntityName               EntityName;

	/* cribbed from pre-OSX PPCToolbox.h -- for TargetID */
	typedef SInt16 PPCLocationKind;
	typedef SInt16 PPCPortKinds;
	enum {
	  ppcByCreatorAndType           = 1,    /* Port type is specified as colloquial Mac creator and type */
	  ppcByString                   = 2     /* Port type is in pascal string format */
	};
	typedef SInt16 PPCXTIAddressType;
	struct PPCXTIAddress {
	  PPCXTIAddressType   fAddressType;           /* A constant specifying what kind of network address this is */
	  UInt8               fAddress[96];           /* The contents of the network address (variable length, NULL terminated). */
	};
	typedef struct PPCXTIAddress            PPCXTIAddress;
	typedef PPCXTIAddress *                 PPCXTIAddressPtr;
	struct PPCAddrRec {
	  UInt8               Reserved[3];            /* reserved - must be initialize to 0          */
	  UInt8               xtiAddrLen;             /* size of the xtiAddr field             */
	  PPCXTIAddress       xtiAddr;                /* the transport-independent network address   */
	};
	typedef struct PPCAddrRec               PPCAddrRec;
	typedef PPCAddrRec *                    PPCAddrRecPtr;
	struct LocationNameRec {
	  PPCLocationKind     locationKindSelector;   /* which variant */
	  union {
	    EntityName          nbpEntity;            /* NBP name entity                   */
	    Str32               nbpType;              /* just the NBP type string, for PPCOpen  */
	    PPCAddrRec          xtiType;              /* an XTI-type network address record     */
	  }                       u;
	};
	typedef struct LocationNameRec          LocationNameRec;
	typedef LocationNameRec *               LocationNamePtr;
	struct PPCPortRec {
	  ScriptCode          nameScript;             /* script of name */
	  Str32Field          name;                   /* name of port as seen in browser */
	  PPCPortKinds        portKindSelector;       /* which variant */
	  union {
	    Str32               portTypeStr;          /* pascal type string */
	    struct {
		 OSType              portCreator;
		 OSType              portType;
	    }                       port;
	  }                       u;
	};
	typedef struct PPCPortRec               PPCPortRec;
	typedef PPCPortRec *                    PPCPortPtr;
	struct PortInfoRec {
	  SInt8               filler1;
	  Boolean             authRequired;
	  PPCPortRec          name;
	};
	typedef struct PortInfoRec              PortInfoRec;
	typedef PortInfoRec *                   PortInfoPtr;

	/* cribbed from pre-OSX EPPC.h -- for TargetID */
	struct TargetID {
	  long                sessionID;
	  PPCPortRec          name;
	  LocationNameRec     location;
	  PPCPortRec          recvrName;
	};
	typedef struct TargetID                 TargetID;
	typedef TargetID *                      TargetIDPtr;
	typedef TargetIDPtr *                   TargetIDHandle;
	typedef TargetIDHandle                  TargetIDHdl;
	typedef TargetID                        SenderID;
	typedef SenderID *                      SenderIDPtr;


	#define topLeft(r)	(((Point *) &(r))[0])
	#define botRight(r)	(((Point *) &(r))[1])


/* common standard C headers */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#endif /* __osincludes_h__ */
