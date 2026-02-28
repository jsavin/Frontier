
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#ifndef langexternalinclude
#define langexternalinclude


#ifndef langinclude

	#include "lang.h"

#endif

#ifndef dbinclude

	#include "db.h"

#endif

#ifndef FRONTIER_PORTABLE
#ifndef shellinclude
    #include "shell.h"
#endif
#else
    /* Portable core: no UI types */
    #ifdef FRONTIER_PORTABLE
        /* Forward declare opaque types for portable stubs */
        struct tywindowinfo;
        struct tyconfigrecord;
        /* Don't define typedefs here to avoid conflicts with shell.h/frontierconfig.h */
    #else
        /* Full UI types when shell.h is present */
        #ifndef shellinclude
            struct tywindowinfo; /* incomplete */
            #ifndef hdlwindowinfo
            typedef struct tywindowinfo** hdlwindowinfo; /* match real shape */
            #endif
            #ifndef tyconfigrecord
            typedef struct tyconfigrecord tyconfigrecord; /* opaque */
            #endif
        #endif
    #endif
#endif


#define idophashresource 134
#define idwordhashresource 135
#define idtablehashresource 136
#define idmenuhashresource 137
#define idpicthashresource 138


typedef enum tyexternalid { /*11/17/90 DW: order determines sort order when viewing by kind*/
	
	idoutlineprocessor,
	
	idwordprocessor,
	
	idheadrecord,
	
	idtableprocessor,
	
	idscriptprocessor,
	
	idmenuprocessor,
	
	idpictprocessor,
	
	
	idappleprocessor,
	
	
	/*new types must be added at end of list, these get saved on disk*/
	
	idcardprocessor,
	
	ctexternalprocessors
	} tyexternalid;

#pragma pack(2)
typedef struct typrocessorcallbacks { /*this structure isn't currently used*/
	
	callback loadroutine;
	
	callback unloadroutine;
	
	callback packroutine;
	
	callback unpackroutine;
	
	callback editroutine;
	
	callback newroutine;
	
	callback disposeroutine;
	
	callback converttotextroutine;
	
	callback getdisplaystringroutine;
	
	callback gettypestringroutine;
	
	callback functionvalueroutine;
	
	hdlhashtable functiontable;
	
	short tableresourceid;
	
	long processorrefcon;
	} typrocessorcallbacks, *ptrprocessorcallbacks;
#pragma options align=reset

/*
	11/15/01 dmb: added flags for various external types so they can use this record,
	without having to add a bunch of macros to preserve existing code.
*/
#pragma pack(2)
typedef struct tyexternalvariable {
	
	unsigned short id;	/*tyexternalid*/
	
	unsigned short flinmemory: 1; /*if true, variabledata is in a handle, else a dbaddress*/
	
	unsigned short flmayaffectdisplay: 1; /*not in memory, but being displayed in a table window*/
	
	unsigned short flpacked: 1; /* for wp doc; it isn't being edited, so we store it packed*/

	unsigned short flscript: 1; /* for outlines and scripts; they're identical, except for this bit*/

	unsigned short flsystemtable: 1; /*for tables: was it created by the system, or by a user script?*/

	#ifdef xmlfeatures
		unsigned short flxml: 1; /*preserve for tables; is it an xml table?*/
	#endif

//	unsigned short variableflags: 3; /*these can be defined independently for each variable*/
	
	long variabledata; /*either a handle to data record or a dbaddress*/
	
	hdldatabaserecord hdatabase; // 5.0a18 dmb

	dbaddress oldaddress; /*last place this variable was stored in db*/

	} tyexternalvariable, *ptrexternalvariable, **hdlexternalvariable;
#pragma options align=reset

typedef hdlexternalvariable hdlexternalhandle;

/*
typedef struct tyexternalhandle {
	
	tyexternalid id; 
	
	Handle hdata; 
	} tyexternalhandle, *ptrexternalhandle, **hdlexternalhandle;
*/

#pragma pack(2)
// 2025-10-27 Codex: Normalize external handle layout for headless 64-bit builds (1-byte id + padding).

typedef struct tydiskexternalhandle {
	
	short versionnumber; /*this structure is stored on disk*/
	
	unsigned char id;
	unsigned char unused;

	} tydiskexternalhandle;
#pragma options align=reset
#define externaldiskversionnumber 1


/*prototypes*/

extern tyexternalid langexternalgettype (tyvaluerecord);

extern boolean langexternalgettable (bigstring, hdlhashtable *);

extern boolean langexternalvaltotable (tyvaluerecord, hdlhashtable *, hdlhashnode);

extern boolean langexternalfindvariable (hdlexternalvariable, hdlhashtable *, bigstring);

extern boolean langexternalgettablevalue (hdltreenode, short, hdlhashtable *);

extern hdldatabaserecord langexternalgetdatabase (hdlexternalvariable);

extern void langexternalsetdatabase (hdlexternalvariable, hdldatabaserecord);

extern boolean langsetexternalsymbol (hdlhashtable, bigstring, tyexternalid, Handle);

extern boolean langexternaldontsave (hdlhashtable, bigstring);

extern boolean langexternalpleasesave (hdlhashtable, bigstring);

extern boolean langexternaltypestring (hdlexternalhandle, bigstring);

extern boolean langexternalgetdisplaystring (hdlexternalhandle, bigstring);

extern boolean langexternalisdirty (hdlexternalhandle);

extern boolean langexternalsetdirty (hdlexternalhandle, boolean);

extern boolean langexternalpack (hdlexternalhandle, Handle *, boolean *);

extern boolean langexternalunpack (Handle, hdlexternalhandle *, hdldatabaserecord);

/* Context-aware internal version (used by db_format layer) */
struct db_context; /* forward declaration */
extern boolean langexternalpack_internal (const struct db_context *, hdlexternalhandle, Handle *, boolean *);

extern boolean ensure_external_in_memory (const struct db_context *, hdlexternalvariable);

/* Legacy (32-bit) pack/unpack shims used during migration. */
extern boolean langexternalpack_legacy (hdlexternalhandle, Handle *, boolean *);
extern boolean langexternalunpack_legacy (Handle, hdlexternalhandle *);

extern boolean langexternalmemorypack (hdlexternalhandle, Handle *, hdlhashnode);

extern boolean langexternalmemoryunpack (Handle, hdlexternalhandle *, hdldatabaserecord);

extern boolean langexternalcopyvalue (const tyvaluerecord *, tyvaluerecord *);

extern boolean langexternalcoercetostring (tyvaluerecord *);

extern boolean langexternalgetowningwindow (hdlwindowinfo *);

extern void langexternalquotename (bigstring);

extern void langexternalbracketname (bigstring);

extern boolean langexternalgetfullpath (hdlhashtable, bigstring, bigstring, hdlwindowinfo *);

extern boolean langexternalgetquotedpath (hdlhashtable, bigstring, bigstring);

extern boolean langexternalgetexternalparam (hdltreenode, short, short *, hdlexternalvariable *);

extern boolean langexternalzoomfrom (tyvaluerecord, hdlhashtable, bigstring, rectparam);

extern boolean langexternalzoom (tyvaluerecord, hdlhashtable, bigstring);

extern boolean langexternalzoomfilewindow (const tyvaluerecord *, ptrfilespec, boolean);

extern boolean langexternalwindowopen (tyvaluerecord, hdlwindowinfo *);

extern boolean langexternalwindowclosed (hdlexternalvariable);

extern boolean langexternaldisposevariable (hdlexternalvariable, boolean, boolean (*) (hdlexternalvariable, boolean));

extern boolean langexternaldisposevalue (tyvaluerecord, boolean);

extern boolean langexternalgetconfig (tyvaluetype, short, struct tyconfigrecord*);

extern boolean langexternalnewvalue (tyexternalid, Handle, tyvaluerecord *);

extern boolean langexternalvaltocode (tyvaluerecord, hdltreenode *);

extern boolean langexternalgetvalsize (tyvaluerecord, long *);

extern boolean langnewexternalvariable (boolean, long, hdlexternalvariable *, hdldatabaserecord);

/* Single-point state transition functions (MODE_SINGLE_DECISION_POINT pattern) */
extern boolean external_set_ondisk (hdlexternalvariable, dbaddress);

/* Returns boolean for API consistency and future invariant checks (e.g., reference counting).
 * Currently always succeeds - no invariants to validate for in-memory transitions. */
extern boolean external_set_inmemory (hdlexternalvariable, Handle, dbaddress);

extern short langexternalcomparetypes (tyexternalid, tyexternalid);

extern boolean langexternalsurfacekey (hdlexternalvariable);

extern boolean langexternalpacktotext (hdlexternalhandle, Handle);

extern boolean langexternalsearch (tyvaluerecord, boolean *);

extern boolean langexternalcontinuesearch (hdlexternalvariable);

extern boolean langexternalgettimes (hdlexternalhandle, int64_t *, int64_t *, hdlhashnode);

extern boolean langexternalsettimes (hdlexternalhandle, int64_t, int64_t, hdlhashnode);

extern boolean langexternalfindusedblocks (hdlexternalhandle, bigstring);
extern boolean langexternalpacktotext_legacy (hdlexternalhandle, Handle);

extern boolean langexternaltitleclick (Point, hdlexternalvariable);

extern OSType langexternalgettypeid (tyvaluerecord);

extern tyvaluetype langexternalgetvaluetype (OSType);

extern boolean langexternalregisterwindow (hdlexternalvariable);

extern boolean langexternalunregisterwindow (hdlwindowinfo hw);

extern boolean langexternalcloseregisteredwindows (boolean);

extern boolean langexternalrefdata (hdlexternalvariable, Handle *);

extern boolean langexternalrefdata_context (const struct db_context *, hdlexternalvariable, Handle *);

boolean langexternalsetreadonly (hdlexternalvariable hv, boolean flreadonly); /*7.0b6 PBS*/

boolean langexternalgetreadonly (hdlexternalvariable hv); /*7.0b6 PBS*/

extern boolean fullpathstats (hdlhashtable intable, boolean flfirst);

extern boolean hashstatsverb (tyvaluerecord *v);

extern boolean langexternalsymbolchanged (hdlhashtable htable, const bigstring bsname, hdlhashnode hnode, boolean flvalue);

extern boolean langexternalsymbolinserted (hdlhashtable htable, const bigstring bsname, hdlhashnode hnode);

#endif
