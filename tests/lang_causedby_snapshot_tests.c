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
 * Probe technique for fields hidden behind flcausedbyerrorvalid
 * ------------------------------------------------------------
 *
 * All public getters (langgetcausedbymessage, langgetcausedbystackdepth,
 * langgetcausedbyerror, langgetcausedbystackframe) gate on
 * flcausedbyerrorvalid. When the save-reset sets valid=false, the entire
 * read surface goes opaque -- "valid=false, depth=0, stackdepth=0, empty
 * message" is indistinguishable from "valid=false, depth=2, stackdepth=5,
 * leaked message" through the public API.
 *
 * Workaround: langsavecausedbysnapshot itself copies the live state's
 * fields into the output struct verbatim, with no valid-flag gating
 * (lang.c::827-845). So calling langsavecausedbysnapshot a SECOND time
 * after the first save gives us a probe window onto each raw field. The
 * second-save's reset of live state is irrelevant -- we only use the
 * captured fields in `peek` for assertions, then discard.
 *
 * This is the same observation technique used in test_restore_writes_*
 * (test 2) at the {peek} block: snapshot the live state to read fields
 * that the gated getters would hide.
 *
 * For the causedbyerrorstack array (memcpy on save and restore), we seed
 * a known frame into the live stack via set_live_state's seedFrame
 * parameter (which writes into the temp snapshot's stack array before
 * calling langrestorecausedbysnapshot, so the live state inherits the
 * frame via the restore-side memcpy). Then we probe via the peek-snapshot
 * technique to assert the frame survived.
 *
 * Falsification confirmation (re-run after each strengthening):
 *
 *   - Revert `causedbyerrorstackdepth = 0;` in langsavecausedbysnapshot:
 *     test_save_resets_live_state fails on the stackdepth assertion.
 *   - Revert the save-side `memcpy(out->causedbyerrorstack, ...)`:
 *     test_save_restore_preserves_stack_array fails on the frame-survives
 *     assertion (snap.causedbyerrorstack[0].errorline != seeded value).
 *   - Revert any individual reset line in save (trybodydepth = 0,
 *     flcausedbyerrorvalid = false, causedbyerrormessage[0] = '\0'):
 *     test_save_resets_live_state fails on the matching peek assertion.
 *   - Revert the entire body of langrestorecausedbysnapshot to a no-op:
 *     all four tests fail (set_live_state itself relies on restore).
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
 * message + flcausedbyerrorvalid + optional seed frame in stack[0]).
 * Used by tests to set up "A's originating context".
 *
 * The seedFrame mechanism: when non-NULL, we copy *seedFrame into
 * temp.causedbyerrorstack[0] and set temp.causedbyerrorstackdepth=1
 * before restoring. The restore-side memcpy then mirrors the frame into
 * the live causedbyerrorstack. This lets tests assert that subsequent
 * save/restore preserves stack contents byte-for-byte.
 */
static void set_live_state(int depth, boolean valid, const char *cmessage,
                           const tyerrorrecord *seedFrame) {
	tycausedbysnapshot temp;

	/* Drain any leftover depth from prior tests. */
	while (langgetcausedbystackdepth() > 0)
		langsetintryblock(false);

	/* trybodydepth is not observable directly via the public getters
	 * (langgetcausedbystackdepth gates on flcausedbyerrorvalid). The
	 * snapshot save/restore is the only way to read/write it. To set it
	 * to a known value, we build a snapshot with the desired depth and
	 * restore -- the restore side writes trybodydepth verbatim. */
	memset(&temp, 0, sizeof temp);
	temp.trybodydepth = depth;
	temp.flcausedbyerrorvalid = valid;
	if (seedFrame != NULL) {
		temp.causedbyerrorstack[0] = *seedFrame;
		temp.causedbyerrorstackdepth = 1;
	} else {
		temp.causedbyerrorstackdepth = 0;
	}
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
}

/*
 * Test 1: save captures depth into the snapshot.
 *
 * Establish A's state (depth=2), save, verify snap.trybodydepth==2. This
 * test exercises the save-side capture of trybodydepth specifically; the
 * reset-of-live-state coverage is in test_save_resets_live_state below.
 *
 * Note: prior versions of this test claimed to verify "depth=0 -> gate
 * closed" by writing through langtrysetcausedbymessage. That assertion was
 * misleading because the gate's observable effect
 * (langgetcausedbymessage()==NULL) holds whenever valid==false, regardless
 * of depth -- so it didn't actually distinguish a missing depth reset
 * from a missing valid reset. The strengthened reset coverage now lives
 * in test_save_resets_live_state with per-field peek assertions.
 */
static void test_save_captures_depth(void) {
	tycausedbysnapshot snap;

	/* A's state: depth=2, valid=false (no captured snapshot yet). */
	set_live_state(2, false, NULL, NULL);

	langsavecausedbysnapshot(&snap);

	/* Save side captured A's depth verbatim. */
	assert(snap.trybodydepth == 2);

	/* Cleanup: reset depth so the next test starts clean. */
	set_live_state(0, false, NULL, NULL);
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
	set_live_state(2, true, "originating-A", NULL);

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
	set_live_state(1, true, "incoming-B", NULL);
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
	set_live_state(0, false, NULL, NULL);
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

	set_live_state(1, true, "long-original-A", NULL);

	langsavecausedbysnapshot(&snap_A);
	assert(strcmp(snap_A.causedbyerrormessage, "long-original-A") == 0);

	/* Hammer the live buffer with many writes. */
	for (i = 0; i < 64; ++i) {
		char buf[64];
		snprintf(buf, sizeof buf, "intermediate-%d", i);
		set_live_state(1, true, buf, NULL);
	}

	/* Restore A. Original message must come back even after 64 overwrites. */
	langrestorecausedbysnapshot(&snap_A);
	assert(langgetcausedbymessage() != NULL);
	assert(strcmp(langgetcausedbymessage(), "long-original-A") == 0);

	/* Cleanup. */
	set_live_state(0, false, NULL, NULL);
}

/*
 * Test 4: save resets live state to a clean slate -- ALL four fields.
 *
 * Establish A's state with non-zero values across all four save-reset
 * fields (trybodydepth, causedbyerrorstackdepth, flcausedbyerrorvalid,
 * causedbyerrormessage[0]). Save. Probe each reset field independently
 * via a second snapshot.
 *
 * Why a second snapshot is needed: when valid=false, the public getters
 * (langgetcausedbymessage, langgetcausedbystackdepth) all return
 * NULL/0 regardless of the other fields. A test that only checks
 * langgetcausedbymessage()==NULL after save passes even when only the
 * valid-flag was reset and the other three fields leaked. The CWE-488
 * cross-thread leak that PR3 closed is specifically about trybodydepth
 * leaking -- so this test must observe trybodydepth's reset directly.
 *
 * langsavecausedbysnapshot copies fields verbatim into the output struct
 * with no gating (lang.c::827-845), so a second save acts as a probe.
 */
static void test_save_resets_live_state(void) {
	tycausedbysnapshot snap;
	tycausedbysnapshot peek;
	tyerrorrecord seed;

	/* Seed a recognizable frame so we can later assert stackdepth was
	 * reset (depth=0 means the frame is unreachable but the raw field
	 * value is what we probe). */
	memset(&seed, 0, sizeof seed);
	seed.errorline = 4242;
	seed.errorchar = 17;
	seed.errorrefcon = 0xABCDL;

	set_live_state(3, true, "A-context", &seed);

	/* Pre-save: live state holds A. */
	assert(langgetcausedbymessage() != NULL);
	assert(langgetcausedbystackdepth() == 1);

	langsavecausedbysnapshot(&snap);

	/* The first-level public-getter assertion: with valid=false from the
	 * save-reset, langgetcausedbymessage returns NULL. This holds even
	 * if only the valid flag was reset (the historical regression
	 * blindspot), so it is necessary but not sufficient. */
	assert(langgetcausedbymessage() == NULL);
	assert(langgetcausedbystackdepth() == 0);

	/* Probe each reset field directly via a second save. langsavecausedby-
	 * snapshot copies live fields into peek verbatim, no gating. This
	 * distinguishes "all four fields reset" from "only valid-flag reset". */
	langsavecausedbysnapshot(&peek);

	assert(peek.trybodydepth == 0);            /* save reset trybodydepth */
	assert(peek.flcausedbyerrorvalid == false); /* save reset valid flag */
	assert(peek.causedbyerrorstackdepth == 0); /* save reset stack depth */
	assert(peek.causedbyerrormessage[0] == '\0'); /* save cleared message */

	/* Cleanup. */
	set_live_state(0, false, NULL, NULL);
}

/*
 * Test 5: save+restore preserves the causedbyerrorstack array byte-for-
 * byte. Catches "memcpy replaced with no-op" regressions on either the
 * save side (line 832-833) or the restore side (line 860-861) of lang.c.
 *
 * Mechanism: seed a recognizable frame into stack[0] via set_live_state,
 * save, mutate live state, restore, then probe via a second snapshot to
 * read back the live stack array. The restored frame must match the
 * seeded values exactly.
 */
static void test_save_restore_preserves_stack_array(void) {
	tycausedbysnapshot snap;
	tycausedbysnapshot peek;
	tyerrorrecord seed;

	/* Build a frame with values unlikely to occur by accident. */
	memset(&seed, 0, sizeof seed);
	seed.errorline = 12345;
	seed.errorchar = 67;
	seed.tokenstart = 89;
	seed.tokenend = 91;
	seed.profilebase = 0xDEADBEEFUL;
	seed.profiletotal = 0xCAFEBABEUL;
	seed.errorrefcon = 0x123456L;

	set_live_state(2, true, "stack-seeded", &seed);

	/* Confirm the seed landed in the live stack (via getter, which is
	 * available here because valid==true). */
	{
		tyerrorrecord readback;
		long refcon = 0;
		assert(langgetcausedbystackdepth() == 1);
		assert(langgetcausedbystackframe(0, &readback, &refcon) == true);
		assert(readback.errorline == 12345);
		assert(refcon == 0x123456L);
	}

	/* Save A. Save-side memcpy must copy the live stack into snap. */
	langsavecausedbysnapshot(&snap);
	assert(snap.causedbyerrorstackdepth == 1);
	assert(snap.causedbyerrorstack[0].errorline == 12345);
	assert(snap.causedbyerrorstack[0].errorchar == 67);
	assert(snap.causedbyerrorstack[0].tokenstart == 89);
	assert(snap.causedbyerrorstack[0].tokenend == 91);
	assert(snap.causedbyerrorstack[0].profilebase == 0xDEADBEEFUL);
	assert(snap.causedbyerrorstack[0].profiletotal == 0xCAFEBABEUL);
	assert(snap.causedbyerrorstack[0].errorrefcon == 0x123456L);

	/* Mutate live state with a different frame to make sure the restore
	 * is actually writing back the saved frame, not just leaving the
	 * intermediate value in place. */
	{
		tyerrorrecord other;
		memset(&other, 0, sizeof other);
		other.errorline = 99999;
		other.errorrefcon = 0x999999L;
		set_live_state(1, true, "intermediate", &other);
		/* Sanity: the intermediate frame is now live. */
		{
			tyerrorrecord readback;
			long refcon = 0;
			assert(langgetcausedbystackframe(0, &readback, &refcon) == true);
			assert(readback.errorline == 99999);
		}
	}

	/* Restore A. Restore-side memcpy must copy snap.causedbyerrorstack
	 * back into the live array. */
	langrestorecausedbysnapshot(&snap);

	/* Probe via the public getter (valid==true was restored, so the
	 * getter is open). */
	{
		tyerrorrecord readback;
		long refcon = 0;
		assert(langgetcausedbystackdepth() == 1);
		assert(langgetcausedbystackframe(0, &readback, &refcon) == true);
		assert(readback.errorline == 12345);
		assert(readback.errorchar == 67);
		assert(readback.tokenstart == 89);
		assert(readback.tokenend == 91);
		assert(readback.profilebase == 0xDEADBEEFUL);
		assert(readback.profiletotal == 0xCAFEBABEUL);
		assert(refcon == 0x123456L);
	}

	/* Belt-and-suspenders: also probe via a second save (independent of
	 * the getter's valid-gate) so we'd catch a regression that broke
	 * BOTH the getter and the restore in symmetric ways. */
	langsavecausedbysnapshot(&peek);
	assert(peek.causedbyerrorstackdepth == 1);
	assert(peek.causedbyerrorstack[0].errorline == 12345);
	assert(peek.causedbyerrorstack[0].errorrefcon == 0x123456L);

	/* Cleanup. */
	set_live_state(0, false, NULL, NULL);
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
	TR_RUN(test_save_restore_preserves_stack_array);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
