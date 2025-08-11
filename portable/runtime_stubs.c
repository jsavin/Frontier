/*
 * Minimal runtime stubs for portable Frontier
 * These provide minimal implementations of functions needed by the language engine
 */

#include "runtime_stubs.h"
#include "standard_portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// tyvaluerecord is now defined in runtime_stubs.h

// Basic memory and handle functions
void *newclearhandle(long size) { 
    return malloc(size); 
}

void *newfilledhandle(long size) { 
    void *h = malloc(size); 
    if (h) memset(h, 0, size); 
    return h; 
}

void enlargehandle(void *h, long s) { 
    (void)h; 
    (void)s; 
}

long gethandlesize(void *h) { 
    (void)h; 
    return 0; 
}

void *newheapstring(const char *str) { 
    if (!str) return NULL; 
    long len = strlen(str); 
    void *h = malloc(len + 1); 
    if (h) strcpy((char*)h, str); 
    return h; 
}

void *newtexthandle(const char *str) { 
    return newheapstring(str); 
}

// Hash table functions
void *newhashtable(void) { 
    return malloc(100); 
}

void disposehashtable(void *t) { 
    if (t) free(t); 
}

void chainhashtable(void *t) { 
    (void)t; 
}

void unchainhashtable(void *t) { 
    (void)t; 
}

// Stack operations
boolean pushhandle(void *h) { 
    (void)h; 
    return true; 
}

boolean pushstring(const char *s) { 
    (void)s; 
    return true; 
}

boolean pushint(long v) { 
    (void)v; 
    return true; 
}

// Error handling functions
void langerror(short e) { 
    printf("Language error: %d\n", e); 
}

void langerrorclear(void) { 
    // TODO: Implement error clearing
}

int langerrorenabled(void) { 
    return 1; 
}

void lang3paramerror(short e, const bigstring p1, const bigstring p2, const bigstring p3) { 
    printf("3-param error: %d\n", e); 
    (void)p1; 
    (void)p2; 
    (void)p3; 
}

void langparamerror(short e, const bigstring p) { 
    printf("param error: %d\n", e); 
    (void)p; 
}

void lang2paramerror(short e, const bigstring p1, const bigstring p2) { 
    printf("2-param error: %d\n", e); 
    (void)p1; 
    (void)p2; 
}

void langlongparamerror(short e, long p) { 
    printf("long param error: %d, %ld\n", e, p); 
}

void langostypeparamerror(short e, OSType p) { 
    printf("OSType param error: %d, %08X\n", e, (unsigned int)p); 
}

void langbadexternaloperror(short e, tyvaluerecord p) { 
    printf("bad external op error: %d\n", e); 
    (void)p; 
}

// Basic utility functions
int alllower(const char *str) {
    if (!str) return 0;
    while (*str) {
        if (*str >= 'A' && *str <= 'Z') return 0;
        str++;
    }
    return 1;
}

int isallnumeric(const char *str) {
    if (!str) return 0;
    while (*str) {
        if (*str < '0' || *str > '9') return 0;
        str++;
    }
    return 1;
}

// Hash table operations
void hashassign(void *table, const char *key, void *value) {
    (void)table;
    (void)key;
    (void)value;
}

void hashdelete(void *table, const char *key) {
    (void)table;
    (void)key;
}

void *hashgetiteminfo(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void *hashgettypestring(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void *hashgetvaluestring(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void hashinsert(void *table, const char *key, void *value) {
    (void)table;
    (void)key;
    (void)value;
}

void *hashinversesearch(void *table, void *value) {
    (void)table;
    (void)value;
    return NULL;
}

void *hashlookupnode(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void *hashresolvevalue(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void hashsetlocality(void *table, const char *key) {
    (void)table;
    (void)key;
}

int hashsymbolexists(void *table, const char *key) {
    (void)table;
    (void)key;
    return 0;
}

void hashtableassign(void *table, const char *key, void *value) {
    (void)table;
    (void)key;
    (void)value;
}

void hashtabledelete(void *table, const char *key) {
    (void)table;
    (void)key;
}

void *hashtablelookup(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void *hashtablelookupnode(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

int hashtablesymbolexists(void *table, const char *key) {
    (void)table;
    (void)key;
    return 0;
}

// Language functions
void *langdisposetree(void *tree) {
    (void)tree;
    return NULL;
}

void *langexternalbracketname(const char *name) {
    (void)name;
    return NULL;
}

void langexternaldisposevalue(void *value) {
    (void)value;
}

void *langexternalgetfullpath(const char *path) {
    (void)path;
    return NULL;
}

void *langexternalgetquotedpath(const char *path) {
    (void)path;
    return NULL;
}

void *langexternalgettype(void *value) {
    (void)value;
    return NULL;
}

void *langexternalgettypeid(void *value) {
    (void)value;
    return NULL;
}

void *langexternalpacktotext(void *value) {
    (void)value;
    return NULL;
}

void *langexternaltypestring(void *value) {
    (void)value;
    return NULL;
}

void *langexternalvaltocode(void *value) {
    (void)value;
    return NULL;
}

void *langexternalvaltotable(void *value) {
    (void)value;
    return NULL;
}

void *langfindsymbol(const char *name) {
    (void)name;
    return NULL;
}

void *langgetlistitem(void *list, long index) {
    (void)list;
    (void)index;
    return NULL;
}

long langgetlistsize(void *list) {
    (void)list;
    return 0;
}

void *langgetsymbolval(void *symbol) {
    (void)symbol;
    return NULL;
}

void *langgettypeid(void *value) {
    (void)value;
    return NULL;
}

void *langgettypestring(void *value) {
    (void)value;
    return NULL;
}

long langgetvalsize(void *value) {
    (void)value;
    return 0;
}

int langheapallocated(void *value) {
    (void)value;
    return 0;
}

void *langipccomplexmessage(void *message) {
    (void)message;
    return NULL;
}

void *langipcmessage(void *message) {
    (void)message;
    return NULL;
}

void *langipctablemessage(void *message) {
    (void)message;
    return NULL;
}

int langisremotefunction(void *function) {
    (void)function;
    return 0;
}

void *langremotefunctioncall(void *function) {
    (void)function;
    return NULL;
}

void langsetsymboltableval(void *table, const char *key, void *value) {
    (void)table;
    (void)key;
    (void)value;
}

void langsetsymbolval(void *symbol, void *value) {
    (void)symbol;
    (void)value;
}

void langsetthisvalue(void *value) {
    (void)value;
}

void *langsymbolchanged(void *symbol) {
    (void)symbol;
    return NULL;
}

void *langunpackverb(void *verb) {
    (void)verb;
    return NULL;
}

// List operations
void *listaddvalue(void *list, void *value) {
    (void)list;
    (void)value;
    return NULL;
}

void *listarrayvalue(void *list, long index) {
    (void)list;
    (void)index;
    return NULL;
}

void listassignvalue(void *list, long index, void *value) {
    (void)list;
    (void)index;
    (void)value;
}

int listcomparevalue(void *list1, void *list2) {
    (void)list1;
    (void)list2;
    return 0;
}

void listdeletevalue(void *list, long index) {
    (void)list;
    (void)index;
}

void *listsubtractvalue(void *list, void *value) {
    (void)list;
    (void)value;
    return NULL;
}

// System functions
void *aliastofilespec(void *alias) {
    (void)alias;
    return NULL;
}

void *aliastostring(void *alias) {
    (void)alias;
    return NULL;
}

void *bytestohexstring(const void *data, long length) {
    (void)data;
    (void)length;
    return NULL;
}

int comparehandles(void *h1, void *h2) {
    (void)h1;
    (void)h2;
    return 0;
}

void countwords(const char *str) {
    (void)str;
}

void dirtostring(const char *dir) {
    (void)dir;
}

void disposetmpvalue(void *value) {
    (void)value;
}

void dolongswap(void *data) {
    (void)data;
}

void doshortswap(void *data) {
    (void)data;
}

void equalhandles(void *h1, void *h2) {
    (void)h1;
    (void)h2;
}

void equalidentifiers(const char *id1, const char *id2) {
    (void)id1;
    (void)id2;
}

void equalstrings(const char *str1, const char *str2) {
    (void)str1;
    (void)str2;
}

void equaltextidentifiers(const char *id1, const char *id2) {
    (void)id1;
    (void)id2;
}

void *evaluateosascript(const char *script) {
    (void)script;
    return NULL;
}

void exemptfromtmpstack(void *value) {
    (void)value;
}

void filespecaddvalue(void *spec, void *value) {
    (void)spec;
    (void)value;
}

void filespecsubtractvalue(void *spec, void *value) {
    (void)spec;
    (void)value;
}

void *findinparenttable(void *table, const char *key) {
    (void)table;
    (void)key;
    return NULL;
}

void *flfindanyspecialsymbol(const char *name) {
    (void)name;
    return NULL;
}

void *floattostring(double value) {
    (void)value;
    return NULL;
}

void *gestalt(long selector) {
    (void)selector;
    return NULL;
}

void *getsystemerrorstring(long error) {
    (void)error;
    return NULL;
}

void *getsystemtablescript(const char *name) {
    (void)name;
    return NULL;
}

void *hexstringtonumber(const char *str) {
    (void)str;
    return NULL;
}

void insertinhandle(void *handle, long offset, const void *data, long length) {
    (void)handle;
    (void)offset;
    (void)data;
    (void)length;
}

void insertstring(const char *str, long offset, const char *insert) {
    (void)str;
    (void)offset;
    (void)insert;
}

int isobjspectree(void *tree) {
    (void)tree;
    return 0;
}

int isosascriptnode(void *node) {
    (void)node;
    return 0;
}

int keyboardstatus(void) {
    return 0;
}

void midinsertstring(char *str, long offset, const char *insert) {
    (void)str;
    (void)offset;
    (void)insert;
}

void *nthword(const char *str, long n) {
    (void)str;
    (void)n;
    return NULL;
}

void *numbertohexstring(long number) {
    (void)number;
    return NULL;
}

void *numbertostring(long number) {
    (void)number;
    return NULL;
}

void opdisposelist(void *list) {
    (void)list;
}

void *ostypetostring(void *ostype) {
    (void)ostype;
    return NULL;
}

void *parsedialogstring(const char *str) {
    (void)str;
    return NULL;
}

void *parseparamstring(const char *str) {
    (void)str;
    return NULL;
}

int patternmatch(const char *pattern, const char *str) {
    (void)pattern;
    (void)str;
    return 0;
}

void popleadingchars(const char *str) {
    (void)str;
}

void *searchhandle(void *handle, const void *data, long length) {
    (void)handle;
    (void)data;
    (void)length;
    return NULL;
}

void sethandlecontents(void *handle, const void *data, long length) {
    (void)handle;
    (void)data;
    (void)length;
}

void setobjspecverb(void *spec, const char *verb) {
    (void)spec;
    (void)verb;
}

void setparseparams(void *params) {
    (void)params;
}

void *shorttostring(short value) {
    (void)value;
    return NULL;
}

void stringdeletechars(char *str, long start, long length) {
    (void)str;
    (void)start;
    (void)length;
}

void stringreplaceall(char *str, const char *find, const char *replace) {
    (void)str;
    (void)find;
    (void)replace;
}

void *stringtodir(const char *str) {
    (void)str;
    return NULL;
}

void *stringtofloat(const char *str) {
    (void)str;
    return NULL;
}

void *stringtonumber(const char *str) {
    (void)str;
    return NULL;
}

void *stringtoosttype(const char *str) {
    (void)str;
    return NULL;
}

void *stringtotime(const char *str) {
    (void)str;
    return NULL;
}

void *subtractstrings(const char *str1, const char *str2) {
    (void)str1;
    (void)str2;
    return NULL;
}

void *timedatestring(void *time) {
    (void)time;
    return NULL;
}

int timegreaterthan(void *time1, void *time2) {
    (void)time1;
    (void)time2;
    return 0;
}

int timelessthan(void *time1, void *time2) {
    (void)time1;
    (void)time2;
    return 0;
}

int validhandle(void *handle) {
    (void)handle;
    return 1;
}
