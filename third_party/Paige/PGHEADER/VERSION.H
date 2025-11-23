/************************* Version control info **********************/

/* This file generates no code and defines no functions or constants.
The purpose of this header is to broadcast the official version of PAIGE.
For example, if you are a PAIGE user and want to know what version your
header files correspond to, open this file to find out.  */


/***********************************************************************

RELEASE VERSION:			2.0gm
RELEASE DATE:				June 19, 1996
LAST MOD IN OBJECT LIBS:	June 19, 1996	10:00 AM
HEXIDECIMAL FILE VERSION (in CPUDEFS.H): 0x00020000

Source code syncing info:

Last DSI/OITC syncronization:	February 21, 1996 11:49 PM PDT ÑÊMacintosh/Windows
Last OITC/DSI syncronization:	February 22, 1996 11:32 PM EST ÑÊMacintosh/Windows

************************************************************************/

/*

Summary Of Latest OITC changes:

Changes for May 18, 1996 by OITC

paige.h - added new funcs from pgstlsht.c and corrected typing
pgstlsht.c - added new funcs
pgieutil.c - compatibility with pgtxr.h
pgexport.cpp - corrected typing
pgimport.cpp - corrected typing
pgtxr.h - corrected typing
pgread.c - corrected typing
pgwrite.c - corrected typing
pgwindef.h - corrected defines

---------------------------------------
Changes for April 1, 1996 by DSI

VERSION 1.5

---------------------------------------
Summary Of February 21, 1996 OITC:

CPUDefs.h - Made cross plaform compiler compiler compliant. NOTE: Change WIN_DLL macro
	to MS_DLL to indicate the use of OLE on the Mac as well as windows.  There are other
	small changes on export/emport macros that are obvious when the header is viewed.
pgBreak.h - Supports Chinese character sets.
pgCntl.c - Minor change in ifdef
pgCntl.h - Fixed header error
pgFiles.h - Fixed struct to be cross compiler compliant
pgmemmgr.c - Fixed bad if's and missing typing
pfrtfdef.h - fixed spacing
pgtraps.h - reorganized
subrefmsg.c - removed unused variables
win16nls.h - Fixed for multilingual
Files.c - Added rtf, corrected prototypes and made cross compiler compliant
init.c - corrected prototypes and made cross compiler compliant
pgdemo.c - Added rtf, corrected prototypes and made cross compiler compliant
pgdemo.h - Corrected prototypes
pgdemo.rc - Added rtf ifdef
print.c - corrected prototypes
resource.h - corrected prototypes
Added OITC freebies
app.c - Fixed old NATIVE stuff and other minor debug changes
edit menu.c -  RTF changes
file menu.c - RTF changes

---------------------------------------
Changes for February 21, 1996 by DSI

VERSION 1.3.6 REVAMPED WINDOWS MEM MANAGER et al

---------------------------------------
Summary Of December 15, 1995 OITC:

file menu.c - got rid of NO_LF_BIT

RTF/Paige Demo.rsrc - updated for RTF resource changes

iomsg.c - added RTF

pgDemo.c - added RTF

pgDemo.rc - added RTF

pgRTF.rc - added RTF

pgWinDef.h - Made language support more robust

CPUDefs.h - More changes for Windows and Windows 16

pgLists.h - Added missing #define

pgScroll.c - Changed to suppress compiler warning

PGEMBED.C - corrected version bug

pgEdit.c - ifdef'd chandhok fix

W16NLS.H - New for 16 bit foriegn Windows

pgWin.c - Updates for foriegn Wind16

---------------------------------------
Changes for October 28, 1995 by OITC

CPUDefs.h - Some #defines and some 16 bit language

pgMTraps.h - Some 16 bit language

Machine.h - Moved some #defines to TDChrSet. Added idle proc.

pgMac.c - Minor cleanup and set NON_ROMAN_FLAG to allow compatible processing between windows and mac and minor language handling. Added idle proc. fixed global sizes

pgWin.c - Minor language handling. Added idle proc. Some 16 bit language. fixed hyphen and global sizes

pgEmbed.c/h - Use of variable before being initialized. Changed bookmark processing. Added horizontal rule. Also made SOM compaitble and safer.

pgGrafx.c - Added horizontal rule.

pgRead.c - Getting ready for unicode

pgWrite.c - Getting ready for unicode. Eliminated unused variables

PGSUBREFS.C - Removed unused variables.

pgPar.C - Removed unused variables.

pgText.C - Removed unused variables.

pgClipBd.c - Minor formatting.

pgUtils.c - Minor formatting.

pgTxtUtl.c - Minor formatting.

pgOSUtl.c/h - Minor formatting and made pgCStrLength return long.

Paige.h - Minor formatting and idle proc. Also made SOM compaitble and safer.

pgExceps.h - Added typing (you missed this one before)

defProcs.h - Added idle proc.

pgInit.c - Added idle proc.

pgMemMgr.h - Corrected ifdefs to remove debug code.

pgSelect.c - Added idle proc.

pgBreak.c/h - new and language changes

pgLists.c/h - new

pgBasic.c - Added idle processing

pgDebug.c - Added ifdef processing

pgEdit.c, pgGrafx.h, pgMemMgr.h, pgShapes.c/h, pgSubRef.c/h, PGDEFSTL.h, pgText.h, pgTraps.h, pgWinDef.h, userdefs.h,  -  Made SOM compaitble and safer.

pgfiles.h -  Also made SOM compaitble and safer.

pgscroll.c -  Fixed bad typing.

??files.h -  Also made SOM compaitble and safer.

app.c - added blink test

pg pict.c - enabled pgEmbed insert

pict layout.c - reenabled insert options

RTF/Paige Demo.rsrc - updated resources

subrefmsg.c - Made compatible with CW 7 cross compiler

print.c,init.c - Made compatible with CW 7 cross compiler

files.c - Made compatible with CW 7 cross compiler and added RTF

pgdemo.c/h - Made compatible with CW 7 cross compiler. added rtf

pgcntl.h/c - Made more compatible with Paige

pgcntlutl.h - Made more compatible with Paige

embedmsg.c - Made more compatible with Paige

docmsg.c - Fixed typing problems

usermsg.c - Missing include

PGDEMO.RC, Resource.h - RTF additions

PGRFT.RC - RTF resources (not yet up to date)

cntldll.c - Made more compatible with Paige

editmsg.c - Made more compatible with Paige

iomsg.c - Made more compatible with Paige

stylemsg.c - Made more compatible with Paige

pgRsrc.h/c - moved folder locations

pgBreak.h - moved folder locations

pgLists.h/c - moved folder locations

pgHdrFtr.h/c - moved folder locations

---------------------------------------
Changes for October 12, 1995 by Gar

Misc. bug fixes

---------------------------------------
Changes for October 3, 1995 by Tom

Paige.h - Correct some calling sequences and added an init handler code and flags

CPUDefs.h - Fix for unix (future) and others

pgStlsht.c - Correct some calling sequences

pgIEUtil.c - added insert debug check

PGWIN.C - Was setting right/left flag improperly and wiping out script

PGREAD.C - Added an init handler to correct documents created on other platforms.

PGWRITE.C - Corrected to properly define font record on output.

PGERRORS.H - Add errors

PGDFLSTL.C - Corrected a proc call.

---------------------------------------
Summary September 22, 1995 by Tom:

CPUDEFS.H - Added PG_DLL for mac and win emulate

PAIGE.H - Corrected resolution defintions

PGWINDEF.H - Added some more typedef's that RTF needed

PGSHARES.H - Added #define for new shape as I can never the size I should put for memory alloc

PGMAC.C - fixed resolution also corrected language

PGREAD.C - fixed input bug, added other doc_info items for RTF and fixed language for backward compatibility assuming you haven't shipped with KEY_REVISION11 yet.

PGWRITE.C - fixed output bug for doc_info and added other doc_info items for RTF.

PGFILES.H - added some comments

MACHINE.H - Removed certain Mac region #defines as part of language fix

PGHLEVEL.C - fixed resolution

PGDEFSTL.C - removed unused variables

Subrefs.h/c - Yes I know. These changes are not "bugs" I just cleaned up for better compiles

---------------------------------------
Changes by Gar as of Sept 21 1995

Changed old Script.h names to newer names in pgMac.c for better compatibility with Universal Headers

Fixed bug for styles being applied to "blank" lines that would incorrectly assume the style
of the previous CR.

Cleaned up subref alignment and pagination bugs.

Added debugging info to memmgr to more easily detect culprits for bad access counters.

Fixed missing PG_FAR for pgFillBlock().

Minor changes/fixes for miscellaneous things reported.


---------------------------------------
Changes by Tom as of Sept 22 1995

Paige.h - Import API changes. Added return value to pgInsertText and pgEndImport

machine.h - Move font language dependancies to RTF.

pgEmbed.h - Added import only call, pgInsertEmbed.

pgIEUtil.c - added emulation for pgInsertText, pgBeginImport, pgEndImport, pgInsertEmbed

PGWIN.C - Fixed weird text

PGREAD.C - Fixed lanuage fix

PGTXTUTL.C -  Formatting only

PGSUBREFS.C - Removed unused variables.

Mac Demo:

app.c/h - Updated for import

file menu.c - Updated for RTF and 1.27

edit menu.c - Updated for RTF and 1.27

ruler.c - Updated for import

taskmaster.c - Updated for import

RTF/Paige Demo.rsrc - Updated for structure alignment

Changes by Gar as of 10/12/95

Reverted to some older #defines for Windows DLL in cpudefs.h.

Added undo_app_insert to let app define position for inserting when preparing Undo.

NO_DEFAULT_LEADING flag added to keep Paige from adding font leading to lines

Various bug fixes reported since last source file merge.

Lots of changes to DSI_SUBSETS


*/



