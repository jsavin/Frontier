/*
 * lang_causedby_snapshot_tests.c - Unit tests for the causedby-snapshot
 * save/restore primitives added by PR3 of the REPL error context chain
 * (commit c2e8ce197, 2026-05-27).
 *
 * Rationale: PR3 added langsavecausedbysnapshot / langrestorecausedbysnapshot
 * (lang.c) and wired them into pushprocess / popprocess (process.c) so that
 * nested process-context switches cannot leak the outgoing context's in-try
 * state into the incoming context's causedby buffer (CWE-488: Exposure of
 * Data Element to Wrong Session).
 *
 * Issue #660 asked for a cross-thread behavioural regression test. During
 * investigation we found:
 *
 *   - pushprocess / popprocess are stubbed to no-ops in the headless build
 *     (frontier-cli/stubs/headless_lang_runtime_more_stubs.c). The PR3 wiring
 *     in process.c is therefore not in effect when the headless CLI runs.
 *
 *   - The headless thread.evaluate path uses langruncode (not langrun), which
 *     does not push a frame onto langcallbacks.scripterrorstack. When the
 *     spawned thread fires lang.scriptError, langseterrorcallbackline takes
 *     the early-return at (**hs).toperror <= 0 and never reaches the
 *     trybodydepth > 0 capture site. So even if trybodydepth leaks across
 *     the GIL handoff (which it can, because headless_save_threadglobals
 *     does not save it), the leak has no observable expression via
 *     thread.evaluate at the integration-test layer.
 *
 * Net: a behavioural integration test for the cross-thread leak in headless
 * is not viable today. Issue #660's behavioural assertion needs follow-up
 * scope (extend headless_save_threadglobals to cover trybodydepth +
 * causedby state, or eliminate the static-global mechanism, then add the
 * integration test). See tests/integration/CROSS_THREAD_TEST_PATTERN.md.
 *
 * What we CAN guard at the unit level is the snapshot save/restore
 * mechanism itself -- the actual functions PR3 added. If a future
 * refactor neuters either function, this test fails. That doesn't cover
 * the cross-process integration the GUI build relies on, but it does
 * stop the snapshot functions from silently degrading.
 *
 * Falsification: comment out the body of langsavecausedbysnapshot's reset
 * block (or langrestorecausedbysnapshot's restore block) in lang.c and
 * rebuild. The tests below fail.
 *
 * What the tests can and cannot drive
 *
 * The fully end-to-end "fire an error inside a try body and capture into
 * causedby" path requires a non-empty langcallbacks.scripterrorstack and
 * goes through langseterrorcallbackline. Rather than stand up that full
 * harness in a unit test (which would couple the test to a lot of
 * runtime state), these tests drive the snapshot fields directly:
 *
 *   - trybodydepth is observed via the langtrysetcausedbymessage gate
 *     (it is a no-op when trybodydepth == 0).
 *   - The message buffer is read directly via causedbyerrormessage (we
 *     drive into it via langtrysetcausedbymessage and read it back via
 *     langgetcausedbymessage AFTER forcing flcausedbyerrorvalid through
 *     a direct write into the snapshot's `flcausedbyerrorvalid` field
 *     and then restoring from it).
 *
 * This gives us coverage of the snapshot's by-value preservation of
 * (trybodydepth, message, flcausedbyerrorvalid) -- the three fields
 * whose leak across a process boundary is the cross-session-exposure
 * window PR3 closes.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "logging.h"
#include "test_report.h"

/* Helper: build a Pascal-style bigstring from a C literal. */
static void bs_from_cstr(const char *cstr, bigstring out) {
	size_t len = strlen(cstr);
	if (len > 255)
		len = 255;
	out[0] = (unsigned char) len;
	if (len > 0)
		memcpy(&out[1], cstr, len);
}

/*
 * Helper: drive the live causedby buffer into a known state (depth +
 * message + flcausedbyerrorvalid). Used by tests to set up "A's
 * originating context".
 *
 * Drives via the public lang API: langsetintryblock to bump trybodydepth,
 * langtrysetcausedbymessage to write the message buffer. To set
 * flcausedbyerrorvalid (which langtrysetcausedbymessage does NOT touch),
 * we round-trip through the snapshot: build a snapshot with valid=true
 * via direct field assignment, then call langrestorecausedbysnapshot.
 * This is the same mechanism popprocess uses on the way back into A's
 * context after B ran.
 */
static void set_live_state(int depth, boolean valid, const char *cmessage) {
	tycausedbysnapshot temp;
	bigstring bsmsg;
	int i;

	/* Drain any leftover depth from prior tests. */
	while (langgetcausedbystackdepth() > 0)
		langsetintryblock(false);

	/* trybodydepth is not observable directly; the snapshot save/restore
	 * is the only way to read it. To set it to a known value, we build
	 * a snapshot with the desired depth and restore -- the restore side
	 * writes trybodydepth verbatim. */
	memset(&temp, 0, sizeof temp);
	temp.trybodydepth = depth;
	temp.flcausedbyerrorvalid = valid;
	temp.causedbyerrorstackdepth = 0;
	if (cmessage != NULL) {
		size_t mlen = strlen(cmessage);
		if (mlen > sizeof(temp.causedbyerrormessage) - 1)
			mlen = sizeof(temp.causedbyerrormessage) - 1;
		memcpy(temp.causedbyerrormessage, cmessage, mlen);
		temp.causedbyerrormessage[mlen] = '\0';
	} else {
		temp.causedbyerrormessage[0] = '\0';
	}
	langrestorecausedbysnapshot(&temp);
	(void) bsmsg;
	(void) i;
}

/*
 * Test 1: save preserves the snapshot's trybodydepth field.
 *
 * Restore A's state (depth=2), snapshot it, mutate live state to depth=0,
 * restore the snapshot. The captured depth must be back.
 *
 * Observation: the restored depth is verified via the
 * langtrysetcausedbymessage gate (no-op when depth==0; writes when
 * depth>0).
 */
static void test_save_captures_depth(void) {
	tycausedbysnapshot snap;
	bigstring probe;

	/* A's state: depth=2, valid=false (no captured snapshot yet). */
	set_live_state(2, false, NULL);

	/* Save. The save side resets live state to (depth=0, valid=false,
	 * empty message). */
	langsavecausedbysnapshot(&snap);

	/* Verify the snapshot captured depth=2. The captured value lives
	 * inside `snap` and we restore it to read it. */
	assert(snap.trybodydepth == 2);

	/* Verify live state was reset by the save: depth=0 -> gate closed ->
	 * langtrysetcausedbymessage is a no-op. After the no-op, the
	 * message buffer should still be empty (we never wrote to it
	 * post-save). */
	bs_from_cstr("post-save-should-not-stick", probe);
	langtrysetcausedbymessage(probe);
	/* langgetcausedbymessage returns NULL when !flcausedbyerrorvalid;
	 * since the save also reset that flag, this should be NULL. */
	assert(langgetcausedbymessage() == NULL);

	/* Cleanup: reset depth so the next test starts clean. */
	set_live_state(0, false, NULL);
}

/*
 * Test 2: restore writes the snapshot's trybodydepth back into the live
 * state.
 *
 * Build a snapshot with depth=3 and a message, restore, verify live
 * state matches. This is the round-trip property: a save followed by an
 * arbitrary mutation followed by a restore reproduces the saved state.
 */
static void test_restore_writes_depth_and_message(void) {
	tycausedbysnapshot snap_A, snap_B;
	bigstring probe;

	/* A's state: depth=2, valid via the snapshot round-trip,
	 * message="originating-A". */
	set_live_state(2, true, "originating-A");

	/* Sanity: live state should now report A's message via the getter
	 * (gated on flcausedbyerrorvalid). */
	assert(langgetcausedbymessage() != NULL);
	assert(strcmp(langgetcausedbymessage(), "originating-A") == 0);

	/* Save A. */
	langsavecausedbysnapshot(&snap_A);

	/* Save asserts: snapshot captured (depth=2, valid=true, message). */
	assert(snap_A.trybodydepth == 2);
	assert(snap_A.flcausedbyerrorvalid == true);
	assert(strcmp(snap_A.causedbyerrormessage, "originating-A") == 0);

	/* Save side reset live state to clean. */
	assert(langgetcausedbymessage() == NULL);

	/* Simulate B's incoming context populating live state. */
	set_live_state(1, true, "incoming-B");
	assert(strcmp(langgetcausedbymessage(), "incoming-B") == 0);

	/* Save B's state too (so we can compare later). */
	langsavecausedbysnapshot(&snap_B);
	assert(strcmp(snap_B.causedbyerrormessage, "incoming-B") == 0);

	/* Restore A. */
	langrestorecausedbysnapshot(&snap_A);

	/* Restore must put A's depth back: langtrysetcausedbymessage is
	 * gated on depth>0 -- but since A also had flcausedbyerrorvalid=true,
	 * the gate is closed by the first-error-wins rule. So we check that
	 * langgetcausedbymessage returns A's message (valid=true preserved). */
	assert(langgetcausedbymessage() != NULL);
	assert(strcmp(langgetcausedbymessage(), "originating-A") == 0);

	/* Now clear valid (simulating A's else block finishing) so we can
	 * verify the depth came back. With valid=false and depth=2 from the
	 * restored snapshot, langtrysetcausedbymessage should write through. */
	langclearcausedbyerror();
	bs_from_cstr("after-clear-A-still-in-try", probe);
	langtrysetcausedbymessage(probe);
	/* Message buffer was written -- but flcausedbyerrorvalid is false,
	 * so the getter returns NULL. We instead verify by taking another
	 * snapshot and inspecting causedbyerrormessage directly: the probe
	 * write happened, proving depth>0 still held. */
	{
		tycausedbysnapshot peek;
		langsavecausedbysnapshot(&peek);
		assert(strcmp(peek.causedbyerrormessage, "after-clear-A-still-in-try") == 0);
		/* (Snapshot peek discarded; live state was cleared by the save side.) */
	}

	/* Cleanup. */
	set_live_state(0, false, NULL);
}

/*
 * Test 3: save/restore is by-value (no aliasing into shared storage).
 *
 * Save A, mutate live state through many intervening write paths, then
 * restore. A's message must come back intact despite the live buffer
 * having been overwritten many times.
 */
static void test_snapshot_is_by_value(void) {
	tycausedbysnapshot snap_A;
	int i;

	set_live_state(1, true, "long-original-A");

	langsavecausedbysnapshot(&snap_A);
	assert(strcmp(snap_A.causedbyerrormessage, "long-original-A") == 0);

	/* Hammer the live buffer with many writes. */
	for (i = 0; i < 64; ++i) {
		char buf[64];
		snprintf(buf, sizeof buf, "intermediate-%d", i);
		set_live_state(1, true, buf);
	}

	/* Restore A. Original message must come back even after 64 overwrites. */
	langrestorecausedbysnapshot(&snap_A);
	assert(langgetcausedbymessage() != NULL);
	assert(strcmp(langgetcausedbymessage(), "long-original-A") == 0);

	/* Cleanup. */
	set_live_state(0, false, NULL);
}

/*
 * Test 4: save resets live state to a clean slate.
 *
 * Establish A's state, save, verify live state is now clean. This is
 * the "incoming context starts fresh" guarantee.
 */
static void test_save_resets_live_state(void) {
	tycausedbysnapshot snap;

	set_live_state(3, true, "A-context");

	/* Pre-save: live state holds A. */
	assert(langgetcausedbymessage() != NULL);

	langsavecausedbysnapshot(&snap);

	/* Post-save: live state must be clean (incoming context starts at
	 * depth=0, valid=false, empty message). */
	assert(langgetcausedbymessage() == NULL); /* valid=false */

	/* Cleanup. */
	set_live_state(0, false, NULL);
}

int main(void) {
	TR_INIT("lang_causedby_snapshot_tests");

	log_init();

	/* The snapshot functions live in lang.c and operate on static
	 * globals inside lang.c; they don't need a fully-initialised runtime.
	 * We do not call initlang() / langinitverbs() etc., to keep the test
	 * focused on the snapshot mechanism and avoid coupling to verb-table
	 * init order. */

	TR_RUN(test_save_captures_depth);
	TR_RUN(test_restore_writes_depth_and_message);
	TR_RUN(test_snapshot_is_by_value);
	TR_RUN(test_save_resets_live_state);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
