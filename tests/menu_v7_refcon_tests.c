/*
 * menu_v7_refcon_tests.c - Unit tests for v7 menu refcon table format
 *
 * Tests the mecreaterefcontable_v7() and meunpackrefcontable_v7() functions
 * that convert menu item data to/from the new v7 packed table format.
 *
 * V7 menu refcon format stores each node's refcon as a packed table with:
 * - keyBinding (char) - command key character or 0
 * - modifiers (table) - {shift, control, option, command} booleans
 * - handlerScript (script) - the script object or nil
 *
 * Part of the menu v6->v7 migration validation test suite.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "op.h"
#include "opinternal.h"
#include "memory.h"
#include "lang.h"
#include "tablestructure.h"
#include "logging.h"
#include "../portable/wptext_portable.h"

/*
 * Forward declarations for functions being tested.
 * These will be implemented in menuverbs.c or a new menu_v7.c file.
 */
extern boolean mecreaterefcontable_v7(byte cmdkey, tykeyflags modifiers,
                                       hdloutlinerecord hscript,
                                       Handle *hpackedtable);

extern boolean meunpackrefcontable_v7(Handle hpackedtable,
                                       byte *cmdkey,
                                       tykeyflags *modifiers,
                                       hdloutlinerecord *hscript);

/*
 * Helper: Create a simple test script outline with one line.
 * Used to verify script roundtripping.
 */
static hdloutlinerecord create_test_script(const char *line) {
    hdloutlinerecord ho = nil;
    if (!newoutlinerecord(&ho))
        return nil;

    /* Push outline to make it current - required before modifying */
    oppushoutline(ho);

    /* Set the root headline text */
    bigstring bs;
    copyctopstring(line, bs);
    if (!opsetheadstring((**ho).hsummit, bs)) {
        oppopoutline();
        opdisposeoutline(ho, false);
        return nil;
    }

    /* Pop outline but keep it allocated */
    oppopoutline();

    return ho;
}

/*
 * Helper: Get the root headline text from an outline.
 */
static boolean get_script_text(hdloutlinerecord ho, char *out, size_t outsize) {
    if (ho == nil || out == nil || outsize == 0)
        return false;

    /* Push outline to make it current - required for opgetheadstring */
    oppushoutline(ho);

    bigstring bs;
    opgetheadstring((**ho).hsummit, bs);
    copyptocstring(bs, out);

    oppopoutline();
    return true;
}

/*
 * Test 1: Empty refcon - no keybinding (cmdkey=0) and no script (hscript=nil)
 *
 * Verify roundtrip: create with no keybinding and no script,
 * unpack and verify both are nil/0.
 */
static void test_menu_refcon_empty(void) {
    printf("[menu_v7] Test 1: Empty refcon (no keybinding, no script)... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 0;
    tykeyflags modifiers_in = keynormal;
    hdloutlinerecord hscript_in = nil;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, hscript_in, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0xFF;  /* Initialize to non-zero to detect changes */
    tykeyflags modifiers_out = (tykeyflags)0xFFFF;
    hdloutlinerecord hscript_out = (hdloutlinerecord)0x1;  /* Non-nil sentinel */

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 0);
    assert(modifiers_out == keynormal);
    assert(hscript_out == nil);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 2: Keybinding only - 'K' key with command modifier, no script
 *
 * Common case: menu item has Cmd-K shortcut but no attached script.
 */
static void test_menu_refcon_keybinding_only(void) {
    printf("[menu_v7] Test 2: Keybinding only (Cmd-K, no script)... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'K';
    tykeyflags modifiers_in = keycommand;
    hdloutlinerecord hscript_in = nil;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, hscript_in, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = (hdloutlinerecord)0x1;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    if (cmdkey_out != 'K' || modifiers_out != keycommand) {
        printf("\nFAILED: cmdkey=%d (expected 'K'=%d), modifiers=0x%04x (expected 0x%04x)\n",
               (int)cmdkey_out, (int)'K', (unsigned)modifiers_out, (unsigned)keycommand);
        fflush(stdout);
    }
    assert(cmdkey_out == 'K');
    assert(modifiers_out == keycommand);
    assert(hscript_out == nil);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 3: Script only - no keybinding but has attached script
 *
 * Some menu items have scripts but no keyboard shortcut.
 */
static void test_menu_refcon_script_only(void) {
    printf("[menu_v7] Test 3: Script only (no keybinding)... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 0;
    tykeyflags modifiers_in = keynormal;
    hdloutlinerecord hscript_in = create_test_script("dialog.alert(\"Hello\")");
    assert(hscript_in != nil);

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, hscript_in, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0xFF;
    tykeyflags modifiers_out = (tykeyflags)0xFFFF;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 0);
    assert(modifiers_out == keynormal);
    assert(hscript_out != nil);

    /* Verify script content */
    char script_text[256];
    assert(get_script_text(hscript_out, script_text, sizeof(script_text)));
    assert(strcmp(script_text, "dialog.alert(\"Hello\")") == 0);

    /* Cleanup */
    opdisposeoutline(hscript_in, false);
    opdisposeoutline(hscript_out, false);
    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 4: Full refcon - keybinding with all modifiers + script
 *
 * Test complete menu item data: Shift+Cmd+Option+Ctrl-X with script.
 */
static void test_menu_refcon_full(void) {
    printf("[menu_v7] Test 4: Full refcon (all modifiers + script)... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'X';
    tykeyflags modifiers_in = (tykeyflags)(keyshift | keycontrol | keyoption | keycommand);
    hdloutlinerecord hscript_in = create_test_script("sys.beep()");
    assert(hscript_in != nil);

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, hscript_in, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'X');
    assert((modifiers_out & keyshift) != 0);
    assert((modifiers_out & keycontrol) != 0);
    assert((modifiers_out & keyoption) != 0);
    assert((modifiers_out & keycommand) != 0);
    assert(hscript_out != nil);

    /* Verify script content */
    char script_text[256];
    assert(get_script_text(hscript_out, script_text, sizeof(script_text)));
    assert(strcmp(script_text, "sys.beep()") == 0);

    /* Cleanup */
    opdisposeoutline(hscript_in, false);
    opdisposeoutline(hscript_out, false);
    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 5: Shift modifier only
 *
 * Verify that the shift modifier flag (0x0001) is correctly encoded/decoded.
 */
static void test_menu_refcon_shift_modifier(void) {
    printf("[menu_v7] Test 5: Shift modifier only... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'S';
    tykeyflags modifiers_in = keyshift;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, nil, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'S');
    assert((modifiers_out & keyshift) != 0);
    assert((modifiers_out & keycontrol) == 0);
    assert((modifiers_out & keyoption) == 0);
    assert((modifiers_out & keycommand) == 0);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 6: Control modifier only
 *
 * Verify that the control modifier flag (0x0200) is correctly encoded/decoded.
 */
static void test_menu_refcon_control_modifier(void) {
    printf("[menu_v7] Test 6: Control modifier only... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'C';
    tykeyflags modifiers_in = keycontrol;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, nil, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'C');
    assert((modifiers_out & keyshift) == 0);
    assert((modifiers_out & keycontrol) != 0);
    assert((modifiers_out & keyoption) == 0);
    assert((modifiers_out & keycommand) == 0);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 7: Option modifier only
 *
 * Verify that the option modifier flag (0x0400) is correctly encoded/decoded.
 */
static void test_menu_refcon_option_modifier(void) {
    printf("[menu_v7] Test 7: Option modifier only... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'O';
    tykeyflags modifiers_in = keyoption;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, nil, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'O');
    assert((modifiers_out & keyshift) == 0);
    assert((modifiers_out & keycontrol) == 0);
    assert((modifiers_out & keyoption) != 0);
    assert((modifiers_out & keycommand) == 0);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 8: Command modifier only
 *
 * Verify that the command modifier flag (0x0800) is correctly encoded/decoded.
 */
static void test_menu_refcon_command_modifier(void) {
    printf("[menu_v7] Test 8: Command modifier only... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'M';
    tykeyflags modifiers_in = keycommand;

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, nil, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'M');
    assert((modifiers_out & keyshift) == 0);
    assert((modifiers_out & keycontrol) == 0);
    assert((modifiers_out & keyoption) == 0);
    assert((modifiers_out & keycommand) != 0);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Test 9: Combined modifiers - Shift+Cmd+Option
 *
 * Test a realistic combination: Shift+Cmd+Option modifier combination.
 * This is a common pattern for "alternate" menu commands.
 */
static void test_menu_refcon_combined_modifiers(void) {
    printf("[menu_v7] Test 9: Combined modifiers (Shift+Cmd+Option)... ");
    fflush(stdout);

    Handle hpacked = nil;
    byte cmdkey_in = 'A';
    tykeyflags modifiers_in = (tykeyflags)(keyshift | keycommand | keyoption);

    /* Create packed refcon */
    assert(mecreaterefcontable_v7(cmdkey_in, modifiers_in, nil, &hpacked));
    assert(hpacked != nil);

    /* Unpack and verify */
    byte cmdkey_out = 0;
    tykeyflags modifiers_out = keynormal;
    hdloutlinerecord hscript_out = nil;

    assert(meunpackrefcontable_v7(hpacked, &cmdkey_out, &modifiers_out, &hscript_out));

    assert(cmdkey_out == 'A');
    assert((modifiers_out & keyshift) != 0);
    assert((modifiers_out & keycontrol) == 0);  /* Control should NOT be set */
    assert((modifiers_out & keyoption) != 0);
    assert((modifiers_out & keycommand) != 0);

    disposehandle(hpacked);

    printf("PASS\n");
    fflush(stdout);
}

/*
 * Main test runner
 */
int main(void) {
    printf("\n=== Menu V7 Refcon Table Format Tests ===\n");
    printf("[menu_v7] Initializing runtime...\n");
    fflush(stdout);

    /* Initialize logging system */
    log_init();

    /* Initialize runtime subsystems - order matters! */
    assert(initmemory());
    initstrings();  /* Must be called before initlang - initializes lowercasetable for hash functions */
    assert(initlang());
    assert(inittablestructure());
    assert(langinitresources_headless());  /* Initialize constants, keywords, builtins - required for outline ops */
    assert(langinitverbs());
    assert(wp_portable_init());

    printf("[menu_v7] Testing v7 menu refcon table pack/unpack\n");
    fflush(stdout);

    test_menu_refcon_empty();
    test_menu_refcon_keybinding_only();
    test_menu_refcon_script_only();
    test_menu_refcon_full();
    test_menu_refcon_shift_modifier();
    test_menu_refcon_control_modifier();
    test_menu_refcon_option_modifier();
    test_menu_refcon_command_modifier();
    test_menu_refcon_combined_modifiers();

    printf("\n========================================\n");
    printf("[menu_v7] ALL TESTS PASSED\n");
    printf("========================================\n");
    fflush(stdout);

    wp_portable_shutdown();
    return 0;
}
