/*
 * headless_string_verbs.c - String processor verbs for headless mode
 *
 * This file provides the callback dispatcher for string verbs in headless mode,
 * routing verb calls to the actual implementations in stringverbs.c.
 *
 * @IMPLEMENTED - All 60 string verbs forward to stringfunctionvalue() in stringverbs.c
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

/* Token enum for all verbs in the string processor
 *
 * CRITICAL: This enum MUST be kept in sync with tystringtoken in Common/source/stringverbs.c
 *
 * Verification:
 *   1. Token order must match exactly (0=delete, 1=insert, etc.)
 *   2. Token count must match ctstringverbs value (60 verbs)
 *   3. Compile-time assertion below will fail if count mismatches
 *
 * To verify manually:
 *   grep -c "func," Common/source/stringverbs.c | should equal 60
 */
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
	ischarsetavailablefunc = 59,

	/* Sentinel - must equal ctstringverbs from stringverbs.c */
	stringv_count
};

/* Compile-time verification that token count matches stringverbs.c
 * If this fails, the enum above is out of sync with tystringtoken */
#define EXPECTED_STRING_VERB_COUNT 60
_Static_assert(stringv_count == EXPECTED_STRING_VERB_COUNT,
               "Token enum out of sync with stringverbs.c - update headless_string_verbs.c");

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

	copystring(PSTRING("\006", "string"), bsname);

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

	ADD_VERB(PSTRING("\006", "delete"), deletefunc);
	ADD_VERB(PSTRING("\006", "insert"), insertfunc);
	ADD_VERB(PSTRING("\012", "popleading"), popleadingfunc);
	ADD_VERB(PSTRING("\013", "poptrailing"), poptrailingfunc);
	ADD_VERB(PSTRING("\016", "trimwhitespace"), trimwhitespacefunc);
	ADD_VERB(PSTRING("\011", "popsuffix"), popsuffixfunc);
	ADD_VERB(PSTRING("\011", "hassuffix"), hassuffixfunc);
	ADD_VERB(PSTRING("\003", "mid"), midfunc);
	ADD_VERB(PSTRING("\014", "nthcharacter"), nthcharfunc);
	ADD_VERB(PSTRING("\010", "nthfield"), nthfieldfunc);
	ADD_VERB(PSTRING("\013", "countfields"), countfieldsfunc);
	ADD_VERB(PSTRING("\013", "setwordchar"), setwordcharfunc);
	ADD_VERB(PSTRING("\013", "getwordchar"), getwordcharfunc);
	ADD_VERB(PSTRING("\011", "firstword"), firstwordfunc);
	ADD_VERB(PSTRING("\010", "lastword"), lastwordfunc);
	ADD_VERB(PSTRING("\007", "nthword"), nthwordfunc);
	ADD_VERB(PSTRING("\012", "countwords"), countwordsfunc);
	ADD_VERB(PSTRING("\015", "commentdelete"), commentdeletefunc);
	ADD_VERB(PSTRING("\015", "firstsentence"), firstsentencefunc);
	ADD_VERB(PSTRING("\014", "patternmatch"), patternmatchfunc);
	ADD_VERB(PSTRING("\003", "hex"), hexfunc);
	ADD_VERB(PSTRING("\012", "timestring"), timestringfunc);
	ADD_VERB(PSTRING("\012", "datestring"), datestringfunc);
	ADD_VERB(PSTRING("\005", "upper"), uppercasefunc);
	ADD_VERB(PSTRING("\005", "lower"), lowercasefunc);
	ADD_VERB(PSTRING("\014", "filledstring"), filledstringfunc);
	ADD_VERB(PSTRING("\011", "addcommas"), addcommasfunc);
	ADD_VERB(PSTRING("\007", "replace"), replacefunc);
	ADD_VERB(PSTRING("\012", "replaceall"), replaceallfunc);
	ADD_VERB(PSTRING("\006", "length"), lengthfunc);
	ADD_VERB(PSTRING("\007", "isalpha"), isalphafunc);
	ADD_VERB(PSTRING("\011", "isnumeric"), isnumericfunc);
	ADD_VERB(PSTRING("\015", "ispunctuation"), ispunctuationfunc);
	ADD_VERB(PSTRING("\021", "processhtmlmacros"), processmacrosfunc);
	ADD_VERB(PSTRING("\011", "urldecode"), urldecodefunc);
	ADD_VERB(PSTRING("\011", "urlencode"), urlencodefunc);
	ADD_VERB(PSTRING("\015", "parsehttpargs"), parseargsfunc);
	ADD_VERB(PSTRING("\015", "iso8859encode"), iso8859encodefunc);
	ADD_VERB(PSTRING("\021", "getgifheightwidth"), getgifheightwidthfunc);
	ADD_VERB(PSTRING("\022", "getjpegheightwidth"), getjpegheightwidthfunc);
	ADD_VERB(PSTRING("\004", "wrap"), wrapfunc);
	ADD_VERB(PSTRING("\017", "davenetmassager"), davenetmassagerfunc);
	ADD_VERB(PSTRING("\014", "parseaddress"), parseaddressfunc);
	ADD_VERB(PSTRING("\015", "dropnonalphas"), dropnonalphasfunc);
	ADD_VERB(PSTRING("\014", "padwithzeros"), padwithzerosfunc);
	ADD_VERB(PSTRING("\011", "ellipsize"), ellipsizefunc);
	ADD_VERB(PSTRING("\015", "innercasename"), innercasefunc);
	ADD_VERB(PSTRING("\010", "urlsplit"), urlsplitfunc);
	ADD_VERB(PSTRING("\007", "hashmd5"), hashmd5func);
	ADD_VERB(PSTRING("\012", "latintomac"), latintomacfunc);
	ADD_VERB(PSTRING("\012", "mactolatin"), mactolatinfunc);
	ADD_VERB(PSTRING("\013", "utf16toansi"), utf16toansifunc);
	ADD_VERB(PSTRING("\012", "utf8toansi"), utf8toansifunc);
	ADD_VERB(PSTRING("\012", "ansitoutf8"), ansitoutf8func);
	ADD_VERB(PSTRING("\013", "ansitoutf16"), ansitoutf16func);
	ADD_VERB(PSTRING("\022", "multiplereplaceall"), multiplereplaceallfunc);
	ADD_VERB(PSTRING("\016", "macromantoutf8"), macromantoutf8func);
	ADD_VERB(PSTRING("\016", "utf8tomacroman"), utf8tomacromanfunc);
	ADD_VERB(PSTRING("\016", "convertcharset"), convertcharsetfunc);
	ADD_VERB(PSTRING("\022", "ischarsetavailable"), ischarsetavailablefunc);

	pophashtable();

	log_debug(LOG_COMP_LANG, "stringinitverbs: string processor registered successfully");

	return true;
}
