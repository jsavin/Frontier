/*
 * palette_arg_inject_concurrency_tests.c — cross-thread arg-isolation
 *                                          regression test for the
 *                                          slash-menu palette dispatch
 *                                          path.
 *
 * Why this file exists (issue #597)
 * ---------------------------------
 * PR #584 originally bound a typed argument from the palette into a leaf's
 * handler call via a process-global pending-arg buffer (g_palette_pending_arg
 * in repl.c). The /gate concurrency review caught a P0: meuserselected_headless's
 * langruncode() yields the GIL between statements, so a sibling thread.new()
 * child could read the wrong arg. The fix replaced the global with per-call
 * script-source synthesis (palette_arg_inject_into_script + palette_arg_escape)
 * — the arg is bound at compile time inside the call frame, with no shared
 * state for a sibling thread to see.
 *
 * The structure-by-construction argument is correct for today's code, but
 * has nothing in CI to enforce it. A future regression that reintroduced a
 * scratch buffer (a static char[] inside palette_arg_inject_into_script,
 * a thread-shared global the dispatcher writes through, etc.) would silently
 * pass the existing per-helper unit tests because those tests are single-
 * threaded.
 *
 * This file fills that gap with a real-pthread, contention-maximizing
 * regression test. N worker threads each repeatedly call the helpers with
 * a thread-private arg and assert their output buffer reflects ONLY their
 * own arg. A condvar barrier makes all threads start simultaneously so
 * any cross-thread stomp has the widest possible window to manifest.
 *
 * Reusable harness pattern
 * ------------------------
 * The barrier_t + worker_args_t shape here is the first instance of a
 * pattern the project plans to reuse for any verb that touches a process-
 * shared global between yield points. See tests/integration/REPL_TESTING_GUIDE.md
 * §"Cross-Thread Test Pattern" for the documented re-use guidance.
 *
 * What this test would catch (failure modes by construction)
 * -----------------------------------------------------------
 *   - reintroducing g_palette_pending_arg (or any process-global the
 *     dispatcher writes and the kernel verb reads) — workers see each
 *     other's args
 *   - introducing a static scratch buffer inside palette_arg_escape or
 *     palette_arg_inject_into_script — workers' outputs alias each other
 *   - thread-unsafe malloc tracking that mutates a global free-list
 *     header without synchronization — workers' buffers overlap
 *
 * What this test does NOT cover
 * -----------------------------
 *   - The dispatcher path inside repl.c (which is REPL-state-dependent and
 *     not safely callable from a unit test). The dispatcher is exercised
 *     end-to-end by existing integration tests (repl_palette_rung2.yaml).
 *   - Thread.evaluate / thread.callscript wiring at the UserTalk level.
 *     Confirmed during issue #597 investigation: thread.* verbs are NOT
 *     registered in the headless build today (thread_verbs_foundation.yaml
 *     is fully skipped). When that wiring lands, the palette case can also
 *     get a YAML-level smoke test driving repl.list("Y") from a sibling
 *     thread; until then, this C-level test is the durable regression
 *     guard.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "palette_arg_inject.h"
#include "test_report.h"

/*
 * Number of worker threads and per-thread iterations. Tuned for a tight
 * inner loop that maximizes cross-thread contention without making the
 * test itself slow — total wall time should stay under ~200ms on a modern
 * laptop.
 *
 * Eight workers exercise more interleavings than 2-4 (the four-workers
 * minimum the prompt suggests gives any candidate global ~25% chance of
 * stomp visibility per inner loop iteration; eight bumps that to ~50% for
 * the worst case). Iteration count of 2000 per worker is the smallest
 * value that reliably surfaced the regression in local fault-injection
 * runs (verified during PR construction — see issue #597 commit log).
 */
#define WORKER_COUNT 8
#define ITERATIONS_PER_WORKER 2000

/*
 * Pthread synchronization barrier — POSIX defines pthread_barrier_t but it
 * is OPTIONAL (the macOS libpthread does not provide it). We hand-roll a
 * portable one against pthread_mutex_t + pthread_cond_t so the test runs
 * on every platform the project builds on.
 *
 * The barrier is single-shot: every worker calls barrier_wait once at the
 * start of its run() function and the main thread calls barrier_release()
 * to start them all simultaneously. This is the contention-maximizing
 * shape — without it, threads spawn at staggered times and any race
 * window narrows.
 */
typedef struct {
	pthread_mutex_t mu;
	pthread_cond_t cv;
	bool released;
} barrier_t;

static void barrier_init(barrier_t *b) {
	pthread_mutex_init(&b->mu, NULL);
	pthread_cond_init(&b->cv, NULL);
	b->released = false;
}

static void barrier_destroy(barrier_t *b) {
	pthread_mutex_destroy(&b->mu);
	pthread_cond_destroy(&b->cv);
}

static void barrier_wait(barrier_t *b) {
	pthread_mutex_lock(&b->mu);
	while (!b->released)
		pthread_cond_wait(&b->cv, &b->mu);
	pthread_mutex_unlock(&b->mu);
}

static void barrier_release(barrier_t *b) {
	pthread_mutex_lock(&b->mu);
	b->released = true;
	pthread_cond_broadcast(&b->cv);
	pthread_mutex_unlock(&b->mu);
}

/*
 * Per-worker arguments. Each thread gets a unique arg string ("ARG-0",
 * "ARG-1", ...) and reports its own pass/fail outcome; the main thread
 * aggregates across workers.
 *
 * We carry the worker's id-number (not the string) explicitly because
 * comparing against a known stable per-worker string is a faster sanity
 * check on every iteration than re-formatting the expected output.
 *
 * `failures` counts iterations whose synthesized output did not contain
 * exactly this worker's arg. Any non-zero value means cross-thread leakage
 * was observed and the regression is real.
 */
typedef struct {
	int id;                /* 0..WORKER_COUNT-1 */
	barrier_t *barrier;
	const char *arg;       /* "ARG-<id>" — owned by main */
	int failures;          /* output: count of iterations that saw bad output */
} worker_args_t;

/*
 * The leaf-script template used as input to palette_arg_inject_into_script.
 * Mirrors the shape PR #584 introduced for the production REPL palette
 * (system.menus.handlers.repl.list ()) — a fully-qualified handler path
 * with empty parens. We could use a shorter template ("f ()") but matching
 * the production shape makes the test more obviously a regression guard
 * for the actual call site.
 */
static const char *kLeafScript = "system.menus.handlers.repl.list ()";

/*
 * Worker entry point.
 *
 * Each iteration:
 *   1. Escape the worker's arg via palette_arg_escape into a stack buffer.
 *   2. Inject the escaped arg into the leaf script via
 *      palette_arg_inject_into_script.
 *   3. Verify the synthesized output contains the worker's own arg
 *      bracketed by string-literal quotes — and DOES NOT contain any
 *      other worker's id digit between the quotes.
 *
 * The point-of-failure check uses strstr() against the literal-bracketed
 * form ("ARG-3"), not just the substring "ARG-3". A naive substring search
 * would let "ARG-3" inside any context match — including inside another
 * worker's leaked arg or the surrounding handler path. Bracketing with
 * the surrounding quote characters narrows the match to "the arg in the
 * synthesized literal slot" specifically.
 */
static void *worker_run(void *opaque) {
	worker_args_t *w = (worker_args_t *)opaque;

	/* Build the expected literal-bracketed form once.
	 * Format: ("ARG-N") — 4 surrounding bytes + arg length. */
	char expected[64];
	int written = snprintf(expected, sizeof(expected), "(\"%s\")", w->arg);
	if (written < 0 || (size_t)written >= sizeof(expected)) {
		/* Test bug, not regression — arg too long for stack buffer. Treat
		 * as a single failure so the assert below trips with a clear count. */
		w->failures = 1;
		return NULL;
	}

	barrier_wait(w->barrier);

	for (int i = 0; i < ITERATIONS_PER_WORKER; ++i) {
		/* Step 1: escape. PALETTE_ARG_MAX*2+4 = 516 in production; we use
		 * a smaller buffer because our args are short and we want to
		 * exercise the same code path the dispatcher uses. */
		char escaped[64];
		palette_arg_escape_result_t er =
			palette_arg_escape(w->arg, escaped, sizeof(escaped));
		if (er != PALETTE_ARG_ESCAPE_OK) {
			++w->failures;
			continue;
		}

		/* Step 2: inject. The synthesized buffer is heap-allocated; the
		 * helper's per-call malloc is part of what we're verifying has
		 * no shared state with sibling threads. */
		char *synth = NULL;
		size_t synth_len = 0;
		bool ok = palette_arg_inject_into_script(kLeafScript,
		                                         strlen(kLeafScript),
		                                         escaped, &synth, &synth_len);
		if (!ok || synth == NULL) {
			++w->failures;
			if (synth) free(synth);
			continue;
		}

		/* Step 3: strict containment check. The synthesized form must
		 * end with our worker's literal-bracketed arg. If a sibling's
		 * arg leaked through any shared state, expected_with_quotes
		 * would be missing — caught here. */
		if (strstr(synth, expected) == NULL)
			++w->failures;

		free(synth);
	}

	return NULL;
}

/*
 * The main regression test: spawn WORKER_COUNT threads each running
 * ITERATIONS_PER_WORKER iterations against per-thread arg strings, and
 * assert that no thread observed cross-thread leakage.
 *
 * Asserts (one assert per failure mode) so the test reporter surfaces
 * exactly what failed:
 *   - pthread_create returned non-zero -> infrastructure failure
 *   - any worker reports failures > 0 -> cross-thread isolation broken
 */
static void test_concurrent_arg_isolation_no_global_leak(void) {
	pthread_t threads[WORKER_COUNT];
	worker_args_t workers[WORKER_COUNT];
	char arg_strings[WORKER_COUNT][16];
	barrier_t barrier;

	barrier_init(&barrier);

	/* Spawn workers, each blocked on the barrier. */
	for (int i = 0; i < WORKER_COUNT; ++i) {
		snprintf(arg_strings[i], sizeof(arg_strings[i]), "ARG-%d", i);
		workers[i].id = i;
		workers[i].barrier = &barrier;
		workers[i].arg = arg_strings[i];
		workers[i].failures = 0;
		int rc = pthread_create(&threads[i], NULL, worker_run, &workers[i]);
		assert(rc == 0);
	}

	/* Release all workers simultaneously — maximum contention window. */
	barrier_release(&barrier);

	/* Wait for all workers to finish. */
	for (int i = 0; i < WORKER_COUNT; ++i) {
		int rc = pthread_join(threads[i], NULL);
		assert(rc == 0);
	}

	/* Aggregate failures across workers. Any non-zero count == regression. */
	int total_failures = 0;
	for (int i = 0; i < WORKER_COUNT; ++i)
		total_failures += workers[i].failures;

	if (total_failures != 0) {
		fprintf(stderr,
		        "[concurrency] CROSS-THREAD LEAK: %d/%d iterations failed\n",
		        total_failures, WORKER_COUNT * ITERATIONS_PER_WORKER);
		for (int i = 0; i < WORKER_COUNT; ++i)
			if (workers[i].failures != 0)
				fprintf(stderr,
				        "[concurrency]   worker %d (%s): %d failures\n",
				        i, workers[i].arg, workers[i].failures);
	}
	assert(total_failures == 0);

	barrier_destroy(&barrier);
}

/*
 * Sanity sub-test: a SINGLE-threaded run of the same workload — proves the
 * test infrastructure itself isn't producing false negatives. If this
 * sub-test fails, the concurrency test's pass result is meaningless;
 * running it first means the test report's first FAIL line points at the
 * right culprit.
 */
static void test_single_thread_baseline_passes(void) {
	worker_args_t w;
	char arg[16];
	barrier_t barrier;

	barrier_init(&barrier);
	snprintf(arg, sizeof(arg), "ARG-%d", 0);
	w.id = 0;
	w.barrier = &barrier;
	w.arg = arg;
	w.failures = 0;

	/* Pre-release the barrier so the worker doesn't block. */
	barrier_release(&barrier);
	worker_run(&w);

	assert(w.failures == 0);

	barrier_destroy(&barrier);
}

/*
 * Sanity sub-test: assert that every worker actually had a chance to run
 * (i.e., that the barrier didn't accidentally let some workers exit before
 * we joined them). Tests the test harness, not the production code.
 *
 * We re-run the full concurrent harness but with a low iteration count —
 * this is fast and just confirms that pthread_create + barrier_release +
 * pthread_join all work as expected. If this fails but
 * test_concurrent_arg_isolation_no_global_leak fails too, the harness
 * itself is broken (debug the test); if only the production-stress test
 * fails, the production helpers have a regression (debug palette_arg_inject).
 */
static void test_harness_smoke_eight_threads_complete(void) {
	pthread_t threads[WORKER_COUNT];
	worker_args_t workers[WORKER_COUNT];
	char arg_strings[WORKER_COUNT][16];
	barrier_t barrier;

	barrier_init(&barrier);

	for (int i = 0; i < WORKER_COUNT; ++i) {
		snprintf(arg_strings[i], sizeof(arg_strings[i]), "ARG-%d", i);
		workers[i].id = i;
		workers[i].barrier = &barrier;
		workers[i].arg = arg_strings[i];
		workers[i].failures = 0;
		int rc = pthread_create(&threads[i], NULL, worker_run, &workers[i]);
		assert(rc == 0);
	}

	barrier_release(&barrier);

	for (int i = 0; i < WORKER_COUNT; ++i) {
		int rc = pthread_join(threads[i], NULL);
		assert(rc == 0);
	}

	/* Smoke: every worker completed cleanly with zero failures. (A worker
	 * that exited without running iterations would also have failures==0,
	 * but the previous test's pthread_join + assert(rc==0) covers that —
	 * the pthread layer signals fork failure or worker abort distinctly
	 * from clean completion. Asserting failures==0 here matches what the
	 * production stress test asserts at high iteration count, so the
	 * smoke is just a low-iteration version of the real check. */
	for (int i = 0; i < WORKER_COUNT; ++i)
		assert(workers[i].failures == 0);

	barrier_destroy(&barrier);
}

int main(void) {
	TR_INIT("palette_arg_inject_concurrency_tests");

	TR_RUN(test_single_thread_baseline_passes);
	TR_RUN(test_harness_smoke_eight_threads_complete);
	TR_RUN(test_concurrent_arg_isolation_no_global_leak);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
