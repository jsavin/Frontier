/* 2025-10-31 Codex: Added coverage for ClassicMemError/ClassicMaxBlock tracking. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "portable_handles.h"
#include "classic_handle.h"

static void test_basic_allocation(void) {
    Handle h = frontierAlloc(16);
    assert(h != NULL);
    assert(frontierSize(h) == 16);

    char *data = frontierLock(h);
    assert(data != NULL);
    memset(data, 0x41, 16);
    frontierUnlock(h);

    Handle resized = frontierReAlloc(h, 64);
    assert(resized == h);
    assert(frontierSize(h) == 64);

    /* verify the newly extended portion is zero-filled */
    data = frontierLock(h);
    for (int i = 16; i < 64; ++i) {
        assert(data[i] == 0);
    }
    frontierUnlock(h);

    frontierFree(h);
}

static void test_realloc_zero(void) {
    Handle h = frontierAlloc(32);
    assert(h != NULL);
    assert(frontierSize(h) == 32);

    frontierReAlloc(h, 0);
    assert(frontierSize(h) == 0);

    frontierFree(h);
}

static void test_temp_handle(void) {
    OSErr err = 0;
    Handle h = TempNewHandle(8, &err);
    assert(h != NULL && err == 0);

    frontierFree(h);
}

static void test_nested_locks(void) {
    Handle h = frontierAlloc(4);
    assert(h);
    char *first = frontierLock(h);
    assert(first);
    char *second = frontierLock(h);
    assert(second == first);
    frontierUnlock(h);
    frontierUnlock(h);
    frontierFree(h);
}

static void test_mem_error_tracking(void) {
    ClassicResetHeapTracking();

    Handle h = ClassicNewHandle(128);
    assert(h != NULL);
    assert(ClassicMemError() == noErr);

    Handle huge = ClassicNewHandle((size_t)SIZE_MAX);
    assert(huge == NULL);
    assert(ClassicMemError() == memFullErr);

    ClassicDisposeHandle(h);

    Handle retry = ClassicNewHandle(256);
    assert(retry != NULL);
    assert(ClassicMemError() == noErr);
    ClassicDisposeHandle(retry);
}

static void test_max_block_hint(void) {
    ClassicResetHeapTracking();
    long initial = ClassicMaxBlock();
    assert(initial > 0);

    Handle h = ClassicNewHandle(512 * 1024);
    assert(h != NULL);
    long after_success = ClassicMaxBlock();
    assert(after_success >= initial);

    Handle huge = ClassicNewHandle((size_t)SIZE_MAX);
    assert(huge == NULL);
    long after_failure = ClassicMaxBlock();
    assert(after_failure > 0);

    ClassicDisposeHandle(h);
}

int main(void) {
    test_basic_allocation();
    test_realloc_zero();
    test_temp_handle();
    test_nested_locks();
    test_mem_error_tracking();
    test_max_block_hint();

    printf("portable handle tests passed\n");
    return 0;
}
