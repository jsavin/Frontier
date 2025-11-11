# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=Paige32 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Paige32 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Paige32 - Win32 Release" && "$(CFG)" !=\
 "Paige32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "Paige32.mak" CFG="Paige32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Paige32 - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Paige32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "Paige32 - Win32 Debug"
RSC=rc.exe
MTL=mktyplib.exe
CPP=cl.exe

!IF  "$(CFG)" == "Paige32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "C:\Paige3.0"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=C:\Paige3.0
INTDIR=.\Release

ALL : "$(OUTDIR)\Paige32.dll"

CLEAN : 
	-@erase "$(INTDIR)\Pgbasics.obj"
	-@erase "$(INTDIR)\Pgboxes.obj"
	-@erase "$(INTDIR)\Pgclipbd.obj"
	-@erase "$(INTDIR)\Pgdebug.obj"
	-@erase "$(INTDIR)\Pgdefpar.obj"
	-@erase "$(INTDIR)\Pgdefstl.obj"
	-@erase "$(INTDIR)\Pgdll32.obj"
	-@erase "$(INTDIR)\Pgedit.obj"
	-@erase "$(INTDIR)\Pgembed.obj"
	-@erase "$(INTDIR)\Pgexceps.obj"
	-@erase "$(INTDIR)\Pgfiles.obj"
	-@erase "$(INTDIR)\Pgfonts.obj"
	-@erase "$(INTDIR)\Pggrafx.obj"
	-@erase "$(INTDIR)\Pghtext.obj"
	-@erase "$(INTDIR)\Pginit.obj"
	-@erase "$(INTDIR)\Pgio.obj"
	-@erase "$(INTDIR)\Pglines.obj"
	-@erase "$(INTDIR)\Pglists.obj"
	-@erase "$(INTDIR)\Pgmemmgr.obj"
	-@erase "$(INTDIR)\Pgmerge.obj"
	-@erase "$(INTDIR)\Pgnboxes.obj"
	-@erase "$(INTDIR)\Pgosutl.obj"
	-@erase "$(INTDIR)\Pgpar.obj"
	-@erase "$(INTDIR)\Pgprint.obj"
	-@erase "$(INTDIR)\Pgread.obj"
	-@erase "$(INTDIR)\Pgscrap.obj"
	-@erase "$(INTDIR)\Pgscript.obj"
	-@erase "$(INTDIR)\Pgscroll.obj"
	-@erase "$(INTDIR)\Pgselect.obj"
	-@erase "$(INTDIR)\Pgshapes.obj"
	-@erase "$(INTDIR)\Pgstlsht.obj"
	-@erase "$(INTDIR)\Pgstyles.obj"
	-@erase "$(INTDIR)\Pgsubref.obj"
	-@erase "$(INTDIR)\Pgtables.obj"
	-@erase "$(INTDIR)\Pgtabs.obj"
	-@erase "$(INTDIR)\Pgtext.obj"
	-@erase "$(INTDIR)\Pgtxr.obj"
	-@erase "$(INTDIR)\Pgtxtblk.obj"
	-@erase "$(INTDIR)\Pgtxtutl.obj"
	-@erase "$(INTDIR)\Pgtxtwid.obj"
	-@erase "$(INTDIR)\Pgutils.obj"
	-@erase "$(INTDIR)\Pgwin.obj"
	-@erase "$(INTDIR)\Pgwrite.obj"
	-@erase "$(OUTDIR)\Paige32.dll"
	-@erase "$(OUTDIR)\Paige32.exp"
	-@erase "$(OUTDIR)\Paige32.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O1 /I "C:\Paige3.0\PGHEADER" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /D "CREATE_MS_DLL" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O1 /I "C:\Paige3.0\PGHEADER" /D "NDEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /D "CREATE_MS_DLL"\
 /Fp"$(INTDIR)/Paige32.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Paige32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/Paige32.pdb" /machine:I386 /out:"$(OUTDIR)/Paige32.dll"\
 /implib:"$(OUTDIR)/Paige32.lib" 
LINK32_OBJS= \
	"$(INTDIR)\Pgbasics.obj" \
	"$(INTDIR)\Pgboxes.obj" \
	"$(INTDIR)\Pgclipbd.obj" \
	"$(INTDIR)\Pgdebug.obj" \
	"$(INTDIR)\Pgdefpar.obj" \
	"$(INTDIR)\Pgdefstl.obj" \
	"$(INTDIR)\Pgdll32.obj" \
	"$(INTDIR)\Pgedit.obj" \
	"$(INTDIR)\Pgembed.obj" \
	"$(INTDIR)\Pgexceps.obj" \
	"$(INTDIR)\Pgfiles.obj" \
	"$(INTDIR)\Pgfonts.obj" \
	"$(INTDIR)\Pggrafx.obj" \
	"$(INTDIR)\Pghtext.obj" \
	"$(INTDIR)\Pginit.obj" \
	"$(INTDIR)\Pgio.obj" \
	"$(INTDIR)\Pglines.obj" \
	"$(INTDIR)\Pglists.obj" \
	"$(INTDIR)\Pgmemmgr.obj" \
	"$(INTDIR)\Pgmerge.obj" \
	"$(INTDIR)\Pgnboxes.obj" \
	"$(INTDIR)\Pgosutl.obj" \
	"$(INTDIR)\Pgpar.obj" \
	"$(INTDIR)\Pgprint.obj" \
	"$(INTDIR)\Pgread.obj" \
	"$(INTDIR)\Pgscrap.obj" \
	"$(INTDIR)\Pgscript.obj" \
	"$(INTDIR)\Pgscroll.obj" \
	"$(INTDIR)\Pgselect.obj" \
	"$(INTDIR)\Pgshapes.obj" \
	"$(INTDIR)\Pgstlsht.obj" \
	"$(INTDIR)\Pgstyles.obj" \
	"$(INTDIR)\Pgsubref.obj" \
	"$(INTDIR)\Pgtables.obj" \
	"$(INTDIR)\Pgtabs.obj" \
	"$(INTDIR)\Pgtext.obj" \
	"$(INTDIR)\Pgtxr.obj" \
	"$(INTDIR)\Pgtxtblk.obj" \
	"$(INTDIR)\Pgtxtutl.obj" \
	"$(INTDIR)\Pgtxtwid.obj" \
	"$(INTDIR)\Pgutils.obj" \
	"$(INTDIR)\Pgwin.obj" \
	"$(INTDIR)\Pgwrite.obj"

"$(OUTDIR)\Paige32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Paige32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "C:\Paige3.0"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=C:\Paige3.0
INTDIR=.\Debug

ALL : "$(OUTDIR)\Paige32.dll"

CLEAN : 
	-@erase "$(INTDIR)\Pgbasics.obj"
	-@erase "$(INTDIR)\Pgboxes.obj"
	-@erase "$(INTDIR)\Pgclipbd.obj"
	-@erase "$(INTDIR)\Pgdebug.obj"
	-@erase "$(INTDIR)\Pgdefpar.obj"
	-@erase "$(INTDIR)\Pgdefstl.obj"
	-@erase "$(INTDIR)\Pgdll32.obj"
	-@erase "$(INTDIR)\Pgedit.obj"
	-@erase "$(INTDIR)\Pgembed.obj"
	-@erase "$(INTDIR)\Pgexceps.obj"
	-@erase "$(INTDIR)\Pgfiles.obj"
	-@erase "$(INTDIR)\Pgfonts.obj"
	-@erase "$(INTDIR)\Pggrafx.obj"
	-@erase "$(INTDIR)\Pghtext.obj"
	-@erase "$(INTDIR)\Pginit.obj"
	-@erase "$(INTDIR)\Pgio.obj"
	-@erase "$(INTDIR)\Pglines.obj"
	-@erase "$(INTDIR)\Pglists.obj"
	-@erase "$(INTDIR)\Pgmemmgr.obj"
	-@erase "$(INTDIR)\Pgmerge.obj"
	-@erase "$(INTDIR)\Pgnboxes.obj"
	-@erase "$(INTDIR)\Pgosutl.obj"
	-@erase "$(INTDIR)\Pgpar.obj"
	-@erase "$(INTDIR)\Pgprint.obj"
	-@erase "$(INTDIR)\Pgread.obj"
	-@erase "$(INTDIR)\Pgscrap.obj"
	-@erase "$(INTDIR)\Pgscript.obj"
	-@erase "$(INTDIR)\Pgscroll.obj"
	-@erase "$(INTDIR)\Pgselect.obj"
	-@erase "$(INTDIR)\Pgshapes.obj"
	-@erase "$(INTDIR)\Pgstlsht.obj"
	-@erase "$(INTDIR)\Pgstyles.obj"
	-@erase "$(INTDIR)\Pgsubref.obj"
	-@erase "$(INTDIR)\Pgtables.obj"
	-@erase "$(INTDIR)\Pgtabs.obj"
	-@erase "$(INTDIR)\Pgtext.obj"
	-@erase "$(INTDIR)\Pgtxr.obj"
	-@erase "$(INTDIR)\Pgtxtblk.obj"
	-@erase "$(INTDIR)\Pgtxtutl.obj"
	-@erase "$(INTDIR)\Pgtxtwid.obj"
	-@erase "$(INTDIR)\Pgutils.obj"
	-@erase "$(INTDIR)\Pgwin.obj"
	-@erase "$(INTDIR)\Pgwrite.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\Paige32.dll"
	-@erase "$(OUTDIR)\Paige32.exp"
	-@erase "$(OUTDIR)\Paige32.ilk"
	-@erase "$(OUTDIR)\Paige32.lib"
	-@erase "$(OUTDIR)\Paige32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "C:\Paige3.0\PGHEADER" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /D "CREATE_MS_DLL" /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "C:\Paige3.0\PGHEADER" /D "_DEBUG"\
 /D "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /D "CREATE_MS_DLL"\
 /Fp"$(INTDIR)/Paige32.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Paige32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/Paige32.pdb" /debug /machine:I386 /out:"$(OUTDIR)/Paige32.dll"\
 /implib:"$(OUTDIR)/Paige32.lib" 
LINK32_OBJS= \
	"$(INTDIR)\Pgbasics.obj" \
	"$(INTDIR)\Pgboxes.obj" \
	"$(INTDIR)\Pgclipbd.obj" \
	"$(INTDIR)\Pgdebug.obj" \
	"$(INTDIR)\Pgdefpar.obj" \
	"$(INTDIR)\Pgdefstl.obj" \
	"$(INTDIR)\Pgdll32.obj" \
	"$(INTDIR)\Pgedit.obj" \
	"$(INTDIR)\Pgembed.obj" \
	"$(INTDIR)\Pgexceps.obj" \
	"$(INTDIR)\Pgfiles.obj" \
	"$(INTDIR)\Pgfonts.obj" \
	"$(INTDIR)\Pggrafx.obj" \
	"$(INTDIR)\Pghtext.obj" \
	"$(INTDIR)\Pginit.obj" \
	"$(INTDIR)\Pgio.obj" \
	"$(INTDIR)\Pglines.obj" \
	"$(INTDIR)\Pglists.obj" \
	"$(INTDIR)\Pgmemmgr.obj" \
	"$(INTDIR)\Pgmerge.obj" \
	"$(INTDIR)\Pgnboxes.obj" \
	"$(INTDIR)\Pgosutl.obj" \
	"$(INTDIR)\Pgpar.obj" \
	"$(INTDIR)\Pgprint.obj" \
	"$(INTDIR)\Pgread.obj" \
	"$(INTDIR)\Pgscrap.obj" \
	"$(INTDIR)\Pgscript.obj" \
	"$(INTDIR)\Pgscroll.obj" \
	"$(INTDIR)\Pgselect.obj" \
	"$(INTDIR)\Pgshapes.obj" \
	"$(INTDIR)\Pgstlsht.obj" \
	"$(INTDIR)\Pgstyles.obj" \
	"$(INTDIR)\Pgsubref.obj" \
	"$(INTDIR)\Pgtables.obj" \
	"$(INTDIR)\Pgtabs.obj" \
	"$(INTDIR)\Pgtext.obj" \
	"$(INTDIR)\Pgtxr.obj" \
	"$(INTDIR)\Pgtxtblk.obj" \
	"$(INTDIR)\Pgtxtutl.obj" \
	"$(INTDIR)\Pgtxtwid.obj" \
	"$(INTDIR)\Pgutils.obj" \
	"$(INTDIR)\Pgwin.obj" \
	"$(INTDIR)\Pgwrite.obj"

"$(OUTDIR)\Paige32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "Paige32 - Win32 Release"
# Name "Paige32 - Win32 Debug"

!IF  "$(CFG)" == "Paige32 - Win32 Release"

!ELSEIF  "$(CFG)" == "Paige32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgdebug\Pgdebug.c
DEP_CPP_PGDEB=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	
NODEP_CPP_PGDEB=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgdebug.obj" : $(SOURCE) $(DEP_CPP_PGDEB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgwin.c
DEP_CPP_PGWIN=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgLists.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgRegion.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	"\Paige3.0\PGHEADER\w16nls.h"\
	
NODEP_CPP_PGWIN=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgwin.obj" : $(SOURCE) $(DEP_CPP_PGWIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgio.c
DEP_CPP_PGIO_=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgIO.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	
NODEP_CPP_PGIO_=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgio.obj" : $(SOURCE) $(DEP_CPP_PGIO_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgmemmgr.c
DEP_CPP_PGMEM=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgIO.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGMEM=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgmemmgr.obj" : $(SOURCE) $(DEP_CPP_PGMEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgosutl.c
DEP_CPP_PGOSU=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgIO.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgRegion.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGOSU=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgosutl.obj" : $(SOURCE) $(DEP_CPP_PGOSU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgscrap.c
DEP_CPP_PGSCR=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgscrap.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSCR=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgscrap.obj" : $(SOURCE) $(DEP_CPP_PGSCR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgdll32.c

"$(INTDIR)\Pgdll32.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgwrite.c
DEP_CPP_PGWRI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\PackDefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgBasics.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxr.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGWRI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgwrite.obj" : $(SOURCE) $(DEP_CPP_PGWRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgboxes.c
DEP_CPP_PGBOX=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGBOX=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgboxes.obj" : $(SOURCE) $(DEP_CPP_PGBOX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgclipbd.c
DEP_CPP_PGCLI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgBasics.h"\
	"\Paige3.0\PGHEADER\pgClipBd.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGCLI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgclipbd.obj" : $(SOURCE) $(DEP_CPP_PGCLI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgdefpar.c
DEP_CPP_PGDEF=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGDEF=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgdefpar.obj" : $(SOURCE) $(DEP_CPP_PGDEF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgdefstl.c
DEP_CPP_PGDEFS=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGDEFS=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgdefstl.obj" : $(SOURCE) $(DEP_CPP_PGDEFS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgedit.c
DEP_CPP_PGEDI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgStyles.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGEDI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgedit.obj" : $(SOURCE) $(DEP_CPP_PGEDI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgembed.c
DEP_CPP_PGEMB=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGEMB=\
	"..\PGHEADER\WinMac.h"\
	"..\Pgsource\pgGIF.h"\
	

"$(INTDIR)\Pgembed.obj" : $(SOURCE) $(DEP_CPP_PGEMB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgexceps.c
DEP_CPP_PGEXC=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	
NODEP_CPP_PGEXC=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgexceps.obj" : $(SOURCE) $(DEP_CPP_PGEXC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgfiles.c
DEP_CPP_PGFIL=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\PackDefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGFIL=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgfiles.obj" : $(SOURCE) $(DEP_CPP_PGFIL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgfonts.c
DEP_CPP_PGFON=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgStyles.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGFON=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgfonts.obj" : $(SOURCE) $(DEP_CPP_PGFON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pggrafx.c
DEP_CPP_PGGRA=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgRegion.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGGRA=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pggrafx.obj" : $(SOURCE) $(DEP_CPP_PGGRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pghtext.c
DEP_CPP_PGHTE=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGHTE=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pghtext.obj" : $(SOURCE) $(DEP_CPP_PGHTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pginit.c
DEP_CPP_PGINI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	
NODEP_CPP_PGINI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pginit.obj" : $(SOURCE) $(DEP_CPP_PGINI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pglines.c
DEP_CPP_PGLIN=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGLIN=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pglines.obj" : $(SOURCE) $(DEP_CPP_PGLIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pglists.c
DEP_CPP_PGLIS=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgLists.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGLIS=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pglists.obj" : $(SOURCE) $(DEP_CPP_PGLIS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgmerge.c
DEP_CPP_PGMER=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgBasics.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGMER=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgmerge.obj" : $(SOURCE) $(DEP_CPP_PGMER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgnboxes.c
DEP_CPP_PGNBO=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGNBO=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgnboxes.obj" : $(SOURCE) $(DEP_CPP_PGNBO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgpar.c
DEP_CPP_PGPAR=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGPAR=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgpar.obj" : $(SOURCE) $(DEP_CPP_PGPAR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgprint.c
DEP_CPP_PGPRI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGPRI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgprint.obj" : $(SOURCE) $(DEP_CPP_PGPRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgread.c
DEP_CPP_PGREA=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\PackDefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgBasics.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgIO.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxr.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGREA=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgread.obj" : $(SOURCE) $(DEP_CPP_PGREA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgscript.c
DEP_CPP_PGSCRI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSCRI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgscript.obj" : $(SOURCE) $(DEP_CPP_PGSCRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgscroll.c
DEP_CPP_PGSCRO=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgRegion.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSCRO=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgscroll.obj" : $(SOURCE) $(DEP_CPP_PGSCRO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgselect.c
DEP_CPP_PGSEL=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgLists.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSEL=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgselect.obj" : $(SOURCE) $(DEP_CPP_PGSEL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgshapes.c
DEP_CPP_PGSHA=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSHA=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgshapes.obj" : $(SOURCE) $(DEP_CPP_PGSHA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgstlsht.c
DEP_CPP_PGSTL=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgStyles.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	
NODEP_CPP_PGSTL=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgstlsht.obj" : $(SOURCE) $(DEP_CPP_PGSTL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgstyles.c
DEP_CPP_PGSTY=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgStyles.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSTY=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgstyles.obj" : $(SOURCE) $(DEP_CPP_PGSTY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgsubref.c
DEP_CPP_PGSUB=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgStyles.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGSUB=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgsubref.obj" : $(SOURCE) $(DEP_CPP_PGSUB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtables.c
DEP_CPP_PGTAB=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTAB=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtables.obj" : $(SOURCE) $(DEP_CPP_PGTAB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtabs.c
DEP_CPP_PGTABS=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgPar.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTABS=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtabs.obj" : $(SOURCE) $(DEP_CPP_PGTABS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtext.c
DEP_CPP_PGTEX=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTEX=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtext.obj" : $(SOURCE) $(DEP_CPP_PGTEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxr.c
DEP_CPP_PGTXR=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxr.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	
NODEP_CPP_PGTXR=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtxr.obj" : $(SOURCE) $(DEP_CPP_PGTXR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtblk.c
DEP_CPP_PGTXT=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgosutl.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTraps.h"\
	"\Paige3.0\PGHEADER\pgTxr.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTXT=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtxtblk.obj" : $(SOURCE) $(DEP_CPP_PGTXT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtutl.c
DEP_CPP_PGTXTU=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTXTU=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtxtutl.obj" : $(SOURCE) $(DEP_CPP_PGTXTU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtwid.c
DEP_CPP_PGTXTW=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgScript.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGTXTW=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgtxtwid.obj" : $(SOURCE) $(DEP_CPP_PGTXTW) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgutils.c
DEP_CPP_PGUTI=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGUTI=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgutils.obj" : $(SOURCE) $(DEP_CPP_PGUTI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgbasics.c
DEP_CPP_PGBAS=\
	"..\PGHEADER\CPUDefs.h"\
	"..\PGHEADER\pgSetJmp.h"\
	"..\PGHEADER\Userdefs.h"\
	"..\PGHEADER\win16jmp.h"\
	"\Paige3.0\PGHEADER\DefProcs.h"\
	"\Paige3.0\PGHEADER\machine.h"\
	"\Paige3.0\PGHEADER\Paige.h"\
	"\Paige3.0\PGHEADER\pgBasics.h"\
	"\Paige3.0\PGHEADER\pgDefPar.h"\
	"\Paige3.0\PGHEADER\pgDefstl.h"\
	"\Paige3.0\PGHEADER\pgEdit.h"\
	"\Paige3.0\PGHEADER\pgEmbed.h"\
	"\Paige3.0\PGHEADER\pgErrors.h"\
	"\Paige3.0\PGHEADER\pgExceps.h"\
	"\Paige3.0\PGHEADER\pgFiles.h"\
	"\Paige3.0\PGHEADER\pgGrafx.h"\
	"\Paige3.0\PGHEADER\PGHText.h"\
	"\Paige3.0\PGHEADER\pgLists.h"\
	"\Paige3.0\PGHEADER\pgMemMgr.h"\
	"\Paige3.0\PGHEADER\pgMTraps.h"\
	"\Paige3.0\PGHEADER\pgSelect.h"\
	"\Paige3.0\PGHEADER\pgShapes.h"\
	"\Paige3.0\PGHEADER\pgSubref.h"\
	"\Paige3.0\PGHEADER\pgTables.h"\
	"\Paige3.0\PGHEADER\pgText.h"\
	"\Paige3.0\PGHEADER\pgTxtWid.h"\
	"\Paige3.0\PGHEADER\pgutils.h"\
	
NODEP_CPP_PGBAS=\
	"..\PGHEADER\WinMac.h"\
	

"$(INTDIR)\Pgbasics.obj" : $(SOURCE) $(DEP_CPP_PGBAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
