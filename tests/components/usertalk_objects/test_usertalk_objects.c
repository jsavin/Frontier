/* Standalone main for usertalk object tests */

#include "../../framework/test_framework.h"

bool run_all_usertalk_object_tests(void);

int main(void) {
    bool ok = run_all_usertalk_object_tests();
    return ok ? 0 : 1;
}


