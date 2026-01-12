/*
 * kernel_verbs_headless.c - Kernel verb registration for headless mode
 *
 * MAINTAINED SOURCE CODE - This file is manually maintained for headless builds.
 *
 * Originally generated from Common/resources/Win32/kernelverbs.rc, this file
 * has been converted to maintained source code to support headless mode with
 * modular verb implementations.
 *
 * ARCHITECTURE:
 * - Each EFP (External Function Processor) registers a verb family (e.g., "file", "frontier")
 * - Instead of routing all verbs through langfunctionvalue's 3600-line switch statement,
 *   headless mode uses processor-specific callbacks (e.g., headless_file_verbs_callback)
 * - This enables modular verb implementation files in tests/ directory
 *
 * MODIFIED PROCESSORS:
 * - init_efp_1001 (table): Uses headless_table_verbs_callback
 * - init_efp_1007 (file): Uses headless_file_verbs_callback
 * - init_efp_1016 (frontier): Uses headless_frontier_verbs_callback
 *
 * OTHER PROCESSORS: Currently use langfunctionvalue, but can be migrated to modular
 * callbacks as needed when implementing additional verbs for headless mode.
 */

#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langinternal.h"
#include "kernelverbdefs.h"

#ifdef FRONTIER_HEADLESS

/* Forward declarations */
extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
extern boolean langaddkeyword(ptrstring, short);
extern boolean pushhashtable(hdlhashtable);
extern boolean pophashtable(void);

/* Headless-specific verb implementations */
extern boolean headless_file_verbs_callback(short, hdltreenode, tyvaluerecord*, bigstring);
extern boolean headless_frontier_verbs_callback(short, hdltreenode, tyvaluerecord*, bigstring);
extern boolean headless_table_verbs_callback(short, hdltreenode, tyvaluerecord*, bigstring);

static boolean init_efp_1000(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: op (45 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\002op"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\013getlinetext"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005level"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011countsubs"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014countsummits"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\002go"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013firstsummit"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006expand"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010collapse"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014subsexpanded"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006insert"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004find"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004sort"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setlinetext"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005reorg"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007promote"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006demote"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005hoist"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007dehoist"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012deletesubs"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012deleteline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013tabkeyreorg"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016flatcursorkeys"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getdisplay"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setdisplay"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getcursor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setcursor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getrefcon"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setrefcon"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021getexpansionstate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021setexpansionstate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getscrollstate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016setscrollstate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getsuboutline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015insertoutline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setmodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getselection"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getheadnumber"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010visitall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\026getselectedsuboutlines"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014xmltooutline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014outlinetoxml"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021sethtmlformatting"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021gethtmlformatting"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setdynamic"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getdynamic"), ixverb++))
        return false;
    pophashtable();

    /* Processor: opattributes (5 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\014opattributes"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\010addgroup"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006getall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006getone"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011makeempty"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006setone"), ixverb++))
        return false;
    pophashtable();

    /* Processor: script (13 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006script"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\007compile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011uncompile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getcode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getlanguage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setlanguage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013makecomment"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011uncomment"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011iscomment"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getbreakpoint"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setbreakpoint"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017clearbreakpoint"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014startprofile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013stopprofile"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1001(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    (void) valuecallback; /* Unused - we use headless_table_verbs_callback instead */

    /* Processor: table (19 verbs) - uses modular headless callback */
    if (!newfunctionprocessor(BIGSTRING("\005table"), &headless_table_verbs_callback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004move"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004copy"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006rename"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015moveandrename"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006assign"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010validate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006sortby"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getcursor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getselection"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\002go"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004goto"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010gotoname"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010jettison"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011packtable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012emptytable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022getdisplaysettings"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022setdisplaysettings"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getsortorder"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021countvisiblerows"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1002(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: menu (14 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004menu"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\012zoomscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014buildmenubar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014clearmenubar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013isinstalled"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007install"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006remove"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016addmenucommand"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021deletemenucommand"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012addsubmenu"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015deletesubmenu"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getcommandkey"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setcommandkey"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1003(langvaluecallback valuecallback) {
    hdlhashtable htable;

    /* Processor: wp (0 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\002wp"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    pophashtable();

    return true;
}

static boolean init_efp_1004(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: pict (4 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004pict"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\016scheduleupdate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013expressions"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getpicture"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setpicture"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1005(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: lang (58 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004lang"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\013scripterror"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003new"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006delete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004edit"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013timecreated"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014timemodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016settimecreated"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017settimemodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007boolean"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004char"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005short"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004long"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004date"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011direction"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007string4"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006string"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015displaystring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007address"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006binary"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getbinarytype"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setbinarytype"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005point"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004rect"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003rgb"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007pattern"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005fixed"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006single"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006double"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010filespec"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005alias"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004list"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006record"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004enum"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010memavail"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013flushmemory"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006random"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010evaluate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016evaluatethread"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015rollbeachball"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003abs"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017seteventtimeout"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\025seteventtransactionid"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023seteventinteraction"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021geteventattribute"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017coerceappleitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020getapplelistitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020putapplelistitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023countapplelistitems"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013systemevent"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010DDEevent"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020transactionEvent"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003msg"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010callxcmd"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007calldll"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012packwindow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014unpackwindow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012callscript"), ixverb++))
        return false;
    pophashtable();

    /* Processor: clock (7 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\005clock"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003now"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010sleepfor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005ticks"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014milliseconds"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013waitseconds"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015waitsixtieths"), ixverb++))
        return false;
    pophashtable();

    /* Processor: date (30 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004date"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014abbrevstring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011dayofweek"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013daysinmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011daystring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014firstofmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013lastofmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012longstring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011nextmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010nextweek"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010nextyear"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011prevmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010prevweek"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010prevyear"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013shortstring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010tomorrow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014weeksinmonth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011yesterday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022getcurrenttimezone"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021netstandardstring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015monthtostring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021dayofweektostring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017versionlessthan"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003day"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005month"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004year"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004hour"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006minute"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007seconds"), ixverb++))
        return false;
    pophashtable();

    /* Processor: dialog (19 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006dialog"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\005alert"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003run"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013runmodeless"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007runcard"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014runmodalcard"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013ismodalcard"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023setmodalcardtimeout"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010setvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setitemenable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010showitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010hideitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006twoway"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010threeway"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003ask"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006getint"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006notify"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getuserinfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getpassword"), ixverb++))
        return false;
    pophashtable();

    /* Processor: kb (4 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\002kb"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\011optionkey"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006cmdkey"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010shiftkey"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012controlkey"), ixverb++))
        return false;
    pophashtable();

    /* Processor: mouse (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\005mouse"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006button"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010location"), ixverb++))
        return false;
    pophashtable();

    /* Processor: point (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\005point"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    pophashtable();

    /* Processor: rectangle (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\011rectangle"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    pophashtable();

    /* Processor: rgb (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003rgb"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    pophashtable();

    /* Processor: speaker (3 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\007speaker"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004beep"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005sound"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016playnamedsound"), ixverb++))
        return false;
    pophashtable();

    /* Processor: target (3 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006target"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005clear"), ixverb++))
        return false;
    pophashtable();

    /* Processor: bit (8 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003bit"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003set"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005clear"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012logicaland"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011logicalor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012logicalxor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011shiftleft"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012shiftright"), ixverb++))
        return false;
    pophashtable();

    /* Processor: semaphore (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\011semaphore"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004lock"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006unlock"), ixverb++))
        return false;
    pophashtable();

    /* Processor: base64 (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006base64"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006encode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006decode"), ixverb++))
        return false;
    pophashtable();

    /* Processor: tcp (23 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003tcp"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\015addressdecode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015addressencode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015addresstoname"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015nametoaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011myaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013abortstream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013closestream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013closelisten"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016openaddrstream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016opennamestream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012readstream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013writestream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014listenstream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014statusstream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getpeeraddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getpeerport"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023writestringtostream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021writefiletostream"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017readstreamuntil"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017readstreambytes"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\025readstreamuntilclosed"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getstats"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020countconnections"), ixverb++))
        return false;
    pophashtable();

    /* Processor: dll (4 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003dll"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004call"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004load"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006unload"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010isloaded"), ixverb++))
        return false;
    pophashtable();

    /* Processor: python (1 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006python"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\010doscript"), ixverb++))
        return false;
    pophashtable();

    /* Processor: htmlcontrol (8 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\013htmlcontrol"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004back"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007forward"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007refresh"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004home"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004stop"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010navigate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011isoffline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setoffline"), ixverb++))
        return false;
    pophashtable();

    /* Processor: statusbar (5 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\011statusbar"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003msg"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setsections"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getsections"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getsectionone"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getmessage"), ixverb++))
        return false;
    pophashtable();

    /* Processor: winregistry (4 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\013winregistry"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006delete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004read"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007gettype"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005write"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1006(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: string (60 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006string"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006delete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006insert"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012popleading"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013poptrailing"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016trimwhitespace"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011popsuffix"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011hassuffix"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003mid"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007nthchar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010nthfield"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013countfields"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setwordchar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getwordchar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011firstword"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010lastword"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007nthword"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012countwords"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015commentdelete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015firstsentence"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014patternmatch"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003hex"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012timestring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012datestring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005upper"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005lower"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014filledstring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011addcommas"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007replace"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012replaceall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006length"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007isalpha"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011isnumeric"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015ispunctuation"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021processhtmlmacros"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011urldecode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011urlencode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015parsehttpargs"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015iso8859encode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021getgifheightwidth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022getjpegheightwidth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004wrap"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017davenetmassager"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014parseaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015dropnonalphas"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014padwithzeros"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011ellipsize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015innercasename"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010urlsplit"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007hashMD5"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012latintomac"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012mactolatin"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013utf16toansi"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012utf8toansi"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012ansitoutf8"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013ansitoutf16"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022multiplereplaceall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016macromantoutf8"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016utf8tomacroman"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016convertcharset"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022ischarsetavailable"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1007(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: file (86 verbs) */
    /* MANUAL PATCH: Use headless-specific callback instead of langfunctionvalue */
    (void)valuecallback;  /* Suppress unused parameter warning */
    if (!newfunctionprocessor(BIGSTRING("\004file"), &headless_file_verbs_callback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\007created"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010modified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004type"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007creator"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setcreated"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setmodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007settype"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setcreator"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010isfolder"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010isvolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010islocked"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004lock"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006unlock"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004copy"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014copydatafork"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020copyresourcefork"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006delete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006rename"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006exists"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004size"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010fullpath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getpath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007setpath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014filefrompath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016folderfrompath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023getsystemfolderpath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\024getspecialfolderpath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003new"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011newfolder"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010newalias"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getfiledialog"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015putfiledialog"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getfolderdialog"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getdiskdialog"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012geticonpos"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012seticonpos"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getversion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setversion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getfullversion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016setfullversion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getcomment"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setcomment"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getlabel"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010setlabel"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017findapplication"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006isbusy"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011hasbundle"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setbundle"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007isalias"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011isvisible"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setvisible"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013followalias"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004move"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005eject"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013isejectable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021freespaceonvolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012volumesize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017volumeblocksize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015filesonvolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017foldersonvolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015unmountvolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021mountservervolume"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012findinfile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012countlines"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004open"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011endoffile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014setendoffile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getendoffile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setposition"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getposition"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010readline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011writeline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004read"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005write"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007compare"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016writewholefile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getpathchar"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\027freespaceonvolumedouble"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020volumesizedouble"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getmp3info"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015readwholefile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getLabelIndex"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setLabelIndex"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getLabelNames"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getPosixPath"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1008(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: rez (15 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003rez"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\013getresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013putresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020getnamedresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020putnamedresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015countrestypes"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getnthrestype"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016countresources"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getnthresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getnthresinfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016resourceexists"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023namedresourceexists"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016deleteresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023deletenamedresource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\025getresourceattributes"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\025setresourceattributes"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1009(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: window (31 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006window"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006isopen"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004open"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007isfront"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014bringtofront"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012sendtoback"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011frontmost"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004next"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011isvisible"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004show"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004hide"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006update"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014ismenuscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getposition"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setposition"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getsize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007setsize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004zoom"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014runselection"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006scroll"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003msg"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007dbstats"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013quickscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012ismodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setmodified"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010gettitle"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010settitle"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005about"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getfile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012isreadonly"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016setquickscript"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1010(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: search (6 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006search"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\005reset"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010findnext"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007replace"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012replaceall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016findtextdialog"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021replacetextdialog"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1011(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: filemenu (10 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\010filemenu"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003new"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004open"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010closeall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004save"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010savecopy"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006revert"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005print"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004quit"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006saveas"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1012(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: editmenu (16 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\010editmenu"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004undo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003cut"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004copy"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005paste"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005clear"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011selectall"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getfont"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getfontsize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007setfont"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013setfontsize"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011plaintext"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007setbold"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setitalic"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014setunderline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012setoutline"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011setshadow"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1013(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: sys (16 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003sys"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\011osversion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012systemtask"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015browsenetwork"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014appisrunning"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014frontmostapp"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017bringapptofront"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011countapps"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getnthapp"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getapppath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010memavail"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007machine"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\002os"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\026getenvironmentvariable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\026setenvironmentvariable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020unixshellcommand"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017winshellcommand"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1014(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: launch (5 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006launch"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\011applemenu"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013application"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017appwithdocument"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010resource"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010anything"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1015(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: clipboard (2 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\011clipboard"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003get"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003put"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1016(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: frontier (14 verbs) */
    /* MANUAL PATCH: Use headless-specific callback instead of langfunctionvalue */
    (void)valuecallback;  /* Suppress unused parameter warning */
    if (!newfunctionprocessor(BIGSTRING("\010frontier"), &headless_frontier_verbs_callback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\016getprogrampath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getfilepath"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014enableagents"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016requesttofront"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011isruntime"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014countthreads"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011ispowerpc"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015reclaimmemory"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007version"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011hashstats"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020gethashloopcount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017hideapplication"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023isvalidserialnumber"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017showapplication"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1018(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: thread (17 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006thread"), valuecallback, true, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006exists"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010evaluate"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012callscript"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getcurrentid"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getcount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getnthid"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005sleep"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010sleepfor"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012sleepticks"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012issleeping"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004wake"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004kill"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014gettimeslice"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014settimeslice"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023getdefaulttimeslice"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023setdefaulttimeslice"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getstats"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1017(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: mainwindow (7 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\012mainwindow"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\010showflag"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010hideflag"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011showpopup"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011hidepopup"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013showbuttons"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013hidebuttons"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017showserverstats"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1019(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: db (13 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\002db"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003new"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004open"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004save"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007defined"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010setvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006delete"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010newTable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007isTable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012countitems"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getnthitem"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getmoddate"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1020(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: xml (14 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\003xml"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\010addtable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010addvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007compile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011decompile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getaddresslist"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getattribute"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021getattributevalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010getvalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013valtostring"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\031frontiervaluetotaggedtext"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\025structtofrontiervalue"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getpathaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\024converttodisplayname"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1021(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: html (23 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004html"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\015processmacros"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011urldecode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011urlencode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015parsehttpargs"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015iso8859encode"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\021getgifheightwidth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022getjpegheightwidth"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016buildpagetable"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013refglossary"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007getpref"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getonedirective"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014rundirective"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015rundirectives"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\024runoutlinedirectives"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016cleanforexport"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015normalizename"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017glossarypatcher"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012expandurls"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015traversalskip"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023getpagetableaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014neutermacros"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012neutertags"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014drawcalendar"), ixverb++))
        return false;
    pophashtable();

    /* Processor: searchengine (5 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\014searchengine"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\013stripmarkup"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013deindexpage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011indexpage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012cleanindex"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014mergeresults"), ixverb++))
        return false;
    pophashtable();

    /* Processor: mrcalendar (11 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\012mrcalendar"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\015getaddressday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getdayaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getfirstaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getfirstday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getlastaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getlastday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\024getmostrecentaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020getmostrecentday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getnextaddress"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012getnextday"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010navigate"), ixverb++))
        return false;
    pophashtable();

    /* Processor: webserver (7 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\011webserver"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\006server"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\010dispatch"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014parseheaders"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014parsecookies"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015buildresponse"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016builderrorpage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getserverstring"), ixverb++))
        return false;
    pophashtable();

    /* Processor: inetd (1 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\005inetd"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\012supervisor"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1023(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: re (10 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\002re"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\007compile"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005match"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007replace"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007extract"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005split"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004join"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005visit"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004grep"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getpatterninfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006expand"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1024(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: math (3 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\004math"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\003min"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003max"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\004sqrt"), ixverb++))
        return false;
    pophashtable();

    return true;
}

/* SKIP init_efp_1025 (crypt processor) - Already registered by cryptinitverbs() in langcrypt.c
   with the correct cryptfunctionvalue callback. Registering here would overwrite that with
   langfunctionvalue, breaking the verb dispatch mechanism. */
/* static boolean init_efp_1025(langvaluecallback valuecallback) {
    ...
} */

static boolean init_efp_1026(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: sqlite (17 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\006sqlite"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004open"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014compileQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012clearQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012resetQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011stepQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getColumnCount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getColumnType"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getColumnInt"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getColumnDouble"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getColumnText"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getColumnName"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\011getColumn"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006getRow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getErrorMessage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015setColumnBlob"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\022getLastInsertRowId"), ixverb++))
        return false;
    pophashtable();

    return true;
}

static boolean init_efp_1027(langvaluecallback valuecallback) {
    hdlhashtable htable;
    short ixverb = 0;

    /* Processor: mysql (27 verbs) */
    if (!newfunctionprocessor(BIGSTRING("\005mysql"), valuecallback, false, &htable))
        return false;

    pushhashtable(htable);
    if (!langaddkeyword(BIGSTRING("\004init"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\003end"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007connect"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014compileQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012clearQuery"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\006getRow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getErrorNumber"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getErrorMessage"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getClientInfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020getClientVersion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getHostInfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\020getServerVersion"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getProtocolInfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\015getServerInfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014getQueryInfo"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023getAffectedRowCount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\023getSelectedRowCount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016getColumnCount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\017getServerStatus"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\024getQueryWarningCount"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\012pingServer"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\007seekRow"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\016selectDatabase"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\013getSQLSTATE"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014escapeString"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\014isThreadSafe"), ixverb++))
        return false;
    if (!langaddkeyword(BIGSTRING("\005close"), ixverb++))
        return false;
    pophashtable();

    return true;
}

boolean headless_init_kernel_verbs(void) {
    extern boolean langfunctionvalue(short, hdltreenode, tyvaluerecord*, bigstring);

    if (!init_efp_1000(&langfunctionvalue))
        return false;

    if (!init_efp_1001(&langfunctionvalue))
        return false;

    if (!init_efp_1002(&langfunctionvalue))
        return false;

    if (!init_efp_1003(&langfunctionvalue))
        return false;

    if (!init_efp_1004(&langfunctionvalue))
        return false;

    /* SKIP init_efp_1005 (ID 1005: lang processor) - already registered by langinitverbs()
       with the correct lang_valueproc callback. Registering again here would overwrite
       that callback with langfunctionvalue, breaking the verb dispatch mechanism. */
    /* if (!init_efp_1005(&langfunctionvalue))
        return false; */

    if (!init_efp_1006(&langfunctionvalue))
        return false;

    if (!init_efp_1007(&langfunctionvalue))
        return false;

    if (!init_efp_1008(&langfunctionvalue))
        return false;

    if (!init_efp_1009(&langfunctionvalue))
        return false;

    if (!init_efp_1010(&langfunctionvalue))
        return false;

    if (!init_efp_1011(&langfunctionvalue))
        return false;

    if (!init_efp_1012(&langfunctionvalue))
        return false;

    if (!init_efp_1013(&langfunctionvalue))
        return false;

    if (!init_efp_1014(&langfunctionvalue))
        return false;

    if (!init_efp_1015(&langfunctionvalue))
        return false;

    if (!init_efp_1016(&langfunctionvalue))
        return false;

    if (!init_efp_1018(&langfunctionvalue))
        return false;

    if (!init_efp_1017(&langfunctionvalue))
        return false;

    if (!init_efp_1019(&langfunctionvalue))
        return false;

    if (!init_efp_1020(&langfunctionvalue))
        return false;

    if (!init_efp_1021(&langfunctionvalue))
        return false;

    if (!init_efp_1023(&langfunctionvalue))
        return false;

    if (!init_efp_1024(&langfunctionvalue))
        return false;

    /* SKIP init_efp_1025 (crypt) - handled by cryptinitverbs() */

    if (!init_efp_1026(&langfunctionvalue))
        return false;

    if (!init_efp_1027(&langfunctionvalue))
        return false;

    /* Register xml processor with custom callback */
    extern boolean xmlinitverbs(void);
    if (!xmlinitverbs())
        return false;

    return true;
}

#endif /* FRONTIER_HEADLESS */
