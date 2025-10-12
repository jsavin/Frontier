/* test_portable_handles.c - smoke tests for portable Handle API */

#include "../../framework/test_framework.h"
#include "../../../portable/handle.h"
#include <string.h>

bool test_handle_alloc_lock_resize_dup(void) {
    test_setup();
    frontier_handle_t h = frontier_new_handle(16);
    TEST_ASSERT(h != NULL, "new_handle failed");
    void* p = frontier_lock(h);
    TEST_ASSERT(p != NULL, "lock failed");
    memset(p, 0xAB, 16);
    frontier_unlock(h);

    TEST_ASSERT(frontier_set_size(h, 32), "resize failed");
    TEST_ASSERT(frontier_get_size(h) == 32, "size mismatch after resize");

    frontier_handle_t d = frontier_dup(h);
    TEST_ASSERT(d != NULL, "dup failed");
    TEST_ASSERT(frontier_get_size(d) == 32, "dup size mismatch");

    frontier_dispose_handle(d);
    frontier_dispose_handle(h);
    test_teardown();
    TEST_PASS("Portable handle API works");
}


