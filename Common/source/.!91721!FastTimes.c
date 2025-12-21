/*	$Id$    */

/* File "FastTimes.c" - Original code by Matt Slot <fprefect@ambrosiasw.com>  */
/* Created 4/24/99    - This file is hereby placed in the public domain       */
/* Updated 5/21/99    - Calibrate to VIA, add TBR support, renamed functions  */
/* Updated 10/4/99    - Use AbsoluteToNanoseconds() in case Absolute = double */
/* Updated 2/15/00    - Check for native Time Manager, no need to calibrate   */
/* Updated 2/19/00    - Fixed default value for gScale under native Time Mgr  */
/* Updated 3/21/00    - Fixed ns conversion, create 2 different scale factors */
/* Updated 5/03/00    - Added copyright and placed into PD. No code changes   */
/* Updated 8/01/00    - Made "Carbon-compatible" by replacing LMGetTicks()    */
/* Updated 8/22/00    - Fixed optimizer bug, changed GENPPC macros, extern C  */

/* This file is Copyright (C) Matt Slot, 1999-2000. It is hereby placed into 
   the public domain. The author makes no warranty as to fitness or stability */

#include "frontier.h"
#include "standard.h"


	#include "CallMachOFrameWork.h"	/*2005-01-15 aradke*/

#include "FastTimes.h"

/* **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** */
/* **** **** **** **** **** **** **** **** **** **** **** **** **** **** **** */
/*
	On 680x0 machines, we just use Microseconds().
	
	On PowerPC machines, we try several methods:
	  * DriverServicesLib is available on all PCI PowerMacs, and perhaps
