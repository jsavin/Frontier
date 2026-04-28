
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

#include "error.h"
#include "strings.h"
#include "sounds.h"

#define squareWaveSynth 1
#define ampCmd 43
#define freqDurationCmd 40

#if oldsounds

static struct {

    short mode;
    
    Tone triplets [2];
    
    } myTone = {swMode, 0};
    
static IOParam pb = {0, 0};



boolean dosound (short duration, short amplitude, short frequency) {
	
	/*
	duration is in 60ths of a second.
	
	amplitude can range between 0 and 255, it determines the volume.
	
	10/12/91 dmb: limit amplitude to 255 explicity here
	*/

	OSErr errcode;

    myTone.triplets [0].count = (short) (7833600 / frequency);
    
    myTone.triplets [0].amplitude = min (amplitude, 255);
    
    myTone.triplets [0].duration = duration;

    pb.ioRefNum = -4;
    
    pb.ioBuffer = (Ptr) &myTone;
    
    pb.ioReqCount = (long) sizeof (myTone);

    errcode = PBWrite ((ParmBlkPtr) &pb, false);
    
    return (errcode == noErr);
	} /*dosound*/

#else

boolean dosound (short duration, short amplitude, short frequency) {
	
	//#if TARGET_API_MAC_CARBON
	//sysbeep();
	//return true;
	//#else

	SndChannelPtr channel;
	SndCommand cmd;
	long ltime;
	float note;
	OSErr err;
	
	channel = nil;
	
	err = SndNewChannel (&channel, squareWaveSynth, 0, nil);
	
	if (oserror (err))
		return (false);
	
	cmd.cmd = ampCmd;
	cmd.param1 = amplitude;
	cmd.param2 = 0;
	err = SndDoCommand (channel, &cmd, false);
	
	ltime = duration * (2000/60);
	
	ltime = min (infinity, ltime);
	
	note = (frequency / 129.0) / 69;
	
	note = 69 + 12.0 * (note - 1);
	
	cmd.cmd = freqDurationCmd;
	cmd.param1 = ltime;
	cmd.param2 = note;
	err = SndDoCommand (channel, &cmd, false);
	
	/*
	cmd.cmd = noteCmd;
	cmd.param1 = 30;
	cmd.param2 = $FF000000 + 83;
	err = SndDoCommand (channel, &cmd, false);
	
	
	cmd.cmd = quietCmd;
	cmd.param1 = 0;
	cmd.param2 = 0;
	err = SndDoCommand (channel, &cmd, false);
	*/
	
	err = SndDisposeChannel (channel, false);
	
	return (!oserror (err));
//#endif

	} /*dosound*/

#endif


void motorsound (void) {
	
	dosound (1, 100, 100);
	} /*motorsound*/


void ouch (void) {
	
	sysbeep (); 

#ifdef WIN95VERSIOON
	Beep(1000, 500);
#endif
	
	//dosound (1 /*duration*/, 250 /*amplitude*/, 14300 /*frequency*/);
	
	} /*ouch*/


boolean playnamedsound (bigstring bsname) {

	Handle hsound;
	
	hsound = GetNamedResource ('snd ', bsname);
	
	if (hsound == nil)
		return (false);
	
	return (SndPlay (nil, (SndListHandle) hsound, false) == noErr);


	} /*playnamedsound*/



