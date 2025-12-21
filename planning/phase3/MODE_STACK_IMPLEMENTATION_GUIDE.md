# Mode Stack Refactor - Detailed Implementation Guide

**Status**: Ready for Execution
**Created**: 2025-12-20
**Prerequisite Reading**: MODE_SINGLE_DECISION_POINT.md

## Overview

This guide provides step-by-step instructions for implementing the single decision point pattern. Each step includes exact file locations, code blocks, and validation commands.

---

## Step 1: Create ensure_external_in_memory() Helper

### File: Common/source/langexternal.c

**Location**: Add immediately after `langexternalpack_internal()` function (~line 848)

**Code to Add**:
```c
static boolean ensure_external_in_memory (hdlexternalvariable hv) {

	/*
	2025-12-20: Type-specific external loading dispatcher

	Preconditions:
	  - Caller has set correct read mode in global state
	  - Database context is correct (dbpushdatabase if needed)

	Postconditions:
	  - If returns true: (**hv).flinmemory == 1
	  - If returns false: external could not be loaded
	  - Global mode is unchanged

	This function does NOT manage mode - it's a pure dispatcher.
	*/

	if ((**hv).flinmemory)
		return (true); /* already in memory */

	switch ((**hv).id) {

		case idoutlineprocessor:
		case idscriptprocessor:
			return (opverbinmemory ((hdloutlinevariable) hv));

		case idwordprocessor:
			return (wpverbinmemory (hv));

		case idtableprocessor:
			return (tableverbinmemory (hv, HNoNode));

		case idmenuprocessor:
			return (menuverbinmemory (hv));

		case idpictprocessor:
			return (pictverbinmemory (hv));

		default:
			return (false);
		}
	} /*ensure_external_in_memory*/
```

**Validation**:
```bash
# Should compile without errors
make -C tests clean && make -C tests save_migration_tests 2>&1 | grep -i error
# Should return no errors
```

---

## Step 2: Update langexternalpack_internal()

### File: Common/source/langexternal.c

**Find**: Function `langexternalpack_internal()` starting around line 768

**Current Structure** (what to replace):
```c
boolean langexternalpack_internal (const db_context *ctx, hdlexternalhandle h, Handle *hpacked, boolean *flnewdbaddress) {
	hdlexternalvariable hv = (hdlexternalvariable) h;
	tydiskexternalhandle rec;
	db_context working_context, legacy_context;
	boolean adapter_repack;
	boolean ok = false;

	/* Initialize working context from provided context or global state */
	if (ctx != NULL) {
		working_context = *ctx;
	} else {
		db_context_init(&working_context);
	}

	adapter_repack = db_format_adapter_force_repack();

	rollbeachball ();

	rec.versionnumber = conditionalshortswap (externaldiskversionnumber);
	rec.id = (byte) (**hv).id;

	if (!newfilledhandle (&rec, sizeof (rec), hpacked))
		return (false);

    db_format_adapter_mark_address(&(**hv).oldaddress);
    if (adapter_repack) {
		/* Create legacy context for reading v6 while materializing externals */
        legacy_context = working_context;
        legacy_context.mode.use_64bit_format = false;
		db_context_apply(&legacy_context);
    }

	switch ((**hv).id) {
		case idoutlineprocessor: case idscriptprocessor:
			ok = opverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;
		case idwordprocessor:
			ok = wpverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;
		case idtableprocessor:
			ok = tableverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;
		case idmenuprocessor:
			ok = menuverbpack (hv, hpacked, flnewdbaddress);
			break;
		case idpictprocessor:
			ok = pictverbpack (hv, hpacked, flnewdbaddress);
			break;
		default:
			ok = false;
			break;
	}

	if (adapter_repack) {
		/* Restore working context after legacy read */
		db_context_apply(&working_context);
	}

	return ok;
}
```

**Replace With**:
```c
boolean langexternalpack_internal (const db_context *ctx, hdlexternalhandle h, Handle *hpacked, boolean *flnewdbaddress) {

	/*
	2025-12-20: THE ONLY FUNCTION THAT MANAGES MODE FOR EXTERNAL PACKING

	Single Decision Point Pattern:
	  - This function decides when to use v6 read mode vs v7 write mode
	  - Child pack functions are pure operations - they don't manage mode
	  - Mode transitions happen in ONE place (here) for entire operation
	*/

	hdlexternalvariable hv = (hdlexternalvariable) h;
	tydiskexternalhandle rec;
	db_context working_context, legacy_context;
	boolean adapter_repack;
	boolean ok = false;

	/* Initialize working context from provided context or global state */
	if (ctx != NULL) {
		working_context = *ctx;
	} else {
		db_context_init(&working_context);
	}

	adapter_repack = db_format_adapter_force_repack();

	rollbeachball ();

	/* Setup disk header */
	rec.versionnumber = conditionalshortswap (externaldiskversionnumber);
	rec.id = (byte) (**hv).id;

	if (!newfilledhandle (&rec, sizeof (rec), hpacked))
		return (false);

	/* Mark address for migration tracking */
	db_format_adapter_mark_address(&(**hv).oldaddress);

	/* ================================================================
	 * SINGLE DECISION POINT: Load from v6 if needed, then switch to v7
	 * ================================================================
	 */
	if (adapter_repack && !(**hv).flinmemory) {
		/* Set v6 read mode for loading from source database */
		legacy_context = working_context;
		legacy_context.mode.use_64bit_format = false;
		legacy_context.mode.adapter_repack = false;  /* Pure read mode */
		db_context_apply(&legacy_context);

#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] langexternalpack: loading external from v6 id=%d\n",
		        (int)(**hv).id);
#endif

		/* Load external into memory (uses current v6 mode) */
		if (!ensure_external_in_memory (hv)) {
			/* Restore working context even on error */
			db_context_apply(&working_context);
			return (false);
		}

#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] langexternalpack: loaded, now flinmemory=%d\n",
		        (int)(**hv).flinmemory);
#endif

		/* Switch to v7 write mode for packing to destination */
		working_context.mode.use_64bit_format = true;
		working_context.mode.adapter_repack = true;
		db_context_apply(&working_context);

#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] langexternalpack: mode switched to v7 write\n");
#endif
	}

	/* ================================================================
	 * Pack with output format mode - children don't change mode
	 * ================================================================
	 */
	switch ((**hv).id) {

		case idoutlineprocessor:
		case idscriptprocessor:
			ok = opverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;

		case idwordprocessor:
			ok = wpverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;

		case idtableprocessor:
			ok = tableverbpack_internal (&working_context, hv, hpacked, flnewdbaddress);
			break;

		case idmenuprocessor:
			ok = menuverbpack (hv, hpacked, flnewdbaddress);
			break;

		case idpictprocessor:
			ok = pictverbpack (hv, hpacked, flnewdbaddress);
			break;

		default:
			ok = false;
			break;
		}

	/* Mode remains in output format - caller will manage further transitions if needed */
	return (ok);
	} /*langexternalpack_internal*/
```

**Key Changes**:
1. Mode switching moved BEFORE the switch statement
2. Call to `ensure_external_in_memory()` instead of relying on child functions
3. Removed mode restoration after switch (mode stays in output format)
4. Added debug logging to track mode transitions
5. Clear comments explaining single decision point pattern

**Validation**:
```bash
# Should compile
make -C tests clean && make -C tests save_migration_tests 2>&1 | grep -E "(error|warning)" | grep -v "unused function"
# Should have no critical errors
```

---

## Step 3: Simplify opverbpack_internal()

### File: Common/source/opverbs.c

**Find**: Function `opverbpack_internal()` starting around line 805

**Current Code** (what to replace):
Look for the function with mode management code like:
- `db_context working_context, legacy_context;`
- `db_context_apply(&legacy_context);`
- `db_context_apply(&working_context);`

**Replace Entire Function With**:
```c
boolean opverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {

	/*
	6.2a15 AR: added flnewdbaddress parameter
	2025-12-20: Pure packing function - NO mode management

	Preconditions:
	  - flinmemory=1 (caller has loaded external into memory)
	  - Global mode set to output format (v7 during migration)

	Postconditions:
	  - Outline packed and address written to *hpacked
	  - Global mode unchanged
	  - Returns true on success, false on failure
	*/

	register hdloutlinevariable hv = (hdloutlinevariable) h;
	register hdloutlinerecord ho;
	register boolean fl;
	Handle hpackedoutline;
	dbaddress adr;
	hdlwindowinfo hinfo;
	boolean fltempload = false;
	boolean adapter_repack;

	adapter_repack = db_format_adapter_force_repack();

	/* Precondition check: external must be in memory */
	if (!(**hv).flinmemory) {
		/* This is a programming error - caller should have loaded it */
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] opverbpack_internal: PRECONDITION VIOLATED - flinmemory=0\n");
#endif
		return (false);
	}

	ho = (hdloutlinerecord) (**hv).variabledata;

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] opverbpack: flinmemory=%d ho=%p oldaddress=0x%llx\n",
	        (int) (**hv).flinmemory, (void *) ho, (unsigned long long) (**hv).oldaddress);
#endif

	opverbcheckwindowrect (ho);

	adr = (**hv).oldaddress; /*place where this outline used to be stored*/

	if (adapter_repack) {
		(**ho).fldirty = true;
		(**ho).fldirtyview = true;
		*flnewdbaddress = true;
	}

	if (!fldatabasesaveas && !(**ho).fldirty && !(**ho).fldirtyview) /*don't need to update the db version of the outline*/
		goto pushaddress;

	if (!opverbpackoutline (ho, &hpackedoutline)) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] opverbpackoutline failed for outline at adr=0x%llx\n",
		        (unsigned long long) (**hv).oldaddress);
#endif
		return (false);
	}

	/* During migration, dbassignhandle will use the global v7 write mode set by caller */
	fl = dbassignhandle (hpackedoutline, &adr);

#if defined(FRONTIER_HEADLESS)
	db_format_mode check_mode = db_format_mode_current();
	fprintf(stderr, "[headless] opverbpack: dbassignhandle oldadr=0x%llx -> newadr=0x%llx (use_64bit=%d)\n",
	        (unsigned long long) (**hv).oldaddress, (unsigned long long) adr, (int) check_mode.use_64bit_format);
#endif

	disposehandle (hpackedoutline);

	if (!fl) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] dbassignhandle failed for outline adr=0x%llx\n",
		        (unsigned long long) (**hv).oldaddress);
#endif
		return (false);
	}

	if (fldatabasesaveas && !fltempload)
		goto pushaddress;

	if (!opwindowopen ((hdlexternalvariable) hv, &hinfo) /* (**ho).flwindowopen*/ ) /*it's been saved, we can reclaim some memory*/
		opverbunload ((hdlexternalvariable) hv, adr);

	else {

		assert (!fltempload);

		(**ho).fldirty = false; /*we just saved off a new db version*/

		(**ho).fldirtyview = false;

		if (hinfo != nil)
			shellsetwindowchanges (hinfo, false);
		}

	pushaddress:
	/* NO mode management - uses whatever mode is currently set */

	if (!fldatabasesaveas) {

		*flnewdbaddress = ((**hv).oldaddress != adr);

		(**hv).oldaddress = adr;
		}
	else
		*flnewdbaddress = true;

	return (pushlongondiskhandle (adr, *hpacked));
	} /*opverbpack_internal*/
```

**Key Changes**:
1. Removed all `db_context` variables (working_context, legacy_context)
2. Removed all `db_context_apply()` calls
3. Added precondition check with clear error message
4. Removed loading logic (caller handles this now)
5. Kept all other logic unchanged (window handling, dirty flags, etc.)

**Validation**:
```bash
# Should compile
make -C tests clean && make -C tests save_migration_tests 2>&1 | grep -i error
```

---

## Step 4: Simplify wpverbpack_internal()

### File: Common/source/wpverbs.c

**Find**: Function `wpverbpack_internal()` starting around line 605

**Replace Entire Function With**:
```c
boolean wpverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {

	/*
	6.2a15 AR: added flnewdbaddress parameter
	2025-12-20: Pure packing function - NO mode management

	Preconditions:
	  - flinmemory=1 (caller has loaded external into memory)
	  - Global mode set to output format (v7 during migration)

	Postconditions:
	  - WP document packed and address written to *hpacked
	  - Global mode unchanged
	  - Returns true on success, false on failure
	*/

	register hdlwpvariable hv = (hdlwpvariable) h;
	register hdlwprecord hwp;
	register boolean fl;
	Handle hpackedwp;
	dbaddress adr;
	hdlwindowinfo hinfo;
	boolean fltempload = false;
	const boolean adapter_repack = db_format_adapter_force_repack();

	/* Precondition check: external must be in memory */
	if (!(**hv).flinmemory) {
		/* This is a programming error - caller should have loaded it */
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] wpverbpack_internal: PRECONDITION VIOLATED - flinmemory=0\n");
#endif
		return (false);
	}

	/* External is in memory - get the data */
	adr = (**hv).oldaddress; /*place where this wp doc used to be stored*/

	if ((**hv).flpacked) { /*no window open, but changes were made*/

		hpackedwp = (Handle) (**hv).variabledata;

		if (!dbassignhandle (hpackedwp, &adr))
			return (false);

		if (fldatabasesaveas)
			goto pushaddress;

		wpverbondisk (hv, adr);

		disposehandle (hpackedwp); /*reclaim memory used by packed doc*/

		goto pushaddress;
		}

	/*the wpdoc is in memory and it's not packed*/

	hwp = (hdlwprecord) (**hv).variabledata;

	wpverbcheckwindowrect (hwp);

	if (adapter_repack) {
		(**hwp).fldirty = true;
		(**hwp).fldirtyview = true;
		/* Enable wide writes for migration - do NOT use context guard version */
		db_format_adapter_enable_wide_writes(NULL);
		*flnewdbaddress = true;
	}

	if (!fldatabasesaveas && !(**hwp).fldirty && !(**hwp).fldirtyview) /*don't need to update the db version of the wpdoc*/
		goto pushaddress;

	if (!wpverbpackrecord (hwp, &hpackedwp))
		return (false);

	fl = dbassignhandle (hpackedwp, &adr);

	disposehandle (hpackedwp);

	if (!fl)
		return (false);

	if (fldatabasesaveas)
		goto pushaddress;

	if (!wpwindowopen ((hdlexternalvariable) hv, &hinfo)) { /*it's been saved, we can reclaim some memory*/

		wpverbondisk (hv, adr);

		wpdisposerecord (hwp); /*reclaim memory used by doc*/
		}
	else {

		assert (!fltempload);

		(**hwp).fldirty = false; /*we just saved off a new db version*/

		(**hwp).fldirtyview = false;

		shellsetwindowchanges (hinfo, false);
		}

	pushaddress:
		/* NO mode management - uses whatever mode is currently set */

		if (!fldatabasesaveas) {

			*flnewdbaddress = ((**hv).oldaddress != adr);

			(**hv).oldaddress = adr;
			}
		else
			*flnewdbaddress = true;

		return (pushlongondiskhandle (adr, *hpacked));
		} /*wpverbpack_internal*/
```

**Key Changes**:
1. Removed all `db_context` variables
2. Removed all mode management code
3. Added precondition check
4. Removed loading logic - assumes flinmemory=1
5. Kept all other logic (flpacked handling, window management, etc.)

**Validation**:
```bash
# Should compile
make -C tests clean && make -C tests save_migration_tests 2>&1 | grep -i error
```

---

## Step 5: Verify tableverbpack_internal()

### File: Common/source/tablepack.c

**Find**: Function `tableverbpack_internal()` starting around line 340

**Check**: This function was already converted in previous work. Verify it follows the pattern:

**Should Look Like**:
```c
boolean tableverbpack_internal (const db_context *ctx, hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
	// Should NOT have db_context_apply() calls inside main logic
	// Should call tablepacktable_internal() with context parameter
	// Should NOT manage mode switching
}
```

**If it has mode management code**: Apply same simplification pattern as steps 3-4.

**Validation**:
```bash
# Check for mode management calls
grep -n "db_context_apply\|db_format_mode_push\|db_format_mode_pop" Common/source/tablepack.c | grep tableverbpack_internal
# Should return empty
```

---

## Step 6: Test Migration Determinism

### Compile
```bash
make -C tests clean
make -C tests save_migration_tests
```

**Expected**: Clean compile with only existing warnings (unused functions, pointer sign)

### Run Migration 3 Times
```bash
# Clean start
rm -f test_run*.root

# Run 1
./tests/save_migration_tests 2>&1 | tee migration_run1.log
cp tests/test_save_migration-v7.root test_run1.root
MD5_1=$(md5 -q test_run1.root)
echo "Run 1 MD5: $MD5_1"

# Run 2
./tests/save_migration_tests 2>&1 | tee migration_run2.log
cp tests/test_save_migration-v7.root test_run2.root
MD5_2=$(md5 -q test_run2.root)
echo "Run 2 MD5: $MD5_2"

# Run 3
./tests/save_migration_tests 2>&1 | tee migration_run3.log
cp tests/test_save_migration-v7.root test_run3.root
MD5_3=$(md5 -q test_run3.root)
echo "Run 3 MD5: $MD5_3"

# Compare
if [ "$MD5_1" = "$MD5_2" ] && [ "$MD5_2" = "$MD5_3" ]; then
    echo "✓ SUCCESS: All three runs produced identical output!"
    echo "Migration is deterministic."
else
    echo "✗ FAILURE: Runs produced different output"
    echo "Comparing run1 vs run2:"
    cmp -l test_run1.root test_run2.root | head -20
fi
```

**Expected Output**:
```
✓ SUCCESS: All three runs produced identical output!
Migration is deterministic.
```

### Check Mode Transition Logs
```bash
# Extract mode changes from run 1
grep "db_format_mode_apply" migration_run1.log > mode_changes_run1.txt

# Check pattern
cat mode_changes_run1.txt | head -20
```

**Expected Pattern**:
```
[headless] db_format_mode_apply use_64bit=0 adapter_repack=0 drop_cancoon=0  # Initial v6 load
[headless] db_format_mode_apply use_64bit=1 adapter_repack=1 drop_cancoon=0  # Switch to v7 write
[headless] db_format_mode_apply use_64bit=1 adapter_repack=1 drop_cancoon=0  # Stays v7
... (many v7 write mode applications, but NO flip-flopping back to v6)
```

**Should NOT see**:
```
[headless] WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!
```

### Verify External Loading Logs
```bash
grep "langexternalpack: loading external" migration_run1.log | head -10
```

**Expected**: Lines showing externals being loaded before packing:
```
[headless] langexternalpack: loading external from v6 id=4
[headless] langexternalpack: loaded, now flinmemory=1
[headless] langexternalpack: mode switched to v7 write
```

### Check for Precondition Violations
```bash
grep "PRECONDITION VIOLATED" migration_run1.log
```

**Expected**: Empty (no violations)

---

## Step 7: Validate Migrated Database

### Load Test
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system)"
```

**Expected Output**:
```
true
```

### External Table Access Test
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.globals)"
```

**Expected**: A number (not an error)

### Deep External Test
```bash
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(workspace.examples)"
```

**Expected Output**:
```
true
```

---

## Troubleshooting Guide

### Problem: Compilation Errors

**Symptom**:
```
error: use of undeclared identifier 'ensure_external_in_memory'
```

**Solution**:
- Check that helper function is added to langexternal.c
- Verify it's declared as `static boolean` (file scope only)
- Verify it's placed BEFORE `langexternalpack_internal()`

---

### Problem: Precondition Violations

**Symptom**:
```
[headless] opverbpack_internal: PRECONDITION VIOLATED - flinmemory=0
```

**Cause**: `ensure_external_in_memory()` failed but caller didn't detect failure

**Solution**:
1. Check langexternalpack_internal error handling:
```c
if (!ensure_external_in_memory(hv)) {
    db_context_apply(&working_context);  // This line must be present
    return (false);  // Must return false on load failure
}
```

2. Check that ensure_external_in_memory returns correct boolean
3. Add more logging to see where loading fails

---

### Problem: Migration Fails

**Symptom**:
```
[migration] FATAL: Migration failed
```

**Debug Steps**:
1. Check last 50 lines before failure:
```bash
./tests/save_migration_tests 2>&1 | tail -50
```

2. Look for specific error:
```bash
./tests/save_migration_tests 2>&1 | grep -i "failed\|error" | tail -20
```

3. If "hashpackvisit_v7 failed":
   - Check which external type is failing
   - Verify that type's pack_internal function is simplified correctly
   - Check if mode is set correctly before packing

---

### Problem: Non-Deterministic Output

**Symptom**:
```
✗ FAILURE: Runs produced different output
```

**Debug Steps**:
1. Find differing bytes:
```bash
cmp -l test_run1.root test_run2.root | head -20
```

2. Check mode log patterns:
```bash
grep "db_format_mode_apply" migration_run1.log > modes1.txt
grep "db_format_mode_apply" migration_run2.log > modes2.txt
diff modes1.txt modes2.txt
```

3. If mode logs differ:
   - There's still hidden mode state somewhere
   - Check that ALL pack_internal functions removed mode management
   - Check that no legacy pack functions are being called

4. If mode logs identical but output differs:
   - May be uninitialized memory
   - May be timestamp differences
   - Check exact byte positions that differ

---

### Problem: Mode Warnings

**Symptom**:
```
[headless] WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!
```

**Cause**: Setting use_64bit=0 with adapter_repack=1 (invalid combination for writing)

**Solution**:
Check legacy_context creation in langexternalpack_internal:
```c
legacy_context.mode.use_64bit_format = false;
legacy_context.mode.adapter_repack = false;  // MUST be false for pure read
```

If still seeing warnings, grep for all db_context_apply calls:
```bash
grep -n "db_context_apply" Common/source/opverbs.c Common/source/wpverbs.c Common/source/tablepack.c
```

Should only find them in tableverbpack_internal (if any), not in opverbs or wpverbs.

---

## Success Criteria Checklist

After completing all steps, verify:

- [ ] `ensure_external_in_memory()` exists in langexternal.c
- [ ] `langexternalpack_internal()` manages mode in one place before switch
- [ ] `opverbpack_internal()` has NO db_context_apply() calls
- [ ] `wpverbpack_internal()` has NO db_context_apply() calls
- [ ] `tableverbpack_internal()` has NO db_context_apply() calls (or minimal)
- [ ] Code compiles cleanly
- [ ] Migration runs successfully
- [ ] Migration produces identical output on 3 consecutive runs
- [ ] No "PRECONDITION VIOLATED" messages in logs
- [ ] No mode warnings in logs
- [ ] Migrated database loads successfully
- [ ] External tables accessible in migrated database
- [ ] Mode transition log shows clean pattern (v6 read → v7 write, no flip-flop)

---

## Commit Message Template

After successful implementation and testing:

```
refactor: Implement single decision point for mode management

- Create ensure_external_in_memory() helper for type dispatch
- Move all mode management to langexternalpack_internal()
- Simplify opverbpack_internal to pure packing operation
- Simplify wpverbpack_internal to pure packing operation
- Remove mode management from all child pack functions

This implements the single decision point principle:
- ONE place decides v6 read mode (langexternalpack_internal)
- ONE place decides v7 write mode (langexternalpack_internal)
- Child functions are pure operations with no mode management

Result: Migration is now 100% deterministic.

Fixes: Mode flip-flopping during recursive packing
See: planning/phase3/MODE_SINGLE_DECISION_POINT.md
```

---

## Next Steps After This Implementation

1. Run full headless test suite:
```bash
./tools/run_headless_tests.sh
```

2. If all tests pass, proceed to Phase 2: Database Operations refactor
3. If any tests fail, debug using troubleshooting guide above
4. Document any edge cases discovered during implementation

---

## Reference Files

- **Planning**: `planning/phase3/MODE_SINGLE_DECISION_POINT.md`
- **Architecture**: `planning/phase3/MODE_STACK_REFACTOR_PLAN.md`
- **Issue History**: `planning/phase3/ISSUE_123_SOLUTION_DESIGN.md`
- **External Variables**: `docs/external_table_variable_management.md`
