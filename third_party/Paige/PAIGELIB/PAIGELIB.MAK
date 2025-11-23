# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

!IF "$(CFG)" == ""
CFG=Paigelib - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Paigelib - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Paigelib - Win32 Release" && "$(CFG)" !=\
 "Paigelib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "Paigelib.mak" CFG="Paigelib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Paigelib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Paigelib - Win32 Debug" (based on "Win32 (x86) Static Library")
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
# PROP Target_Last_Scanned "Paigelib - Win32 Debug"
CPP=cl.exe

!IF  "$(CFG)" == "Paigelib - Win32 Release"

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

ALL : "$(OUTDIR)\Paigelib.lib"

CLEAN : 
	-@erase "$(INTDIR)\Pgbasics.obj"
	-@erase "$(INTDIR)\Pgboxes.obj"
	-@erase "$(INTDIR)\Pgclipbd.obj"
	-@erase "$(INTDIR)\Pgdefpar.obj"
	-@erase "$(INTDIR)\Pgdefstl.obj"
	-@erase "$(INTDIR)\Pgedit.obj"
	-@erase "$(INTDIR)\Pgembed.obj"
	-@erase "$(INTDIR)\Pgexceps.obj"
	-@erase "$(INTDIR)\Pgfiles.obj"
	-@erase "$(INTDIR)\Pgfonts.obj"
	-@erase "$(INTDIR)\pgFrame.obj"
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
	-@erase "$(OUTDIR)\Paigelib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O1 /I "C:\Paige3.0\pgheader" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /YX /c
CPP_PROJ=/nologo /ML /W3 /GX /O1 /I "C:\Paige3.0\pgheader" /D "NDEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /Fp"$(INTDIR)/Paigelib.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Paigelib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/Paigelib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Pgbasics.obj" \
	"$(INTDIR)\Pgboxes.obj" \
	"$(INTDIR)\Pgclipbd.obj" \
	"$(INTDIR)\Pgdefpar.obj" \
	"$(INTDIR)\Pgdefstl.obj" \
	"$(INTDIR)\Pgedit.obj" \
	"$(INTDIR)\Pgembed.obj" \
	"$(INTDIR)\Pgexceps.obj" \
	"$(INTDIR)\Pgfiles.obj" \
	"$(INTDIR)\Pgfonts.obj" \
	"$(INTDIR)\pgFrame.obj" \
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

"$(OUTDIR)\Paigelib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

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

ALL : "$(OUTDIR)\Paigelib.lib"

CLEAN : 
	-@erase "$(INTDIR)\Pgbasics.obj"
	-@erase "$(INTDIR)\Pgboxes.obj"
	-@erase "$(INTDIR)\Pgclipbd.obj"
	-@erase "$(INTDIR)\Pgdefpar.obj"
	-@erase "$(INTDIR)\Pgdefstl.obj"
	-@erase "$(INTDIR)\Pgedit.obj"
	-@erase "$(INTDIR)\Pgembed.obj"
	-@erase "$(INTDIR)\Pgexceps.obj"
	-@erase "$(INTDIR)\Pgfiles.obj"
	-@erase "$(INTDIR)\Pgfonts.obj"
	-@erase "$(INTDIR)\pgFrame.obj"
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
	-@erase "$(OUTDIR)\Paigelib.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /Z7 /Od /I "C:\Paige3.0\pgheader" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /YX /c
CPP_PROJ=/nologo /MLd /W3 /GX /Z7 /Od /I "C:\Paige3.0\pgheader" /D "_DEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "WIN32_COMPILE" /Fp"$(INTDIR)/Paigelib.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Paigelib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
LIB32_FLAGS=/nologo /out:"$(OUTDIR)/Paigelib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\Pgbasics.obj" \
	"$(INTDIR)\Pgboxes.obj" \
	"$(INTDIR)\Pgclipbd.obj" \
	"$(INTDIR)\Pgdefpar.obj" \
	"$(INTDIR)\Pgdefstl.obj" \
	"$(INTDIR)\Pgedit.obj" \
	"$(INTDIR)\Pgembed.obj" \
	"$(INTDIR)\Pgexceps.obj" \
	"$(INTDIR)\Pgfiles.obj" \
	"$(INTDIR)\Pgfonts.obj" \
	"$(INTDIR)\pgFrame.obj" \
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

"$(OUTDIR)\Paigelib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
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

# Name "Paigelib - Win32 Release"
# Name "Paigelib - Win32 Debug"

!IF  "$(CFG)" == "Paigelib - Win32 Release"

!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgwin.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGWIN=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	"\Paige3.0\pgheader\w16nls.h"\
	
NODEP_CPP_PGWIN=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgwin.obj" : $(SOURCE) $(DEP_CPP_PGWIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGWIN=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	"\Paige3.0\pgheader\w16nls.h"\
	
NODEP_CPP_PGWIN=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgwin.obj" : $(SOURCE) $(DEP_CPP_PGWIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgmemmgr.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGMEM=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGMEM=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgmemmgr.obj" : $(SOURCE) $(DEP_CPP_PGMEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGMEM=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGMEM=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgmemmgr.obj" : $(SOURCE) $(DEP_CPP_PGMEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgosutl.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGOSU=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGOSU=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgosutl.obj" : $(SOURCE) $(DEP_CPP_PGOSU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGOSU=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGOSU=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgosutl.obj" : $(SOURCE) $(DEP_CPP_PGOSU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgscrap.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSCR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgscrap.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscrap.obj" : $(SOURCE) $(DEP_CPP_PGSCR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSCR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgscrap.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscrap.obj" : $(SOURCE) $(DEP_CPP_PGSCR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgplatfo\Pgio.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGIO_=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGIO_=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgio.obj" : $(SOURCE) $(DEP_CPP_PGIO_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGIO_=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGIO_=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgio.obj" : $(SOURCE) $(DEP_CPP_PGIO_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgwrite.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGWRI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGWRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgwrite.obj" : $(SOURCE) $(DEP_CPP_PGWRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGWRI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGWRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgwrite.obj" : $(SOURCE) $(DEP_CPP_PGWRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgboxes.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGBOX=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGBOX=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgboxes.obj" : $(SOURCE) $(DEP_CPP_PGBOX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGBOX=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGBOX=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgboxes.obj" : $(SOURCE) $(DEP_CPP_PGBOX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgclipbd.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGCLI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgClipBd.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGCLI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgclipbd.obj" : $(SOURCE) $(DEP_CPP_PGCLI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGCLI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgClipBd.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGCLI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgclipbd.obj" : $(SOURCE) $(DEP_CPP_PGCLI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgdefpar.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGDEF=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGDEF=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgdefpar.obj" : $(SOURCE) $(DEP_CPP_PGDEF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGDEF=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGDEF=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgdefpar.obj" : $(SOURCE) $(DEP_CPP_PGDEF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgdefstl.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGDEFS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGDEFS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgdefstl.obj" : $(SOURCE) $(DEP_CPP_PGDEFS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGDEFS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGDEFS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgdefstl.obj" : $(SOURCE) $(DEP_CPP_PGDEFS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgedit.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGEDI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGEDI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgedit.obj" : $(SOURCE) $(DEP_CPP_PGEDI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGEDI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGEDI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgedit.obj" : $(SOURCE) $(DEP_CPP_PGEDI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgembed.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGEMB=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGEMB=\
	"..\pgheader\WinMac.h"\
	"..\Pgsource\pgGIF.h"\
	

"$(INTDIR)\Pgembed.obj" : $(SOURCE) $(DEP_CPP_PGEMB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGEMB=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGEMB=\
	"..\pgheader\WinMac.h"\
	"..\Pgsource\pgGIF.h"\
	

"$(INTDIR)\Pgembed.obj" : $(SOURCE) $(DEP_CPP_PGEMB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgexceps.c
DEP_CPP_PGEXC=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	
NODEP_CPP_PGEXC=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgexceps.obj" : $(SOURCE) $(DEP_CPP_PGEXC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgfiles.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGFIL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGFIL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgfiles.obj" : $(SOURCE) $(DEP_CPP_PGFIL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGFIL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGFIL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgfiles.obj" : $(SOURCE) $(DEP_CPP_PGFIL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgfonts.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGFON=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGFON=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgfonts.obj" : $(SOURCE) $(DEP_CPP_PGFON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGFON=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGFON=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgfonts.obj" : $(SOURCE) $(DEP_CPP_PGFON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pggrafx.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGGRA=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGGRA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pggrafx.obj" : $(SOURCE) $(DEP_CPP_PGGRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGGRA=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGGRA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pggrafx.obj" : $(SOURCE) $(DEP_CPP_PGGRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pghtext.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGHTE=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGHTE=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pghtext.obj" : $(SOURCE) $(DEP_CPP_PGHTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGHTE=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGHTE=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pghtext.obj" : $(SOURCE) $(DEP_CPP_PGHTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pginit.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGINI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGINI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pginit.obj" : $(SOURCE) $(DEP_CPP_PGINI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGINI=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGINI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pginit.obj" : $(SOURCE) $(DEP_CPP_PGINI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pglines.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGLIN=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGLIN=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pglines.obj" : $(SOURCE) $(DEP_CPP_PGLIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGLIN=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGLIN=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pglines.obj" : $(SOURCE) $(DEP_CPP_PGLIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pglists.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGLIS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGLIS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pglists.obj" : $(SOURCE) $(DEP_CPP_PGLIS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGLIS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGLIS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pglists.obj" : $(SOURCE) $(DEP_CPP_PGLIS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgmerge.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGMER=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGMER=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgmerge.obj" : $(SOURCE) $(DEP_CPP_PGMER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGMER=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGMER=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgmerge.obj" : $(SOURCE) $(DEP_CPP_PGMER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgnboxes.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGNBO=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGNBO=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgnboxes.obj" : $(SOURCE) $(DEP_CPP_PGNBO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGNBO=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGNBO=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgnboxes.obj" : $(SOURCE) $(DEP_CPP_PGNBO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgpar.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGPAR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGPAR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgpar.obj" : $(SOURCE) $(DEP_CPP_PGPAR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGPAR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGPAR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgpar.obj" : $(SOURCE) $(DEP_CPP_PGPAR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgprint.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGPRI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGPRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgprint.obj" : $(SOURCE) $(DEP_CPP_PGPRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGPRI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGPRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgprint.obj" : $(SOURCE) $(DEP_CPP_PGPRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgread.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGREA=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGREA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgread.obj" : $(SOURCE) $(DEP_CPP_PGREA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGREA=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\pgheader\PackDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgIO.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGREA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgread.obj" : $(SOURCE) $(DEP_CPP_PGREA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgscript.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSCRI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscript.obj" : $(SOURCE) $(DEP_CPP_PGSCRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSCRI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCRI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscript.obj" : $(SOURCE) $(DEP_CPP_PGSCRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgscroll.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSCRO=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCRO=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscroll.obj" : $(SOURCE) $(DEP_CPP_PGSCRO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSCRO=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgRegion.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSCRO=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgscroll.obj" : $(SOURCE) $(DEP_CPP_PGSCRO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgselect.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSEL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSEL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgselect.obj" : $(SOURCE) $(DEP_CPP_PGSEL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSEL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSEL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgselect.obj" : $(SOURCE) $(DEP_CPP_PGSEL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgshapes.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSHA=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSHA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgshapes.obj" : $(SOURCE) $(DEP_CPP_PGSHA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSHA=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSHA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgshapes.obj" : $(SOURCE) $(DEP_CPP_PGSHA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgstlsht.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSTL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSTL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgstlsht.obj" : $(SOURCE) $(DEP_CPP_PGSTL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSTL=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSTL=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgstlsht.obj" : $(SOURCE) $(DEP_CPP_PGSTL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgstyles.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSTY=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSTY=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgstyles.obj" : $(SOURCE) $(DEP_CPP_PGSTY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSTY=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSTY=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgstyles.obj" : $(SOURCE) $(DEP_CPP_PGSTY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgsubref.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGSUB=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSUB=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgsubref.obj" : $(SOURCE) $(DEP_CPP_PGSUB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGSUB=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgStyles.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGSUB=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgsubref.obj" : $(SOURCE) $(DEP_CPP_PGSUB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtables.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTAB=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTAB=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtables.obj" : $(SOURCE) $(DEP_CPP_PGTAB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTAB=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTAB=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtables.obj" : $(SOURCE) $(DEP_CPP_PGTAB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtabs.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTABS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTABS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtabs.obj" : $(SOURCE) $(DEP_CPP_PGTABS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTABS=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgPar.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTABS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtabs.obj" : $(SOURCE) $(DEP_CPP_PGTABS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtext.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTEX=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTEX=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtext.obj" : $(SOURCE) $(DEP_CPP_PGTEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTEX=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTEX=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtext.obj" : $(SOURCE) $(DEP_CPP_PGTEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxr.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTXR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxr.obj" : $(SOURCE) $(DEP_CPP_PGTXR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTXR=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXR=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxr.obj" : $(SOURCE) $(DEP_CPP_PGTXR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtblk.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTXT=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXT=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtblk.obj" : $(SOURCE) $(DEP_CPP_PGTXT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTXT=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgosutl.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTraps.h"\
	"\Paige3.0\pgheader\pgTxr.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXT=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtblk.obj" : $(SOURCE) $(DEP_CPP_PGTXT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtutl.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTXTU=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXTU=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtutl.obj" : $(SOURCE) $(DEP_CPP_PGTXTU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTXTU=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXTU=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtutl.obj" : $(SOURCE) $(DEP_CPP_PGTXTU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgtxtwid.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGTXTW=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXTW=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtwid.obj" : $(SOURCE) $(DEP_CPP_PGTXTW) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGTXTW=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgScript.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGTXTW=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgtxtwid.obj" : $(SOURCE) $(DEP_CPP_PGTXTW) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgutils.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGUTI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGUTI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgutils.obj" : $(SOURCE) $(DEP_CPP_PGUTI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGUTI=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGUTI=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgutils.obj" : $(SOURCE) $(DEP_CPP_PGUTI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\Pgbasics.c

!IF  "$(CFG)" == "Paigelib - Win32 Release"

DEP_CPP_PGBAS=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGBAS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgbasics.obj" : $(SOURCE) $(DEP_CPP_PGBAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "Paigelib - Win32 Debug"

DEP_CPP_PGBAS=\
	"..\pgheader\win16jmp.h"\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\DefProcs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgBasics.h"\
	"\Paige3.0\pgheader\pgDefPar.h"\
	"\Paige3.0\pgheader\pgDefstl.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\pgheader\pgExceps.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\pgheader\PGHText.h"\
	"\Paige3.0\pgheader\pgLists.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\pgheader\pgSubref.h"\
	"\Paige3.0\pgheader\pgTables.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgTxtWid.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGBAS=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\Pgbasics.obj" : $(SOURCE) $(DEP_CPP_PGBAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Paige3.0\Pgsource\pgFrame.c
DEP_CPP_PGFRA=\
	"\Paige3.0\Pgheader\CPUDefs.h"\
	"\Paige3.0\pgheader\machine.h"\
	"\Paige3.0\Pgheader\Paige.h"\
	"\Paige3.0\pgheader\pgEdit.h"\
	"\Paige3.0\Pgheader\pgEmbed.h"\
	"\Paige3.0\Pgheader\pgErrors.h"\
	"\Paige3.0\Pgheader\pgFiles.h"\
	"\Paige3.0\pgheader\pgFrame.h"\
	"\Paige3.0\Pgheader\pgGrafx.h"\
	"\Paige3.0\Pgheader\pgMemMgr.h"\
	"\Paige3.0\Pgheader\pgMTraps.h"\
	"\Paige3.0\pgheader\pgSelect.h"\
	"\Paige3.0\Pgheader\pgSetJmp.h"\
	"\Paige3.0\pgheader\pgShapes.h"\
	"\Paige3.0\Pgheader\pgText.h"\
	"\Paige3.0\pgheader\pgUtils.h"\
	"\Paige3.0\Pgheader\Userdefs.h"\
	
NODEP_CPP_PGFRA=\
	"..\pgheader\WinMac.h"\
	

"$(INTDIR)\pgFrame.obj" : $(SOURCE) $(DEP_CPP_PGFRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
