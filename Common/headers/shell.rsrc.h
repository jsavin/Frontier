
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

#define shellrsrcinclude


#define defaultlistnumber 129 /*resource number for program defaults STR# resource*/
#define programname 1
#define untitledfilename 2
#define defaultfilename 3
#define startupfileprompt 4
#define preferencesfilename 5
#define startupfilename 6
#define nonstartupfileprompt 7

#define interfacelistnumber 130 /*misc strings that appear in UI of program*/

	#define scriptbuttonstring 1
	#define zoombuttonstring 2
	#define trychooserstring 3
	#define cmdkeypopupstring 4
	#define setcmdkeyitemstring 5
	#define cmdkeypromptstring 6
	#define internalerrorstring 7
	#define timedateseperatorstring 8
	#define cancelbuttonstring 9
	#define scriptforstring 10
	#define customsizestring 11
	#define kilobytestring 12
	#define customleadingstring 13
	#define commastring 14
	#define outofmemorystring 15
	#define saveitemstring 16
	#define savedatabaseitemstring 17
	#define saveasitemstring 18
	#define saveacopyitemstring 19
	#define openolddatabasestring 20


#define langerrorlistnumber 131 /*strings used by langerror.c, the Error Info window*/
#define scripticonstring 1
#define langerrortitlestring 2
#define errorlocationstring 3

#define fontnamelistnumber 132 /*font names stored in a STR# list*/

#define commandlistnumber 133 /*strings used by command.c, the QuickScript window*/
#define runiconstring 1
#define commandtitlestring 2

#define undolistnumber 134 /*operations that can be undone, in string format*/
#define cantundoitem 1
#define undostring 2
#define redostring 3
#define undocutstring 4
#define undocopystring 5
#define undopastestring 6
#define undoclearstring 7
#define undotypingstring 8
#define undomovestring 9
#define undosortstring 10
#define undopromotestring 11
#define undodemotestring 12
#define undodeletionstring 13
#define undoformatstring 14

#define directionlistnumber 135 /*the strings for up, down, left, right, etc.*/

#define alertstringlistnumber 141 /*strings that go into alerts*/
#define baddatabaseversionstring 1
	#define itemnametoolongstring 2
	#define itemnameinusestring 3
	#define reopenerrorstring 4
	#define cantpasteherestring 5
#define notenoughmemorystring 6
#define archaicsystemstring 7
#define needthreadmanagerstring 8

#define specialfolderlistnumber 142

#define idvertbar 256 /*standard horizontal/vertical scrollbars*/
#define idhorizbar 257

#define idvertbaroid 258 /*"windoid" versions of horizontal/vertical scrollbars*/
#define idhorizbaroid 259



