/*
 * opattributes_stub_tests.c - Stub tests for opattributes verbs
 *
 * NOTE (2026-01-12): opattributes verbs are GUI-centric features that are
 * not supported in headless CLI mode. These tests verify that the verbs
 * correctly return "not supported" errors.
 *
 * Testing strategy:
 * 1. Verify all 5 opattributes verbs are dispatched correctly
 * 2. Verify each verb returns error in headless environment
 * 3. Verify error messages are appropriate
 */

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "op.h"
#include "opinternal.h"
#include <stdio.h>
#include <string.h>

/* Verb token values from headless_opattributes_verbs.c */
enum {
    opav_addgroup = 0,
    opav_getall = 1,
    opav_getone = 2,
    opav_makeempty = 3,
    opav_setone = 4
};

/* Forward declarations */
extern boolean opattributes_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror);
static boolean test_opattributes_not_supported(void);
static void pstring_to_cstring(const unsigned char *pstr, char *out, size_t out_size);

int main(int argc, char *argv[]) {
    printf("opattributes_stub_tests: Testing headless opattributes stubs\n");
    printf("========================================================================\n");

    if (!test_opattributes_not_supported()) {
        printf("FAIL: opattributes verbs not properly stubbed\n");
        return 1;
    }

    printf("\n");
    printf("========================================================================\n");
    printf("All tests passed\n");
    return 0;
}

/*
 * Helper: Convert Pascal string to C string
 */
static void pstring_to_cstring(const unsigned char *pstr, char *out, size_t out_size) {
    if (pstr == NULL) {
        out[0] = '\0';
        return;
    }

    size_t len = pstr[0];
    if (len >= out_size)
        len = out_size - 1;

    if (len > 0)
        memcpy(out, &pstr[1], len);

    out[len] = '\0';
}

/*
 * Test: Verify opattributes verbs return "not supported" error
 *
 * In headless mode, opattributes operations (addgroup, getall, getone,
 * makeempty, setone) are GUI-centric and not supported. This test verifies
 * that attempting to use these verbs returns appropriate error messages.
 */
static boolean test_opattributes_not_supported(void) {
    printf("TEST: opattributes verbs should be stubbed in headless mode\n");
    printf("\nCalling verbs with dispatcher and verifying error messages:\n\n");

    struct {
        short token;
        const char *name;
    } verbs[] = {
        { opav_addgroup,  "op.attributes.addgroup" },
        { opav_getall,    "op.attributes.getall" },
        { opav_getone,    "op.attributes.getone" },
        { opav_makeempty, "op.attributes.makeempty" },
        { opav_setone,    "op.attributes.setone" }
    };

    int num_verbs = sizeof(verbs) / sizeof(verbs[0]);
    int passed = 0;
    int failed = 0;

    for (int i = 0; i < num_verbs; i++) {
        bigstring bserror;
        tyvaluerecord vreturned;
        boolean result;

        /* Call verb with NULL error string first to test error handling */
        result = opattributes_valueproc(verbs[i].token, NULL, &vreturned, NULL);

        /* Call verb with error buffer to capture error message */
        bserror[0] = 0;  /* Initialize Pascal string */
        result = opattributes_valueproc(verbs[i].token, NULL, &vreturned, bserror);

        /* Convert Pascal string error to C string */
        char error_msg[256];
        pstring_to_cstring((unsigned char *)bserror, error_msg, sizeof(error_msg));

        /* Verify return value is false (error) */
        if (!result) {
            /* Verify error message is appropriate */
            if (strstr(error_msg, "not supported") != NULL ||
                strstr(error_msg, "headless") != NULL) {
                printf("  ✓ %s: Returns error (not supported)\n", verbs[i].name);
                printf("    Error message: \"%s\"\n", error_msg);
                passed++;
            } else {
                printf("  ✗ %s: Error message doesn't mention 'not supported' or 'headless'\n",
                       verbs[i].name);
                printf("    Received: \"%s\"\n", error_msg);
                failed++;
            }
        } else {
            printf("  ✗ %s: Expected false return value but got true\n", verbs[i].name);
            failed++;
        }
    }

    printf("\nResults: %d passed, %d failed\n", passed, failed);

    /*
     * Note: Full integration testing would require:
     * 1. Outline target to be set (op.attributes operates on current node)
     * 2. Complex binary serialization (pack/unpack with BE64 compliance)
     * 3. GUI context (attributes are primarily for GUI display)
     *
     * Since none of these apply in headless CLI mode, these verbs are
     * correctly stubbed to return "not supported" errors.
     *
     * This approach is consistent with other GUI-only verbs:
     * - op.hoist / op.dehoist: GUI window operations
     * - op.expand / op.collapse: Visual display state
     * - op.attributes: GUI attribute display and editing
     */

    return (failed == 0);
}
