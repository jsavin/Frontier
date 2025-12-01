#ifndef TEST_MIGRATION_SHIM_H
#define TEST_MIGRATION_SHIM_H

#include "../../portable/file_portable.h"
boolean test_copy_file(const char *src, const char *dst);
void test_remove_if_exists(const char *path);

#endif /* TEST_MIGRATION_SHIM_H */
