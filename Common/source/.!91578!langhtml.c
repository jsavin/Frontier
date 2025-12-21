
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

/*dmb 6/17/96 4.0.2b1: integrated macroprocessor ucmd code*/

#include "frontier.h"
#include "standard.h"

#include "error.h"
#include "file.h"
#include "memory.h"
#include "ops.h"
#include "resources.h"
#include "strings.h"
#include "lang.h"
#include "langipc.h"
#include "langinternal.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "langhtml.h"
#include "langwinipc.h"
#include "process.h"
#include "tableinternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "op.h"
#include "opinternal.h"
#include "oplist.h"
#include "opverbs.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "shell.rsrc.h"
#include "timedate.h"
#include "WinSockNetEvents.h"
#include "osacomponent.h"

#include "tableverbs.h"  //6.1b8 AR: we need gettablevalue
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */

#include "iso8859.c"

extern boolean frontierversion (tyvaluerecord *v); //implemted in shellsysverbs.c
extern boolean sysos (tyvaluerecord *v); //implemted in shellsysverbs.c

#define fldebugwebsite false

#define str_separatorline BIGSTRING ("\x17<hr size=\"2\" width=\"100%\" />\r")/* 2005-12-18 creedon - end tag with space slash for compatibility with post HTML 4.01 standards */
#define str_macroerror BIGSTRING ("\x20<b>[</b>Macro error: ^0<b>]</b>\r")
#define str_mailto	BIGSTRING ("\x1A<a href=\"mailto:^0\">^0</a>")
#define str_hotlink	BIGSTRING ("\x13<a href=\"^0\">^1</a>")
#define str_pagebreak BIGSTRING ("\x05<p />") /* 2005-10-29 creedon - changed from <p> to <p /> for compatibility with post HTML 4.01 standards */
#define str_startbold BIGSTRING ("\x03<b>")
#define str_endbold	BIGSTRING ("\x04</b>")
#define str_default BIGSTRING ("\x07" "default")
#define str_index BIGSTRING ("\x05" "index")

#define str_closetag BIGSTRING ("\x05" "</^0>")

#define flcurlybracemacros true
#define startmacrochar '{'
#define endmacrochar '}'
#define flvariablemacrocharacters true /*PBS 7.1b1: variable macro characters*/

#define maxglossarynamelength 127 /*DW 5/3/96: upped from 31*/

#define str_no	BIGSTRING ("\x02" "no")
#define str_yes	BIGSTRING ("\x03" "yes")

#define str_adrpagetable		BIGSTRING ("\x16" "html.data.adrpagetable")
#define str_websitesdata		BIGSTRING ("\x0e" "websites.#data")
#define str_userhtmlprefs		BIGSTRING ("\x0f" "user.html.prefs")
#define str_usermacros			BIGSTRING ("\x10" "user.html.macros")
#define str_standardmacros		BIGSTRING ("\x18" "html.data.standardMacros")
#define str_tools				BIGSTRING ("\x05" "tools")
#define str_glossary			BIGSTRING ("\x08" "glossary")
#define str_images				BIGSTRING ("\x06" "images")
#define str_glosspatch			BIGSTRING ("\x0e" "[[#glossPatch ")
#define str_useglosspatcher		BIGSTRING ("\x0f" "useGlossPatcher")
#define str_renderedtext		BIGSTRING ("\x0c" "renderedtext")
	#define str_iso8859map		BIGSTRING ("\x15" "html.data.iso8859.mac")

#define str_template			BIGSTRING ("\x08" "template")
#define str_indirecttemplate	BIGSTRING ("\x10" "indirectTemplate")
#define str_adrobject			BIGSTRING ("\x09" "adrobject")
#define str_ftpsite				BIGSTRING ("\x07" "ftpsite")
#define str_fileextension 		BIGSTRING ("\x0d" "fileextension")
#define str_maxfilenamelength	BIGSTRING ("\x11" "maxfilenamelength")
#define str_defaulttemplate		BIGSTRING ("\x0f" "defaulttemplate")
#define str_defaultfilename		BIGSTRING ("\x0f" "defaultfilename")
#define str_directivesonlyatbeginning	BIGSTRING ("\x19" "directivesOnlyAtBeginning")

#define STR_P_ERRORPAGETEMPLATE			BIGSTRING ("\x4C" "<HTML><HEAD><TITLE>^0</TITLE></HEAD><BODY><H1>^0</H1><P>^1</P></BODY></HTML>")
#define STR_P_MISSING_HOST_HEADER		BIGSTRING ("\x40" "Every HTTP/1.1 request must include a Host header")
#define STR_P_UNSUPPORTED_VERSION		BIGSTRING ("\x24" "This server does not support HTTP/^0")
#define STR_P_INVALID_URI				BIGSTRING ("\x23" "All URIs must begin with / or http:")
#define STR_P_BODY_NOT_READ				BIGSTRING ("\x22" "The request body couldn't be read.")
#define STR_P_METHOD_NOT_ALLOWED		BIGSTRING ("\x20" "^0 isn't allowed on this object.")
#define STR_P_INVALID_REQUEST_LINE		BIGSTRING ("\x1C" "The request line is invalid.")
#define STR_P_USERWEBSERVERPOSTFILTERS	BIGSTRING ("\x1A" "user.webserver.postfilters")
#define STR_P_USERWEBSERVERPREFILTERS	BIGSTRING ("\x19" "user.webserver.prefilters")
#define STR_P_USERWEBSERVERRESPONDERS	BIGSTRING ("\x19" "user.webserver.responders")
#define STR_P_PROCESSING_STARTED		BIGSTRING ("\x18" "requestProcessingStarted")
#define STR_P_WEBSERVERDATARESPONSES	BIGSTRING ("\x18" "webserver.data.responses")

#define STR_P_macrostartchars			BIGSTRING ("\x14" "macrostartcharacters") /*PBS 7.1b1*/
#define STR_P_macroendchars				BIGSTRING ("\x12" "macroendcharacters")

#define STR_P_flprocessmacrosintags     BIGSTRING ("\x17" "processmacrosinhtmltags")

#ifdef PIKE /*7.0 PBS: server string is Radio UserLand*/

#ifndef OPMLEDITOR

		#define STR_P_SERVERSTRING				BIGSTRING ("\x15" "Radio UserLand/^0-^1X")

#else  // OPMLEDITOR 2005-04-06 dluebbert
		#define STR_P_SERVERSTRING				BIGSTRING ("\x0b" "OPML/^0-^1X")
#endif // OPMLEDITOR

#else // !PIKE

		#define STR_P_SERVERSTRING			BIGSTRING ("\x0f" "Frontier/^0-^1X") /* 2005-01-04 creedon - removed UserLand for open source release */

#endif  //!PIKE

#define STR_P_RESPONDERERROR			BIGSTRING ("\x16" "Responder method error")
#define STR_P_LOGADD					BIGSTRING ("\x16" "log.addToGuestDatabase")
#define STR_P_USERWEBSERVERCONFIG		BIGSTRING ("\x15" "user.webserver.config")
#define STR_P_USERWEBSERVERPREFS		BIGSTRING ("\x14" "user.webserver.prefs")
#define STR_P_USERWEBSERVERSTATS		BIGSTRING ("\x14" "user.webserver.stats")
#define STR_P_INETDCONFIGTABLEADR		BIGSTRING ("\x13" "inetdConfigTableAdr")
#define STR_P_DEFAULTTIMEOUTSECS		BIGSTRING ("\x12" "defaultTimeoutSecs")
#define STR_P_USERINETDLISTENS			BIGSTRING ("\x12" "user.inetd.listens")
#define STR_P_WEBSERVERDISPATCH			BIGSTRING ("\x12" "webserver.dispatch")
#define STR_P_RESPONDERTABLEADR			BIGSTRING ("\x11" "responderTableAdr")
#define STR_P_POSTFILTERERROR			BIGSTRING ("\x11" "Post Filter error")
#define STR_P_PREFILTERERROR			BIGSTRING ("\x10" "Pre filter error")
#define STR_P_DEFAULTRESPONDER			BIGSTRING ("\x10" "defaultResponder")
#define STR_P_USERINETDPREFS			BIGSTRING ("\x10" "user.inetd.prefs")
#define STR_P_RESPONSEHEADERS			BIGSTRING ("\x0F" "responseHeaders")
#define STR_P_WHATWEREWEDOING			BIGSTRING ("\x0F" "whatWereWeDoing")
#define STR_P_RETURNCHUNKSIZE			BIGSTRING ("\x0F" "returnChunkSize")
#define STR_P_ADRHEADERTABLE			BIGSTRING ("\x0E" "adrHeaderTable")
#define STR_P_MAXCONNECTIONS			BIGSTRING ("\x0E" "maxConnections")
#define STR_P_REQUESTHEADERS	 		BIGSTRING ("\x0E" "requestHeaders")
#define STR_P_CONTENT_LENGTH			BIGSTRING ("\x0E" "Content-Length")
#define STR_P_WHATWENTWRONG				BIGSTRING ("\x0D" "whatWentWrong")
#define STR_P_100CONTINUE				BIGSTRING ("\x0C" "100-continue")
#define STR_P_RESPONSEBODY				BIGSTRING ("\x0C" "responseBody")
#define STR_P_REQUESTBODY				BIGSTRING ("\x0B" "requestBody")
#define STR_P_MAXMEMAVAIL				BIGSTRING ("\x0B" "maxMemAvail")
#define STR_P_MINMEMAVAIL				BIGSTRING ("\x0B" "minMemAvail")
#define STR_P_CONNECTION				BIGSTRING ("\x0A" "Connection")
#define STR_P_PARAMTABLE 				BIGSTRING ("\x0A" "paramTable")
#define STR_P_SEARCHARGS				BIGSTRING ("\x0A" "searchArgs")
#define STR_P_FIRSTLINE					BIGSTRING ("\x09" "firstLine")
#define STR_P_CHUNKSIZE					BIGSTRING ("\x09" "chunksize")
#define STR_P_HTTP11					BIGSTRING ("\x09" "HTTP/1.1 ")
#define STR_P_CONDITION					BIGSTRING ("\x09" "condition")
#define STR_P_RESPONDER					BIGSTRING ("\x09" "responder")
#define STR_P_PATHARGS					BIGSTRING ("\x08" "pathArgs")
#define STR_P_ADRTABLE					BIGSTRING ("\x08" "adrTable")
#define STR_P_FLPARAMS					BIGSTRING ("\x08" "flParams")
#define STR_P_ENABLED					BIGSTRING ("\x07" "enabled")
#define STR_P_REQUEST					BIGSTRING ("\x07" "request")
#define STR_P_TIMEOUT					BIGSTRING ("\x07" "timeout")
#define STR_P_COOKIES					BIGSTRING ("\x07" "cookies")
#define STR_P_METHODS					BIGSTRING ("\x07" "methods")
#define STR_P_FLLEGAL					BIGSTRING ("\x07" "flLegal")
#define STR_P_FLCLOSE					BIGSTRING ("\x07" "flClose")
#define STR_P_NOWAIT					BIGSTRING ("\x06" "noWait")
#define STR_P_METHOD					BIGSTRING ("\x06" "method")
#define STR_P_EXPECT					BIGSTRING ("\x06" "Expect")
#define STR_P_STREAM					BIGSTRING ("\x06" "stream")
#define STR_P_REFCON					BIGSTRING ("\x06" "refcon")
#define STR_P_CLIENT					BIGSTRING ("\x06" "client")
#define STR_P_COOKIE					BIGSTRING ("\x06" "Cookie")
#define STR_P_SERVER					BIGSTRING ("\x06" "Server")
#define STR_P_UNKNOWN					BIGSTRING ("\x07" "UNKNOWN")
#define STR_P_THREAD					BIGSTRING ("\x06" "thread")
#define STR_P_DAEMON					BIGSTRING ("\x06" "daemon")
#define STR_P_ALLOW						BIGSTRING ("\x05" "ALLOW")
#define STR_P_CLOSE						BIGSTRING ("\x05" "close")
#define STR_P_READY						BIGSTRING ("\x05" "ready")
#define STR_P_STATS						BIGSTRING ("\x05" "stats")
#define STR_P_COUNT						BIGSTRING ("\x05" "count")
#define STR_P_CODE						BIGSTRING ("\x04" "code")
#define STR_P_HITS						BIGSTRING ("\x04" "hits")
#define STR_P_HOST						BIGSTRING ("\x04" "host")
#define STR_P_PORT						BIGSTRING ("\x04" "port")
#define STR_P_PATH						BIGSTRING ("\x04" "path")
#define STR_P_DATE						BIGSTRING ("\x04" "Date")
#define STR_P_CRLFCRLF					BIGSTRING ("\x04" "\r\n\r\n")
#define STR_P_ANY						BIGSTRING ("\x03" "any")
#define STR_P_URI						BIGSTRING ("\x03" "URI")
#define STR_P_DOLLAR_ENCODED			BIGSTRING ("\x03" "%24")
#define STR_P_CRLF						BIGSTRING ("\x02" "\r\n")
#define STR_P_COLON						BIGSTRING ("\x02" ": ")
#define STR_P_DOLLAR					BIGSTRING ("\x01" "$")
#define STR_P_SPACE						BIGSTRING ("\x01" " ")
#define STR_P_EMPTY						BIGSTRING ("\x00")
#define STR_P_USERWEBSERVERSTRING			BIGSTRING ( "\x11" "headerFieldServer" )

#define STR_STATUSCONTINUE				"HTTP/1.1 100 CONTINUE\r\n\r\n"
#define sizestatuscontinue				25

typedef enum tyhtmlverbtoken { /*verbs that are processed by langhtml.c*/
	
	processmacrosfunc,
	
	urldecodefunc,
	
	urlencodefunc,
	
	parseargsfunc,
	
	iso8859encodefunc,

	getgifheightwidthfunc,

	getjpegheightwidthfunc,
	
	buildpagetablefunc,
	
	refglossaryfunc,
	
	getpreffunc,
	
	getonedirectivefunc,
	
	rundirectivefunc,
	
	rundirectivesfunc,
	
	runoutlinedirectivesfunc,
	
	cleanforexportfunc,
	
	normalizenamefunc,
	
	glossarypatcherfunc,
	
	expandurlsfunc,
	
	traversalskipfunc,
	
	getpagetableaddressfunc,

	htmlneutermacrosfunc,

	htmlneutertagsfunc,
	
	htmlcalendardrawfunc,

	/* searchengine */

	stripmarkupfunc,
	
	deindexpagefunc,
	
	indexpagefunc,
	
	cleanindexfunc,
	
	unionmatchesfunc,
	
	/* mainResponder.calendar */
	
	mrcalendargetaddressdayfunc,
	
	mrcalendargetdayaddressfunc,
	
	mrcalendargetfirstaddressfunc,
	
	mrcalendargetfirstdayfunc,
	
	mrcalendargetlastaddressfunc,
	
	mrcalendargetlastdayfunc,
	
	mrcalendargetmostrecentaddressfunc,
	
	mrcalendargetmostrecentdayfunc,
	
	mrcalendargetnextaddressfunc,
	
	mrcalendargetnextdayfunc,
	
	mrcalendarnavigatefunc,

	/* webserver */

	webserverserverfunc,

	webserverdispatchfunc,

	webserverparseheadersfunc,

	webserverparsecookiesfunc,

	webserverbuildresponsefunc,

	webserverbuilderrorpagefunc,

	webservergetserverstringfunc,

	/* inetd */

	inetdsupervisorfunc,

	cthtmlverbs
	} tyhtmlverbtoken;

static bigstring bsdebug;

static boolean flpagemillfile = false;


//static ptraddress callbackscript = nil;

#pragma pack(2)
typedef struct typrocessmacrosinfo {

	hdlhashtable hpagetable;
	hdlhashtable hstandardmacros;
	hdlhashtable huserprefs;
	hdlhashtable husermacros;
	hdlhashtable htools;
	hdlhashtable hmacrocontext;
	
	boolean flprocessmacros;
	boolean flexpandglossaryitems;
	boolean flautoparagraphs;
	boolean flactiveurls;
	boolean flclaycompatibility;
	boolean flisofilter;
	} typrocessmacrosinfo, *ptrprocessmacrosinfo;
#pragma options align=reset



#pragma mark === processhtmlmacros ===

static boolean htmlcallbackerror (bigstring bsmsg, ptrvoid perrorstring) {
	
	/*
	4.0.2b1 dmb: this error trapping isn't bullet proof, but it should 
	be fine since once an error occurs, script execution quickly unwinds, 
	with no thread yielding.
	*/
	
	copystring (bsmsg, (ptrstring) perrorstring);
	
	return (true);
	} /*htmlcallbackerror*/



static boolean strongcoercetostring (tyvaluerecord *val) {

	boolean fl;
	
	flcoerceexternaltostring = true;
	
	fl = coercetostring (val);
	
	flcoerceexternaltostring = false;
	
	return (fl);
	} /*strongcoercetostring*/


static boolean langpushwithtable (hdlhashtable ht, hdlhashtable hwith) {

	/*
	5.0.2b14 dmb: sucked out of evaluatewith, comes in at a different level
	*/
	
	short n = (**ht).ctwithvalues;
	bigstring bs;
	tyvaluerecord valwith;
	
	if (n == 7) { /*maximum value of ctwithvalues*/
		
		langlongparamerror (toomanywithtableserror, n);
		
		return (false);
		}
	
	if (!setaddressvalue (hwith, zerostring, &valwith))
		return (false);
	
	langgetwithvaluename (++n, bs);
	
	(**ht).ctwithvalues = n; /*optimization for langfindsymbol*/
	
	if (!hashtableassign (ht, bs, valwith)) {
		
		disposevaluerecord (valwith, false);
		
		return (false);
		}

	exemptfromtmpstack (&valwith); /*its in the local table now*/
	
	return (true);
	} /*langpushwithtable*/


static boolean htmlgetdefaultpagetable (hdlhashtable *hpagetable) {
	
	/*
	5.0.2 dmb: super-fast lookup of "websites.[#data]"
	*/
	
	return (langfastaddresstotable (roottable, str_websitesdata, hpagetable));
	} /*htmlgetdefaultpagetable*/


static boolean getoptionalpagetablevalue (hdltreenode hp1, short n, hdlhashtable *hpagetable) {

	if (langgetparamcount (hp1) >= n) {
		
		flnextparamislast = true;
		
		if (!gettablevalue (hp1, n, hpagetable))
			return (false);
		}
	else {
		if (!htmlgetdefaultpagetable (hpagetable))
			return (false);
		}
	
	return (true);
	} /*getoptionalpagetablevalue*/



		
static boolean htmlgetprefstable (hdlhashtable *huserprefs) {
	
	/*
	5.0.2 dmb: super-fast lookup of "user.html.prefs"
	*/
	
	return (langfastaddresstotable (roottable, str_userhtmlprefs, huserprefs));
	} /*htmlgetprefstable*/


static boolean htmlgetpref (typrocessmacrosinfo *pmi, bigstring pref, tyvaluerecord *val) {

	/*
	5.0.2 dmb: pulled into kernel. we return val on the tmp stack
	
