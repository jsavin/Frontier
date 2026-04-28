
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

/*
scrap.c -- routines that support use of the desk scrap.
*/

#include "frontier.h"
#include "standard.h"

#include "shelltypes.h"
#include "scrap.h"
#include "error.h"
#include "memory.h"
#include "threads.h"




boolean resetscrap (void) {
	//Code change by Timothy Paustian Sunday, June 25, 2000 11:05:43 AM
	//Carbon Support
		return (ClearCurrentScrap() == noErr);

	} /*resetscrap*/


short getscrapcount (void) {

	/*
	2002-10-13 AR: Commented out variable declaration for theCount
	to eliminate compiler warning about an unused variable.
	*/
	
		//Code change by Timothy Paustian Sunday, June 25, 2000 11:05:56 AM
		//Carbon Support
		
			ScrapRef 	theRef;
			//UInt32	 	theCount;
			static short theShortCount = 1;
			OSStatus	status;
			static ScrapRef lastScrapRef = nil;
			
			status = GetCurrentScrap(&theRef);
			
			if(status != noErr)
				return 0;
			
			assert(theRef != nil);
			
			if (theRef != lastScrapRef)
				theShortCount++;
				
			lastScrapRef = theRef;
			
			return (theShortCount);
			
			//status = GetScrapFlavorCount(theRef, &theCount);
			//if(status != noErr)
			//	return 0;
			
			//restrict the return value to a short size
			//theShortCount = min(theCount, 32000);
			//return theShortCount;
			
		

	} /*getscrapcount*/




tyscraptype getscraptype (void) {
	
	/*
	return the type of the first object on the scrap.
	*/
	
		//Code change by Timothy Paustian Sunday, June 25, 2000 11:06:10 AM
		//Carbon support for new Scrap Manager API
		ScrapRef		scrap;
		UInt32			infoNumber = 1;
    	ScrapFlavorInfo info[1];
    	OSStatus		status;
    	
    	status = GetCurrentScrap(&scrap);
    	if(status != noErr)
			return 0;
		//now get the first item on the list
		status = GetScrapFlavorInfoList(scrap, &infoNumber, info);
		if(status != noErr)
			return 0;
		
		return (tyscraptype)info[1].flavorType;
		

	} /*getscraptype*/


boolean getscrap (tyscraptype scraptype, Handle hscrap) {
	
	/*
	return true if something was loaded from the scrap, false 
	otherwise.
	
	12/26/91 dmb: return true if scraptype exists; generate error alert on real error.

	5.0a16 dmb: need to scan more than last 32 bytes for NUL char
	*/
	
		ScrapRef			theScrap;
	    ScrapFlavorType 	flavorType = (ScrapFlavorType)scraptype;
	    Size 				byteCount;
	    OSStatus			status;
	    ScrapFlavorFlags	flavorFlags;
		boolean				retVal = true;
		Size				prevCount;

	    status = GetCurrentScrap(&theScrap);
	    if(status != noErr)
			oserror (status);

		status = GetScrapFlavorFlags(theScrap, flavorType, &flavorFlags);
		if(status == noTypeErr)
		{
			retVal = false;
		}
		else if(status < 0)
		{
			retVal = false;
			oserror (status);
		}
		//if we get here and the handles nil, we really don't want the data
		//also if retVal is false, it means the type does not exist
		if(hscrap == nil || !retVal)
			return retVal;

		status = GetScrapFlavorSize(theScrap, flavorType, &byteCount);
		if(status != noErr)
			oserror (status);
		prevCount = byteCount;
		SetHandleSize(hscrap, byteCount);
		if (MemError () != noErr)
			return (false);
		//lock the handle before getting the data
		HLock(hscrap);
	    status = GetScrapFlavorData(theScrap, flavorType, &byteCount, *(hscrap));
	    HUnlock(hscrap);

	    if(status != noErr)
			oserror (status);
		//only return true if we got all the data.
		return (byteCount == prevCount);

		
	
	} /*getscrap*/


boolean putscrap (tyscraptype scraptype, Handle hscrap) {
	
	/*
	return true if something was put on the scrap, false 
	otherwise.

	5.0a16 dmb: handle NULL scrap for delayed rendering

	5.0b13 dmb: report errors (win)
	*/

	boolean fl;

		//Code change by Timothy Paustian Sunday, June 25, 2000 11:28:22 AM
		//New scrap code. We don't need flavorFlags I think
		ScrapRef			theScrap;
		OSStatus			status;
	    ScrapFlavorType 	flavorType = (ScrapFlavorType) scraptype;
	    ScrapFlavorFlags 	flavorFlags = kScrapFlavorMaskNone;
		Size				flavorSize = GetHandleSize(hscrap);
		
		status = GetCurrentScrap(&theScrap);
		if(status != noErr)
			return false;
		HLock(hscrap);
		fl = (PutScrapFlavor (theScrap, flavorType, flavorFlags, flavorSize, *hscrap) == noErr);
    	HUnlock(hscrap);
    	return fl;
    	
		

	} /*putscrap*/


boolean openclipboard (void) {

		return (true);

	} /*openclipboard*/


boolean closeclipboard (void) {

		return (true);

	} /*closeclipboard*/


void initclipboard (void) {

	} /*initclipboard*/







