/*
 * headless_string_verbs.c - String processor verbs for headless mode
 *
 * This file provides the callback dispatcher for string verbs in headless mode,
 * routing verb calls to the actual implementations in stringverbs.c.
 *
 * Created: 2025-12-31 - String verb bindings (Phase 1 quick wins)
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "logging.h"

/* Token enum for all verbs in the string processor - MUST match tystringtoken in stringverbs.c */
enum {
	deletefunc = 0,
	insertfunc = 1,
	popleadingfunc = 2,
	poptrailingfunc = 3,
	trimwhitespacefunc = 4,
	popsuffixfunc = 5,
	hassuffixfunc = 6,
	midfunc = 7,
	nthcharfunc = 8,
	nthfieldfunc = 9,
	countfieldsfunc = 10,
	setwordcharfunc = 11,
	getwordcharfunc = 12,
	firstwordfunc = 13,
	lastwordfunc = 14,
	nthwordfunc = 15,
	countwordsfunc = 16,
	commentdeletefunc = 17,
	firstsentencefunc = 18,
	patternmatchfunc = 19,
	hexfunc = 20,
	timestringfunc = 21,
	datestringfunc = 22,
	uppercasefunc = 23,
	lowercasefunc = 24,
	filledstringfunc = 25,
	addcommasfunc = 26,
	replacefunc = 27,
	replaceallfunc = 28,
	lengthfunc = 29,
	isalphafunc = 30,
	isnumericfunc = 31,
	ispunctuationfunc = 32,
	processmacrosfunc = 33,
	urldecodefunc = 34,
	urlencodefunc = 35,
	parseargsfunc = 36,
	iso8859encodefunc = 37,
	getgifheightwidthfunc = 38,
	getjpegheightwidthfunc = 39,
	wrapfunc = 40,
	davenetmassagerfunc = 41,
	parseaddressfunc = 42,
	dropnonalphasfunc = 43,
	padwithzerosfunc = 44,
	ellipsizefunc = 45,
	innercasefunc = 46,
	urlsplitfunc = 47,
	hashmd5func = 48,
	latintomacfunc = 49,
	mactolatinfunc = 50,
	utf16toansifunc = 51,
	utf8toansifunc = 52,
	ansitoutf8func = 53,
	ansitoutf16func = 54,
	multiplereplaceallfunc = 55,
	macromantoutf8func = 56,
	utf8tomacromanfunc = 57,
	convertcharsetfunc = 58,
	ischarsetavailablefunc = 59
};

/* Forward declaration of the actual implementation in stringverbs.c */
extern boolean stringfunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror);

static boolean string_valueproc(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned,
                                bigstring bserror) {
	/*
	 * Dispatcher for string verbs in headless mode.
	 * Simply forwards all calls to the actual implementation in stringverbs.c
	 */
	boolean result;

	log_debug(LOG_COMP_LANG, "string_valueproc: ENTRY token=%d hparam1=%p vreturned=%p bserror=%p",
	          token, (void*)hparam1, (void*)vreturned, (void*)bserror);

	result = stringfunctionvalue(token, hparam1, vreturned, bserror);

	if (bserror && bserror[0] > 0) {
		char errmsg[256];
		copyptocstring(bserror, errmsg);
		log_debug(LOG_COMP_LANG, "string_valueproc: EXIT token=%d result=%d bserror='%s'",
		          token, result, errmsg);
	} else {
		log_debug(LOG_COMP_LANG, "string_valueproc: EXIT token=%d result=%d bserror=<empty>",
		          token, result);
	}

	return result;
}

/* Exported callback used by headless kernel verb bootstrap */
boolean headless_string_verbs_callback(short token, hdltreenode hparam1,
                                      tyvaluerecord *vreturned, bigstring bserror) {
	return string_valueproc(token, hparam1, vreturned, bserror);
}

/* Headless-specific string verb initialization function
 *
 * This replaces stringinitverbs() for headless mode, registering the string processor
 * with headless_string_verbs_callback instead of the windowed stringfunctionvalue callback.
 *
 * Follows the same pattern as other headless processor init functions (e.g., tableinitverbs).
 *
 * Returns: true if string processor was successfully registered, false otherwise
 */
boolean stringinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
	extern boolean langaddkeyword(bigstring, short);
	extern boolean pushhashtable(hdlhashtable);
	extern boolean pophashtable(void);

	log_debug(LOG_COMP_LANG, "stringinitverbs: registering headless string processor");

	copystring(BIGSTRING("\006string"), bsname);

	if (!newfunctionprocessor(bsname, &headless_string_verbs_callback, true, &htable))
		return false;

	pushhashtable(htable);

	/* Register all string verbs */
	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(BIGSTRING("\006delete"), deletefunc);
	ADD_VERB(BIGSTRING("\006insert"), insertfunc);
	ADD_VERB(BIGSTRING("\012popleading"), popleadingfunc);
	ADD_VERB(BIGSTRING("\013poptrailing"), poptrailingfunc);
	ADD_VERB(BIGSTRING("\016trimwhitespace"), trimwhitespacefunc);
	ADD_VERB(BIGSTRING("\011popsuffix"), popsuffixfunc);
	ADD_VERB(BIGSTRING("\011hassuffix"), hassuffixfunc);
	ADD_VERB(BIGSTRING("\003mid"), midfunc);
	ADD_VERB(BIGSTRING("\014nthcharacter"), nthcharfunc);
	ADD_VERB(BIGSTRING("\010nthfield"), nthfieldfunc);
	ADD_VERB(BIGSTRING("\013countfields"), countfieldsfunc);
	ADD_VERB(BIGSTRING("\013setwordchar"), setwordcharfunc);
	ADD_VERB(BIGSTRING("\013getwordchar"), getwordcharfunc);
	ADD_VERB(BIGSTRING("\011firstword"), firstwordfunc);
	ADD_VERB(BIGSTRING("\010lastword"), lastwordfunc);
	ADD_VERB(BIGSTRING("\007nthword"), nthwordfunc);
	ADD_VERB(BIGSTRING("\012countwords"), countwordsfunc);
	ADD_VERB(BIGSTRING("\015commentdelete"), commentdeletefunc);
	ADD_VERB(BIGSTRING("\016firstsentence"), firstsentencefunc);
	ADD_VERB(BIGSTRING("\014patternmatch"), patternmatchfunc);
	ADD_VERB(BIGSTRING("\003hex"), hexfunc);
	ADD_VERB(BIGSTRING("\012timestring"), timestringfunc);
	ADD_VERB(BIGSTRING("\012datestring"), datestringfunc);
	ADD_VERB(BIGSTRING("\005upper"), uppercasefunc);
	ADD_VERB(BIGSTRING("\005lower"), lowercasefunc);
	ADD_VERB(BIGSTRING("\014filledstring"), filledstringfunc);
	ADD_VERB(BIGSTRING("\011addcommas"), addcommasfunc);
	ADD_VERB(BIGSTRING("\007replace"), replacefunc);
	ADD_VERB(BIGSTRING("\012replaceall"), replaceallfunc);
	ADD_VERB(BIGSTRING("\006length"), lengthfunc);
	ADD_VERB(BIGSTRING("\007isalpha"), isalphafunc);
	ADD_VERB(BIGSTRING("\011isnumeric"), isnumericfunc);
	ADD_VERB(BIGSTRING("\016ispunctuation"), ispunctuationfunc);
	ADD_VERB(BIGSTRING("\020processhtmlmacros"), processmacrosfunc);
	ADD_VERB(BIGSTRING("\011urldecode"), urldecodefunc);
	ADD_VERB(BIGSTRING("\011urlencode"), urlencodefunc);
	ADD_VERB(BIGSTRING("\016parsehttpargs"), parseargsfunc);
	ADD_VERB(BIGSTRING("\015iso8859encode"), iso8859encodefunc);
	ADD_VERB(BIGSTRING("\020getgifheightwidth"), getgifheightwidthfunc);
	ADD_VERB(BIGSTRING("\021getjpegheightwidth"), getjpegheightwidthfunc);
	ADD_VERB(BIGSTRING("\004wrap"), wrapfunc);
	ADD_VERB(BIGSTRING("\017davenetmassager"), davenetmassagerfunc);
	ADD_VERB(BIGSTRING("\014parseaddress"), parseaddressfunc);
	ADD_VERB(BIGSTRING("\015dropnonalphas"), dropnonalphasfunc);
	ADD_VERB(BIGSTRING("\014padwithzeros"), padwithzerosfunc);
	ADD_VERB(BIGSTRING("\011ellipsize"), ellipsizefunc);
	ADD_VERB(BIGSTRING("\014innercasename"), innercasefunc);
	ADD_VERB(BIGSTRING("\010urlsplit"), urlsplitfunc);
	ADD_VERB(BIGSTRING("\007hashmd5"), hashmd5func);
	ADD_VERB(BIGSTRING("\012latintomac"), latintomacfunc);
	ADD_VERB(BIGSTRING("\012mactolatin"), mactolatinfunc);
	ADD_VERB(BIGSTRING("\013utf16toansi"), utf16toansifunc);
	ADD_VERB(BIGSTRING("\012utf8toansi"), utf8toansifunc);
	ADD_VERB(BIGSTRING("\012ansitoutf8"), ansitoutf8func);
	ADD_VERB(BIGSTRING("\013ansitoutf16"), ansitoutf16func);
	ADD_VERB(BIGSTRING("\021multiplereplaceall"), multiplereplaceallfunc);
	ADD_VERB(BIGSTRING("\017macromantoutf8"), macromantoutf8func);
	ADD_VERB(BIGSTRING("\017utf8tomacroman"), utf8tomacromanfunc);
	ADD_VERB(BIGSTRING("\016convertcharset"), convertcharsetfunc);
	ADD_VERB(BIGSTRING("\022ischarsetavailable"), ischarsetavailablefunc);

	pophashtable();

	log_debug(LOG_COMP_LANG, "stringinitverbs: string processor registered successfully");

	return true;
}
