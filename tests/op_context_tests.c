/*
 * op_context_tests.c - Unit tests for operation context lifecycle and refcounting
 *
 * Tests for Phase 3: Testing Infrastructure
 * Issue #135: Outline Operation Context Pattern
 *
 * This test suite validates:
 * - Context acquisition and release
 * - Reference counting (retain/release)
 * - Version tracking (bump/get)
 * - Validation and invariant checking
 * - Multi-context scenarios
 * - Error conditions
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "op_context.h"
#include "test_report.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  ✗ FAILED: %s\n", msg); \
    } \
} while(0)

#define TEST_SECTION(name) \
    printf("\n%s\n", name); \
    printf("================================================\n")

/* ========== Lifecycle Tests ========== */

static void test_acquire_release(void) {
    TEST_SECTION("Test 1: Basic Acquire/Release");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    TEST_ASSERT(ctx != NULL, "Context acquired successfully");
    TEST_ASSERT(op_context_validate(ctx), "Context is valid after acquire");

    op_context_release(ctx);
    printf("  ✓ Context acquired and released\n");
}

static void test_multiple_acquire(void) {
    TEST_SECTION("Test 2: Multiple Independent Contexts");

    op_context_t *ctx1 = op_context_acquire(OP_CONTEXT_NORMAL);
    op_context_t *ctx2 = op_context_acquire(OP_CONTEXT_NORMAL);
    op_context_t *ctx3 = op_context_acquire(OP_CONTEXT_NORMAL);

    TEST_ASSERT(ctx1 != NULL, "Context 1 acquired");
    TEST_ASSERT(ctx2 != NULL, "Context 2 acquired");
    TEST_ASSERT(ctx3 != NULL, "Context 3 acquired");
    TEST_ASSERT(ctx1 != ctx2, "Context 1 and 2 are different");
    TEST_ASSERT(ctx2 != ctx3, "Context 2 and 3 are different");
    TEST_ASSERT(ctx1 != ctx3, "Context 1 and 3 are different");

    op_context_release(ctx1);
    op_context_release(ctx2);
    op_context_release(ctx3);
    printf("  ✓ Multiple contexts managed independently\n");
}

static void test_null_release(void) {
    TEST_SECTION("Test 3: NULL Release Safety");

    /* Should not crash or assert */
    op_context_release(NULL);
    TEST_ASSERT(1, "NULL release handled safely");
    printf("  ✓ NULL release is safe\n");
}

/* ========== Refcounting Tests ========== */

static void test_retain_release_cycle(void) {
    TEST_SECTION("Test 4: Retain/Release Cycle");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    TEST_ASSERT(ctx != NULL, "Context acquired");

    /* Retain should return the same context */
    op_context_t *ctx_retained = op_context_retain(ctx);
    TEST_ASSERT(ctx_retained == ctx, "Retain returns same context");
    TEST_ASSERT(op_context_validate(ctx), "Context valid after retain");

    /* Release both references */
    op_context_release(ctx);
    op_context_release(ctx_retained);
    printf("  ✓ Retain/release cycle works correctly\n");
}

static void test_multiple_retains(void) {
    TEST_SECTION("Test 5: Multiple Retains");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    TEST_ASSERT(ctx != NULL, "Context acquired");

    /* Retain multiple times */
    op_context_retain(ctx);
    op_context_retain(ctx);
    op_context_retain(ctx);
    TEST_ASSERT(op_context_validate(ctx), "Context valid after multiple retains");

    /* Release all references */
    op_context_release(ctx);
    op_context_release(ctx);
    op_context_release(ctx);
    op_context_release(ctx);
    printf("  ✓ Multiple retains handled correctly\n");
}

static void test_null_retain(void) {
    TEST_SECTION("Test 6: NULL Retain Safety");

    op_context_t *result = op_context_retain(NULL);
    TEST_ASSERT(result == NULL, "Retain(NULL) returns NULL");
    printf("  ✓ NULL retain is safe\n");
}

/* ========== Version Tracking Tests ========== */

static void test_initial_version(void) {
    TEST_SECTION("Test 7: Initial Version Value");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    uint64_t version = op_context_version_get(ctx);
    TEST_ASSERT(version == 0, "Initial version is 0");

    op_context_release(ctx);
    printf("  ✓ Initial version correctly 0\n");
}

static void test_version_bump_sequence(void) {
    TEST_SECTION("Test 8: Version Bump Sequence");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Bump and check version increments */
    TEST_ASSERT(op_context_version_get(ctx) == 0, "Version starts at 0");

    uint64_t v1 = op_context_version_bump(ctx);
    TEST_ASSERT(v1 == 1, "First bump returns 1");
    TEST_ASSERT(op_context_version_get(ctx) == 1, "Version is 1 after first bump");

    uint64_t v2 = op_context_version_bump(ctx);
    TEST_ASSERT(v2 == 2, "Second bump returns 2");
    TEST_ASSERT(op_context_version_get(ctx) == 2, "Version is 2 after second bump");

    uint64_t v3 = op_context_version_bump(ctx);
    TEST_ASSERT(v3 == 3, "Third bump returns 3");
    TEST_ASSERT(op_context_version_get(ctx) == 3, "Version is 3 after third bump");

    op_context_release(ctx);
    printf("  ✓ Version bumps correctly in sequence\n");
}

static void test_version_bump_returns_new(void) {
    TEST_SECTION("Test 9: Version Bump Return Value");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Verify that bump returns the NEW version, not the old one */
    uint64_t old_version = op_context_version_get(ctx);
    uint64_t bump_result = op_context_version_bump(ctx);
    uint64_t new_version = op_context_version_get(ctx);

    TEST_ASSERT(bump_result == old_version + 1, "Bump returns new version");
    TEST_ASSERT(new_version == bump_result, "Get returns same as bump");

    op_context_release(ctx);
    printf("  ✓ Version bump returns new version correctly\n");
}

static void test_independent_version_spaces(void) {
    TEST_SECTION("Test 10: Independent Version Counters");

    op_context_t *ctx1 = op_context_acquire(OP_CONTEXT_NORMAL);
    op_context_t *ctx2 = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Bump context 1 several times */
    op_context_version_bump(ctx1);
    op_context_version_bump(ctx1);
    op_context_version_bump(ctx1);

    /* Context 2 should still be at 0 */
    uint64_t v1 = op_context_version_get(ctx1);
    uint64_t v2 = op_context_version_get(ctx2);

    TEST_ASSERT(v1 == 3, "Context 1 version is 3");
    TEST_ASSERT(v2 == 0, "Context 2 version is still 0");
    TEST_ASSERT(v1 != v2, "Contexts have independent version counters");

    op_context_release(ctx1);
    op_context_release(ctx2);
    printf("  ✓ Version counters are independent per context\n");
}

/* ========== Validation Tests ========== */

static void test_reserved_fields_zero(void) {
    TEST_SECTION("Test 11: Reserved Fields Zero After Acquire");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Check that all 8 reserved pointers are NULL */
    int all_null = 1;
    for (int i = 0; i < 8; i++) {
        if (ctx->reserved[i] != NULL) {
            all_null = 0;
            printf("  ! reserved[%d] is non-NULL: %p\n", i, ctx->reserved[i]);
        }
    }
    TEST_ASSERT(all_null, "All reserved fields are NULL");

    op_context_release(ctx);
    printf("  ✓ Reserved fields properly initialized to NULL\n");
}

static void test_validate_after_acquire(void) {
    TEST_SECTION("Test 12: Validation After Acquire");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    bool valid = op_context_validate(ctx);
    TEST_ASSERT(valid, "Context validates successfully after acquire");

    op_context_release(ctx);
    printf("  ✓ Context validation passes\n");
}

static void test_validate_after_version_bump(void) {
    TEST_SECTION("Test 13: Validation After Version Bump");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    op_context_version_bump(ctx);
    op_context_version_bump(ctx);

    bool valid = op_context_validate(ctx);
    TEST_ASSERT(valid, "Context validates successfully after version bumps");

    op_context_release(ctx);
    printf("  ✓ Validation passes after mutations\n");
}

/* ========== Context Flags Tests ========== */

static void test_context_flags_storage(void) {
    TEST_SECTION("Test 14: Context Flags Storage");

    op_context_t *ctx_normal = op_context_acquire(OP_CONTEXT_NORMAL);
    op_context_t *ctx_readonly = op_context_acquire(OP_CONTEXT_READONLY);
    op_context_t *ctx_undo = op_context_acquire(OP_CONTEXT_UNDO);

    TEST_ASSERT(ctx_normal->flags == OP_CONTEXT_NORMAL, "Normal flag stored");
    TEST_ASSERT(ctx_readonly->flags == OP_CONTEXT_READONLY, "Readonly flag stored");
    TEST_ASSERT(ctx_undo->flags == OP_CONTEXT_UNDO, "Undo flag stored");

    op_context_release(ctx_normal);
    op_context_release(ctx_readonly);
    op_context_release(ctx_undo);
    printf("  ✓ Context flags stored correctly\n");
}

/* ========== Stress Tests ========== */

static void test_many_bumps(void) {
    TEST_SECTION("Test 15: Many Version Bumps");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Do 10,000 bumps */
    for (int i = 0; i < 10000; i++) {
        op_context_version_bump(ctx);
    }

    uint64_t version = op_context_version_get(ctx);
    TEST_ASSERT(version == 10000, "Version correct after 10,000 bumps");

    op_context_release(ctx);
    printf("  ✓ Version bumps scale correctly\n");
}

static void test_many_retains(void) {
    TEST_SECTION("Test 16: Many Retains");

    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

    /* Retain 1000 times */
    for (int i = 0; i < 1000; i++) {
        op_context_retain(ctx);
    }

    bool valid = op_context_validate(ctx);
    TEST_ASSERT(valid, "Context valid after 1000 retains");

    /* Release all 1001 references (acquire + 1000 retains) */
    for (int i = 0; i <= 1000; i++) {
        op_context_release(ctx);
    }
    printf("  ✓ Many retains handled correctly\n");
}

/* ========== Integration Tests ========== */

static void test_mutation_workflow(void) {
    TEST_SECTION("Test 17: Typical Mutation Workflow");

    /* Simulate a typical mutation operation */
    op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
    TEST_ASSERT(ctx != NULL, "Context acquired for mutation");

    /* Perform operation (version bump at start) */
    uint64_t v1 = op_context_version_bump(ctx);
    TEST_ASSERT(v1 == 1, "First mutation bumps to v1");

    /* Nested operation might retain context */
    op_context_t *nested_ctx = op_context_retain(ctx);
    uint64_t v2 = op_context_version_bump(ctx);
    TEST_ASSERT(v2 == 2, "Nested operation bumps to v2");

    /* Release nested reference */
    op_context_release(nested_ctx);

    /* Final release */
    op_context_release(ctx);
    printf("  ✓ Typical mutation workflow works\n");
}

static void test_wrapper_pattern(void) {
    TEST_SECTION("Test 18: Wrapper Pattern Simulation");

    /* Simulate the backward-compat wrapper pattern */
    {
        op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);

        /* Simulate calling a _ctx variant */
        if (ctx != NULL) {
            op_context_version_bump(ctx);
            TEST_ASSERT(op_context_validate(ctx), "Context valid in _ctx variant");
        }

        op_context_release(ctx);
    }

    printf("  ✓ Wrapper pattern simulation works\n");
}

/* ========== Main Test Runner ========== */

int main(void) {
    TR_INIT("op_context_tests");
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Operation Context (op_context_t) Unit Tests              ║\n");
    printf("║  Phase 3: Testing Infrastructure                          ║\n");
    printf("║  Issue #135: Outline Operation Context Pattern            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* Lifecycle Tests */
    TR_RUN(test_acquire_release);
    TR_RUN(test_multiple_acquire);
    TR_RUN(test_null_release);

    /* Refcounting Tests */
    TR_RUN(test_retain_release_cycle);
    TR_RUN(test_multiple_retains);
    TR_RUN(test_null_retain);

    /* Version Tracking Tests */
    TR_RUN(test_initial_version);
    TR_RUN(test_version_bump_sequence);
    TR_RUN(test_version_bump_returns_new);
    TR_RUN(test_independent_version_spaces);

    /* Validation Tests */
    TR_RUN(test_reserved_fields_zero);
    TR_RUN(test_validate_after_acquire);
    TR_RUN(test_validate_after_version_bump);

    /* Flags Tests */
    TR_RUN(test_context_flags_storage);

    /* Stress Tests */
    TR_RUN(test_many_bumps);
    TR_RUN(test_many_retains);

    /* Integration Tests */
    TR_RUN(test_mutation_workflow);
    TR_RUN(test_wrapper_pattern);

    /* Print summary */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Results Summary                                      ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests:    %d\n", tests_run);
    printf("║  Passed:         %d\n", tests_passed);
    printf("║  Failed:         %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("║                                                            ║\n");
        printf("║  🎉 ALL TESTS PASSED! 🎉                                  ║\n");
    } else {
        printf("║                                                            ║\n");
        printf("║  ❌ SOME TESTS FAILED                                     ║\n");
    }
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    TR_SUMMARY();
    return TR_EXIT_CODE();
}
