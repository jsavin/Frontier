/* portable/cli_executor.c - Minimal evaluator to satisfy current tests */

#include "cli_executor.h"
#include "platform_adapter.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include <stdlib.h>
#include <string.h>

// Helper function for string duplication
static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* dup = (char*)malloc(len + 1);
    if (dup) memcpy(dup, s, len + 1);
    return dup;
}

usertalk_execution_t* cli_create_execution_context(void) {
    usertalk_execution_t* e = (usertalk_execution_t*)calloc(1, sizeof(*e));
    return e;
}

bool cli_compile_script(const char* script, usertalk_execution_t* exec) {
    if (!exec) return false;
    if (exec->script_source) free(exec->script_source);
    exec->script_source = script ? str_dup(script) : NULL;
    return exec->script_source != NULL;
}

bool cli_execute_compiled_script(usertalk_execution_t* exec) {
    if (!exec || !exec->script_source) return false;
    if (exec->result) { free(exec->result); exec->result = NULL; }

    // Use real language engine when linked
    Handle htext = NewHandle(strlen(exec->script_source));
    if (!htext) return false;
    HLock(htext);
    memcpy(*htext, exec->script_source, strlen(exec->script_source));
    HUnlock(htext);
    hdltreenode hcode = NULL;
    if (!langcompiletext(htext, false, &hcode)) {
        DisposeHandle(htext);
        return false;
    }
    tyvaluerecord vreturned; setnilvalue(&vreturned);
    bigstring empty; setstringlength(empty, 0);
    extern boolean langrunscriptcode(hdlhashtable, bigstring, hdltreenode, tyvaluerecord*, hdlhashtable, tyvaluerecord*);
    // Pass NULL for vparams (no parameters), not a pointer to a nil value
    boolean ok = langrunscriptcode(NULL, empty, hcode, NULL, NULL, &vreturned);
    if (!ok) return false;
    if (!coercetostring(&vreturned)) return false;
    bigstring bs; copyheapstring(vreturned.data.stringvalue, bs);
    size_t len = (size_t)stringlength(bs);
    exec->result = (char*)malloc(len+1);
    if (!exec->result) return false;
    memcpy(exec->result, stringbaseaddress(bs), len);
    exec->result[len] = '\0';
    return true;
}

char* cli_get_execution_result_string(usertalk_execution_t* exec) {
    if (!exec || !exec->result) return NULL;
    return str_dup(exec->result);
}

void cli_free_execution_context(usertalk_execution_t* exec) {
    if (!exec) return;
    free(exec->script_source);
    free(exec->result);
    free(exec);
}

void cli_free(void* p) { free(p); }


