# Microsoft Visual C++ generated build script - Do not modify

PROJ = PAIGE
DEBUG = 0
PROGTYPE = 1
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = C:\PAIGE3.0\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = PGDLL.C     
FIRSTCPP =             
RC = rc
CFLAGS_D_WDLL = /nologo /G2 /W3 /Zi /ALw /Od /D "_DEBUG" /D "WIN16_COMPILE" /D "CREATE_MS_DLL" /I "C:\Paige3.0\pgheader" /GD /Fd"PAIGE.PDB"
CFLAGS_R_WDLL = /nologo /W3 /ALw /O1 /Os /D "NDEBUG" /D "WIN16_COMPILE" /D "CREATE_MS_DLL" /I "C:\Paige3.0\pgheader" /GD 
LFLAGS_D_WDLL = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /CO /NOE /ALIGN:16 /MAP:FULL
LFLAGS_R_WDLL = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /NOE /ALIGN:16 /MAP:FULL
LIBS_D_WDLL = oldnames libw commdlg shell olecli olesvr ldllcew
LIBS_R_WDLL = oldnames libw commdlg shell olecli olesvr ldllcew
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = PAIGE.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WDLL)
LFLAGS = $(LFLAGS_D_WDLL)
LIBS = $(LIBS_D_WDLL)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WDLL)
LFLAGS = $(LFLAGS_R_WDLL)
LIBS = $(LIBS_R_WDLL)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = PGDLL.SBR \
		PGIO.SBR \
		PGMEMMGR.SBR \
		PGOSUTL.SBR \
		PGSCRAP.SBR \
		PGWIN.SBR \
		PGDEBUG.SBR \
		PGBASICS.SBR \
		PGBOXES.SBR \
		PGCLIPBD.SBR \
		PGDEFPAR.SBR \
		PGDEFSTL.SBR \
		PGEDIT.SBR \
		PGEMBED.SBR \
		PGEXCEPS.SBR \
		PGFILES.SBR \
		PGFONTS.SBR \
		PGGRAFX.SBR \
		PGHTEXT.SBR \
		PGINIT.SBR \
		PGLINES.SBR \
		PGLISTS.SBR \
		PGMERGE.SBR \
		PGNBOXES.SBR \
		PGPAR.SBR \
		PGPRINT.SBR \
		PGREAD.SBR \
		PGSCRIPT.SBR \
		PGSCROLL.SBR \
		PGSELECT.SBR \
		PGSHAPES.SBR \
		PGSTLSHT.SBR \
		PGSTYLES.SBR \
		PGSUBREF.SBR \
		PGTABLES.SBR \
		PGTABS.SBR \
		PGTEXT.SBR \
		PGTXR.SBR \
		PGTXTBLK.SBR \
		PGTXTUTL.SBR \
		PGTXTWID.SBR \
		PGUTILS.SBR \
		PGWRITE.SBR \
		PGFRAME.SBR


PGDLL_DEP = 

PGIO_DEP = c:\paige3.0\pgheader\pgio.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\defprocs.h


PGMEMMGR_DEP = c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgio.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h


PGOSUTL_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgregion.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\pgio.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h


PGSCRAP_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgscrap.h


PGWIN_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgregion.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\w16nls.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pglists.h \
	c:\paige3.0\pgheader\pgsubref.h


PGDEBUG_DEP = c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h


PGBASICS_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pglists.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h \
	c:\paige3.0\pgheader\pgframe.h


PGBOXES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h


PGCLIPBD_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgclipbd.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h


PGDEFPAR_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtables.h


PGDEFSTL_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pghtext.h


PGEDIT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgstyles.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h


PGEMBED_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgframe.h


PGEXCEPS_DEP = c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\win16jmp.h


PGFILES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\packdefs.h \
	c:\paige3.0\pgheader\pgfiles.h


PGFONTS_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgstyles.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h


PGGRAFX_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgregion.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgembed.h


PGHTEXT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\machine.h


PGINIT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h


PGLINES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgutils.h


PGLISTS_DEP = c:\paige3.0\pgheader\pglists.h \
	c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\machine.h


PGMERGE_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgedit.h


PGNBOXES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgframe.h \
	c:\paige3.0\pgheader\pgfiles.h


PGPAR_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtables.h


PGPRINT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgshapes.h


PGREAD_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\packdefs.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgio.h \
	c:\paige3.0\pgheader\pgmtraps.h \
	c:\paige3.0\pgheader\pgtxr.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgframe.h


PGSCRIPT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtables.h


PGSCROLL_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgregion.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h


PGSELECT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pglists.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h


PGSHAPES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h


PGSTLSHT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgstyles.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtxr.h


PGSTYLES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgstyles.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h


PGSUBREF_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgstyles.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgtables.h \
	c:\paige3.0\pgheader\pghtext.h


PGTABLES_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtables.h \
	c:\paige3.0\pgheader\pgtxtwid.h


PGTABS_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgpar.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgutils.h


PGTEXT_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtables.h


PGTXR_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgtxr.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pghtext.h \
	c:\paige3.0\pgheader\pgtables.h \
	c:\paige3.0\pgheader\pgframe.h


PGTXTBLK_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgscript.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgtxr.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgtables.h


PGTXTUTL_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h


PGTXTWID_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgtxtwid.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgscript.h


PGUTILS_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\pgdefpar.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgedit.h


PGWRITE_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgexceps.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgsetjmp.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\win16jmp.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\defprocs.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgdefstl.h \
	c:\paige3.0\pgheader\packdefs.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\pgsubref.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgtxr.h \
	c:\paige3.0\pgheader\pghtext.h


PGFRAME_DEP = c:\paige3.0\pgheader\paige.h \
	c:\paige3.0\pgheader\pgmemmgr.h \
	c:\paige3.0\pgheader\userdefs.h \
	c:\paige3.0\pgheader\pgframe.h \
	c:\paige3.0\pgheader\pgfiles.h \
	c:\paige3.0\pgheader\machine.h \
	c:\paige3.0\pgheader\pgutils.h \
	c:\paige3.0\pgheader\pgtext.h \
	c:\paige3.0\pgheader\pgshapes.h \
	c:\paige3.0\pgheader\pgedit.h \
	c:\paige3.0\pgheader\pgselect.h \
	c:\paige3.0\pgheader\pgerrors.h \
	c:\paige3.0\pgheader\cpudefs.h \
	c:\paige3.0\pgheader\pgembed.h \
	c:\paige3.0\pgheader\pggrafx.h \
	c:\paige3.0\pgheader\pgbasics.h \
	c:\paige3.0\pgheader\pgosutl.h \
	c:\paige3.0\pgheader\pgtraps.h


all:	$(PROJ).DLL

PGDLL.OBJ:	PGPLATFO\PGDLL.C $(PGDLL_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c PGPLATFO\PGDLL.C

PGIO.OBJ:	PGPLATFO\PGIO.C $(PGIO_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGPLATFO\PGIO.C

PGMEMMGR.OBJ:	PGPLATFO\PGMEMMGR.C $(PGMEMMGR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGPLATFO\PGMEMMGR.C

PGOSUTL.OBJ:	PGPLATFO\PGOSUTL.C $(PGOSUTL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGPLATFO\PGOSUTL.C

PGSCRAP.OBJ:	PGPLATFO\PGSCRAP.C $(PGSCRAP_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGPLATFO\PGSCRAP.C

PGWIN.OBJ:	PGPLATFO\PGWIN.C $(PGWIN_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGPLATFO\PGWIN.C

PGDEBUG.OBJ:	PGDEBUG\PGDEBUG.C $(PGDEBUG_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGDEBUG\PGDEBUG.C

PGBASICS.OBJ:	PGSOURCE\PGBASICS.C $(PGBASICS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGBASICS.C

PGBOXES.OBJ:	PGSOURCE\PGBOXES.C $(PGBOXES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGBOXES.C

PGCLIPBD.OBJ:	PGSOURCE\PGCLIPBD.C $(PGCLIPBD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGCLIPBD.C

PGDEFPAR.OBJ:	PGSOURCE\PGDEFPAR.C $(PGDEFPAR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGDEFPAR.C

PGDEFSTL.OBJ:	PGSOURCE\PGDEFSTL.C $(PGDEFSTL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGDEFSTL.C

PGEDIT.OBJ:	PGSOURCE\PGEDIT.C $(PGEDIT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGEDIT.C

PGEMBED.OBJ:	PGSOURCE\PGEMBED.C $(PGEMBED_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGEMBED.C

PGEXCEPS.OBJ:	PGSOURCE\PGEXCEPS.C $(PGEXCEPS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGEXCEPS.C

PGFILES.OBJ:	PGSOURCE\PGFILES.C $(PGFILES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGFILES.C

PGFONTS.OBJ:	PGSOURCE\PGFONTS.C $(PGFONTS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGFONTS.C

PGGRAFX.OBJ:	PGSOURCE\PGGRAFX.C $(PGGRAFX_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGGRAFX.C

PGHTEXT.OBJ:	PGSOURCE\PGHTEXT.C $(PGHTEXT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGHTEXT.C

PGINIT.OBJ:	PGSOURCE\PGINIT.C $(PGINIT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGINIT.C

PGLINES.OBJ:	PGSOURCE\PGLINES.C $(PGLINES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGLINES.C

PGLISTS.OBJ:	PGSOURCE\PGLISTS.C $(PGLISTS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGLISTS.C

PGMERGE.OBJ:	PGSOURCE\PGMERGE.C $(PGMERGE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGMERGE.C

PGNBOXES.OBJ:	PGSOURCE\PGNBOXES.C $(PGNBOXES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGNBOXES.C

PGPAR.OBJ:	PGSOURCE\PGPAR.C $(PGPAR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGPAR.C

PGPRINT.OBJ:	PGSOURCE\PGPRINT.C $(PGPRINT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGPRINT.C

PGREAD.OBJ:	PGSOURCE\PGREAD.C $(PGREAD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGREAD.C

PGSCRIPT.OBJ:	PGSOURCE\PGSCRIPT.C $(PGSCRIPT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSCRIPT.C

PGSCROLL.OBJ:	PGSOURCE\PGSCROLL.C $(PGSCROLL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSCROLL.C

PGSELECT.OBJ:	PGSOURCE\PGSELECT.C $(PGSELECT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSELECT.C

PGSHAPES.OBJ:	PGSOURCE\PGSHAPES.C $(PGSHAPES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSHAPES.C

PGSTLSHT.OBJ:	PGSOURCE\PGSTLSHT.C $(PGSTLSHT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSTLSHT.C

PGSTYLES.OBJ:	PGSOURCE\PGSTYLES.C $(PGSTYLES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSTYLES.C

PGSUBREF.OBJ:	PGSOURCE\PGSUBREF.C $(PGSUBREF_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGSUBREF.C

PGTABLES.OBJ:	PGSOURCE\PGTABLES.C $(PGTABLES_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTABLES.C

PGTABS.OBJ:	PGSOURCE\PGTABS.C $(PGTABS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTABS.C

PGTEXT.OBJ:	PGSOURCE\PGTEXT.C $(PGTEXT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTEXT.C

PGTXR.OBJ:	PGSOURCE\PGTXR.C $(PGTXR_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTXR.C

PGTXTBLK.OBJ:	PGSOURCE\PGTXTBLK.C $(PGTXTBLK_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTXTBLK.C

PGTXTUTL.OBJ:	PGSOURCE\PGTXTUTL.C $(PGTXTUTL_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTXTUTL.C

PGTXTWID.OBJ:	PGSOURCE\PGTXTWID.C $(PGTXTWID_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGTXTWID.C

PGUTILS.OBJ:	PGSOURCE\PGUTILS.C $(PGUTILS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGUTILS.C

PGWRITE.OBJ:	PGSOURCE\PGWRITE.C $(PGWRITE_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGWRITE.C

PGFRAME.OBJ:	PGSOURCE\PGFRAME.C $(PGFRAME_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PGSOURCE\PGFRAME.C


$(PROJ).DLL::	PGDLL.OBJ PGIO.OBJ PGMEMMGR.OBJ PGOSUTL.OBJ PGSCRAP.OBJ PGWIN.OBJ PGDEBUG.OBJ \
	PGBASICS.OBJ PGBOXES.OBJ PGCLIPBD.OBJ PGDEFPAR.OBJ PGDEFSTL.OBJ PGEDIT.OBJ PGEMBED.OBJ \
	PGEXCEPS.OBJ PGFILES.OBJ PGFONTS.OBJ PGGRAFX.OBJ PGHTEXT.OBJ PGINIT.OBJ PGLINES.OBJ \
	PGLISTS.OBJ PGMERGE.OBJ PGNBOXES.OBJ PGPAR.OBJ PGPRINT.OBJ PGREAD.OBJ PGSCRIPT.OBJ \
	PGSCROLL.OBJ PGSELECT.OBJ PGSHAPES.OBJ PGSTLSHT.OBJ PGSTYLES.OBJ PGSUBREF.OBJ PGTABLES.OBJ \
	PGTABS.OBJ PGTEXT.OBJ PGTXR.OBJ PGTXTBLK.OBJ PGTXTUTL.OBJ PGTXTWID.OBJ PGUTILS.OBJ \
	PGWRITE.OBJ PGFRAME.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
PGDLL.OBJ +
PGIO.OBJ +
PGMEMMGR.OBJ +
PGOSUTL.OBJ +
PGSCRAP.OBJ +
PGWIN.OBJ +
PGDEBUG.OBJ +
PGBASICS.OBJ +
PGBOXES.OBJ +
PGCLIPBD.OBJ +
PGDEFPAR.OBJ +
PGDEFSTL.OBJ +
PGEDIT.OBJ +
PGEMBED.OBJ +
PGEXCEPS.OBJ +
PGFILES.OBJ +
PGFONTS.OBJ +
PGGRAFX.OBJ +
PGHTEXT.OBJ +
PGINIT.OBJ +
PGLINES.OBJ +
PGLISTS.OBJ +
PGMERGE.OBJ +
PGNBOXES.OBJ +
PGPAR.OBJ +
PGPRINT.OBJ +
PGREAD.OBJ +
PGSCRIPT.OBJ +
PGSCROLL.OBJ +
PGSELECT.OBJ +
PGSHAPES.OBJ +
PGSTLSHT.OBJ +
PGSTYLES.OBJ +
PGSUBREF.OBJ +
PGTABLES.OBJ +
PGTABS.OBJ +
PGTEXT.OBJ +
PGTXR.OBJ +
PGTXTBLK.OBJ +
PGTXTUTL.OBJ +
PGTXTWID.OBJ +
PGUTILS.OBJ +
PGWRITE.OBJ +
PGFRAME.OBJ +
$(OBJS_EXT)
$(PROJ).DLL
$(MAPFILE)
c:\msvc\lib\+
c:\msvc\mfc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) $@
	implib /nowep $(PROJ).LIB $(PROJ).DLL


run: $(PROJ).DLL
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
