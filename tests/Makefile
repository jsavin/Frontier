# Frontier Test Suite Makefile
# 2025-10-27 Codex: Link db_format sources into portable component tests (needed by migration suite).
# 2025-11-07 Codex: Add portable include path for core/db_format tests so headless
#                   builds can find classic_handle and friends.
# 2025-11-11 Codex: Define HEADLESS_LINKS_REAL_PAIGE so tests skip Paige stubs once the real library links.
# 2025-11-17 Codex: Hook Paige's CMake build into this Makefile so source edits retrigger libpaige.a.
# 2025-12-01 Codex: Link real tableexternal code for migration tests to avoid stubbed table lookups.
# Phase 1.1: Comprehensive Testing Implementation

# Include shared headless verb file list
include headless_verbs.mk

CC = clang
# Opt-in sanitizers: set SANITIZE=1 to enable ASan/UBSan for easier debugging
SANITIZE ?= 0
SAN_FLAGS =
ifeq ($(SANITIZE),1)
  SAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
  # macOS leak detection can be noisy; disable unless needed
  # export ASAN_OPTIONS=detect_leaks=0  # set in shell if desired
endif

# Enable Paige debug instrumentation + wp runtime guards when tests are built
FRONTIER_TESTS ?= 1
CWARNING_SUPPRESS ?= -Wno-nonportable-include-path -Wno-error=implicit-function-declaration  # Suppress Paige header case warnings (third-party code)
CFLAGS = -Wall -Wextra -std=c99 -g -O0 -fpascal-strings -Wno-deprecated-non-prototype -fcommon \
         -DFRONTIER_USE_PORTABLE_HANDLES -DFRONTIER_HEADLESS \
         -DFRONTIER_PORTABLE_STRINGS \
         -DFRONTIER_PORTABLE_FILE_AVAILABLE -DFRONTIER_PORTABLE_DB_AVAILABLE \
         -DFRONTIER_PORTABLE_APPLEEVENTS \
         -DHEADLESS_LINKS_REAL_PAIGE \
         -DHEADLESS_LINKS_REAL_DB \
         -DHEADLESS_USE_REAL_TABLEPACK \
         -DFRONTIER_DISABLE_DOCINFO=1 \
         -DLAND_GENERALLOG_LEVEL=LAND_LOGLEVEL_OFF -DLAND_GENERALLOG_TARGET=LAND_LOGTARGET_NONE \
         -DUNIX_COMPILE -DC_LIBRARY -DNO_OS_INLINE \
         $(SAN_FLAGS) \
         $(CWARNING_SUPPRESS) \
         -I../generated
STRINGS_COMPILER_DIR = ../tools/strings_compiler
STRINGS_COMPILER = $(STRINGS_COMPILER_DIR)/strings_compiler
STRINGS_INPUTS = $(wildcard ../resources/strings/*.yaml)
GENERATED_DIR = ../generated
STRINGS_C = $(GENERATED_DIR)/strings_tables.c
STRINGS_H = $(GENERATED_DIR)/strings_tables.h
STRINGS_MANIFEST = $(GENERATED_DIR)/strings_manifest.json
GENERATED_STRINGS = $(STRINGS_C) $(STRINGS_H) $(STRINGS_MANIFEST)
FRONTIER_CLI_DIR = ../frontier-cli
SYSTEMHEADERDIR = ../Common/SystemHeaders
HEADERDIR = ../Common/headers
FRAMEWORKS = -framework CoreFoundation -framework Carbon

# Paige integration (WPText engine)
THIRDPARTYDIR = ../third_party
PAIGE_DIR = $(THIRDPARTYDIR)/Paige
PAIGE_BUILD_DIR_RELEASE = $(PAIGE_DIR)/build-headless
PAIGE_BUILD_DIR_DEBUG = $(PAIGE_DIR)/build-headless-debug
ifeq ($(FRONTIER_TESTS),1)
  PAIGE_BUILD_DIR = $(PAIGE_BUILD_DIR_DEBUG)
  PAIGE_BUILD_TYPE = Debug
  PAIGE_TESTS_FLAG = ON
else
  PAIGE_BUILD_DIR = $(PAIGE_BUILD_DIR_RELEASE)
  PAIGE_BUILD_TYPE = Release
  PAIGE_TESTS_FLAG = OFF
endif
PAIGE_LIB = $(PAIGE_BUILD_DIR)/libpaige.a
CMAKE_BIN = $(THIRDPARTYDIR)/cmake-install/bin/cmake
PAIGE_ARCHES = arm64;x86_64
PAIGE_CONFIG = $(PAIGE_BUILD_DIR)/CMakeCache.txt
PAIGE_SOURCES := $(shell find $(PAIGE_DIR) -type f \( -name '*.C' -o -name '*.CPP' -o -name '*.c' -o -name '*.cpp' -o -name '*.H' -o -name '*.h' \))

CFLAGS += -I$(PAIGE_DIR)/PGHEADER
ifeq ($(FRONTIER_TESTS),1)
  CFLAGS += -DFRONTIER_TESTS=1
endif
LIBS = $(PAIGE_LIB)
LDFLAGS = -Wl,-undefined,dynamic_lookup
LINK_LIBS = $(PAIGE_LIB) $(LDFLAGS)

# Frontier core source files (minimal set suitable for CLI/testing)
# Portable core sources (subset)
FRONTIER_SOURCES = \
    ../Common/source/langscan.c \
    ../Common/source/langparser.c \
    ../Common/source/langvalue.c \
    ../Common/source/langevaluate.c \
    ../Common/source/lang.c \
    ../Common/source/langhash.c \
    ../Common/source/memory.c \
    ../Common/source/strings.c \
    ../Common/source/ops.c \
    ../Common/source/tableops.c \
    ../Common/source/tableexternal_common.c \
    ../Common/source/tablepack.c \
    ../Common/source/legacy/tablepack_legacy.c \
    ../Common/source/tablestructure.c \
    ../Common/source/db.c \
    ../Common/source/dbverbs.c \
    ../Common/source/db_format.c \
    ../Common/source/db_reader_legacy.c \
    ../Common/source/db_reader_v7.c \
    ../Common/source/db_writer_v7.c \
    ../Common/source/timedate.c

# Migration tests use the same headless runtime slice as runtime_tests to avoid test-only forks.
MIGRATION_SOURCES = \
    $(LANG_RUNTIME_SOURCES) \
    headless_shell.c \
    components/test_migration_portable.c

LANG_RUNTIME_SOURCES = \
    ../Common/source/logging.c \
    ../Common/source/lang.c \
    ../Common/source/langcallbacks.c \
    ../Common/source/langcrypt.c \
    ../Common/source/langerror.c \
    ../Common/source/langscan.c \
    ../Common/source/langparser.c \
    ../Common/source/langvalue.c \
    ../Common/source/langevaluate.c \
    ../Common/source/langhash.c \
    ../Common/source/langlist.c \
    ../Common/source/langmath.c \
    ../Common/source/langops.c \
    ../Common/source/langpack.c \
    \
    ../Common/source/langexternal.c \
    ../Common/source/legacy/langexternal_legacy.c \
    ../Common/source/langstartup.c \
    ../Common/source/langverbs.c \
    ../Common/source/langwarnings.c \
    ../Common/source/langtmpstack.c \
    ../Common/source/langtrace.c \
    ../Common/source/langtree.c \
    ../Common/source/langdate.c \
    ../Common/source/timedate.c \
    ../Common/source/langsystypes.c \
    ../Common/source/kernel_verbs_headless.c \
    ../Common/source/memory.c \
    ../Common/source/portable_handles.c \
    ../Common/source/strings.c \
    ../portable/time_portable.c \
    ../Common/source/md5.c \
    ../Common/source/sha1dgst.c \
    ../Common/source/whirlpool.c \
    ../Common/source/strings_extras.c \
    ../portable/text_encoding_portable.c \
    ../Common/source/ops.c \
    ../Common/source/op.c \
    ../Common/source/opinit.c \
    ../Common/source/opexpand.c \
    ../Common/source/opvisit.c \
    ../Common/source/opedit.c \
    ../Common/source/opops.c \
    ../Common/source/oplist.c \
    ../Common/source/opstructure.c \
    ../Common/source/oprefcon.c \
    ../Common/source/op_context.c \
    ../Common/source/oppack_v7.c \
    ../Common/source/legacy/oppack_legacy.c \
    ../Common/source/opverbs.c \
    ../Common/source/legacy/opverbs_legacy.c \
    ../Common/source/opxml.c \
    ../Common/source/langxml.c \
    $(HEADLESS_VERBS_SOURCES) \
    ../Common/source/tableops.c \
    ../Common/source/tableexternal.c \
    ../Common/source/tableexternal_common.c \
    ../Common/source/legacy/tablepack_legacy.c \
    ../Common/source/tablepack.c \
    ../Common/source/tablestructure.c \
    ../Common/source/tableverbs.c \
    ../Common/source/dbverbs.c \
    ../Common/source/db.c \
    ../Common/source/db_format.c \
    ../Common/source/db_reader_legacy.c \
    ../Common/source/db_reader_v7.c \
    ../Common/source/db_writer_v7.c \
    ../Common/source/shell_api_headless.c \
    ../Common/source/shell_api.c \
    ../Common/source/headless_stubs.c \
    ../portable/file_portable.c \
    headless_lang_runtime_more_stubs.c \
    headless_shellhooks.c \
    headless_table_stubs.c \
    ../portable/wptext_runtime.c \
    ../portable/paige_text_extractor.c \
    headless_pict_stubs.c \
    headless_menu_stubs.c \
    headless_search_stubs.c \
    headless_mac_compat.c \
    headless_langregexp_stub.c \
    headless_langipc_stub.c
LANG_RUNTIME_SOURCES += \
    ../portable/classic_handle.c \
    ../portable/appleevent_portable.c \
    ../portable/script_portable.c \
    ../portable/wptext_portable.c \
    ../generated/strings_tables.c

# CLI source files
CLI_SOURCES =
CLI_BIN = ../frontier-cli/frontier-cli

# Test framework
TEST_FRAMEWORK = framework/test_framework.c

# Test executables (aspirational; many may not exist on this branch)
# Keep the original list for reference, but do not depend on it for the test run.
TEST_EXECUTABLES = \
	test_usertalk_objects \
	test_cli_integration \
	test_database \
	test_migration \
	test_compatibility \
	test_64bit_structures \
	test_dbnew \
	paige_text_tests \
	db_format_tests

# UserTalk object test executables
USERTALK_OBJECT_TESTS = \
	test_basic_types \
	test_collections \
	test_complex_objects \
	test_database_persistence \
	test_table_sorting \
	test_usertalk_runner

# Buildable test executables on this branch
RUN_BUILDABLE = core_tests db_format_tests runtime_tests parser_tests save_migration_tests test_efp_augmentation file_portable_tests file_readline_tests file_verb_tests handle_tests cli_runtime_tests paige_text_tests oppack_tests hashpack_modern_tests hash_corruption_tests langvalue_64_tests table_operations_integration refcon_tests refcon_phase2_tests op_context_tests table_context_tests time_portability_test test_sys_shell_command_verbs headless_selection_tests opattributes_stub_tests headless_thread_registry_tests test_callback_infrastructure
# Note: table_verb_tests skipped - pre-existing macOS path detection issue
# Note: test_error_messages skipped - needs full runtime linking (tracked in separate task)
# Note: test_table_sorting skipped - script execution errors cause segfault (Issue #219 tracks verb runtime fixes)

# Prebuilt or auxiliary test binaries that may be present
RUN_PREBUILT = test_64bit_direct test_migration_verification test_real_databases test_runner verify_migration_success

# All test executables (for reference only)
ALL_TESTS = $(TEST_EXECUTABLES) $(USERTALK_OBJECT_TESTS)

# Default target: build the tests we know are supported here
all: $(PAIGE_LIB) strings_generated $(RUN_BUILDABLE)

$(RUN_BUILDABLE): $(PAIGE_LIB)

# Headless smoke test
core_tests: core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c ../Common/source/logging.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c ../Common/source/logging.c headless_shell.c -o $@ $(LINK_LIBS)

op_context_tests: op_context_tests.c ../Common/source/op_context.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable op_context_tests.c ../Common/source/op_context.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

table_context_tests: table_context/table_context_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../portable table_context/table_context_tests.c -o $@ $(LINK_LIBS)

time_portability_test: time_portability_test.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable time_portability_test.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

test_sys_shell_command_verbs: test_sys_shell_command_verbs.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		test_sys_shell_command_verbs.c $(LANG_RUNTIME_SOURCES) headless_shell.c \
		../Common/source/sysshellcall.c -o $@ $(LINK_LIBS)

headless_selection_tests: headless_selection_tests.c ../Common/source/headless_selection.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		headless_selection_tests.c ../Common/source/headless_selection.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

db_format_tests: db_format_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		db_format_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

oppack_tests: oppack_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		oppack_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

hashpack_modern_tests: hashpack_modern_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		hashpack_modern_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

hash_corruption_tests: hash_corruption_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		hash_corruption_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

langvalue_64_tests: langvalue_64_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		langvalue_64_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

table_verb_tests: table_operations/table_verb_tests.c
	$(CC) -std=c99 -Wall -Wextra -g -O0 table_operations/table_verb_tests.c -o $@

table_operations_integration: table_operations/table_operations_integration.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		table_operations/table_operations_integration.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

refcon_tests: refcon_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		refcon_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

refcon_phase2_tests: refcon_cli_tests/phase2_serialization.c
	$(CC) -std=c99 -Wall -Wextra -g -O0 refcon_cli_tests/phase2_serialization.c -o $@

opattributes_stub_tests: opattributes_stub_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		opattributes_stub_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

# Thread safety Layer 1 tests - standalone, no full runtime needed
headless_thread_registry_tests: headless_thread_registry_tests.c ../Common/source/threadregistry.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		headless_thread_registry_tests.c ../Common/source/threadregistry.c ../Common/source/logging.c -o $@ -lpthread

# P0a: Parameterized callback infrastructure tests
test_callback_infrastructure: test_callback_infrastructure.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		test_callback_infrastructure.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

runtime_tests: runtime_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable runtime_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

parser_tests: parser_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable parser_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@ $(LINK_LIBS)

save_migration_tests: save_migration_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable save_migration_tests.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c -o $@ $(LINK_LIBS)

refcon_migration_64bit_test: refcon_migration_64bit_test.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable refcon_migration_64bit_test.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c -o $@ $(LINK_LIBS)

test_efp_augmentation: test_efp_augmentation.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable test_efp_augmentation.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c -o $@ $(LINK_LIBS)

test_dump_system_paths: test_dump_system_paths.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable test_dump_system_paths.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c -o $@ $(LINK_LIBS)

file_portable_tests: file_portable_tests.c ../portable/file_portable.c headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_portable_tests.c ../portable/file_portable.c headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

file_readline_tests: file_readline_tests.c ../portable/file_portable.c headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_readline_tests.c ../portable/file_portable.c headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

file_verb_tests: file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c headless_shell.c -o $@ $(LINK_LIBS)

handle_tests: handle_tests.c ../Common/source/portable_handles.c ../portable/classic_handle.c
	$(CC) $(CFLAGS) -DFRONTIER_USE_PORTABLE_HANDLES -I../Common/headers -I../Common/SystemHeaders -I../portable \
		handle_tests.c ../Common/source/portable_handles.c ../portable/classic_handle.c -o $@ $(LINK_LIBS)

test_logging: test_logging.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders test_logging.c ../Common/source/logging.c -o $@

test_error_messages: test_error_messages.c ../Common/source/logging.c ../Common/source/strings.c headless_lang_runtime_more_stubs.c ../generated/strings_tables.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable -I../generated \
		test_error_messages.c ../Common/source/logging.c ../Common/source/strings.c headless_lang_runtime_more_stubs.c ../generated/strings_tables.c -o $@

cli_runtime_tests: cli_runtime_tests.c $(CLI_BIN)
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders cli_runtime_tests.c -o $@ $(LINK_LIBS)

$(CLI_BIN):
	$(MAKE) -C ../frontier-cli frontier-cli

# Test framework object
framework/test_framework.o: framework/test_framework.c framework/test_framework.h
	$(CC) $(CFLAGS) -c $< -o $@

# Main test executables
test_usertalk_objects: components/test_usertalk_objects.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_cli_integration: components/test_cli_integration.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_database: components/test_database.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS)

test_migration: components/test_migration.c $(TEST_FRAMEWORK) $(MIGRATION_SOURCES)
	$(CC) $(CFLAGS) -DTEST_MIGRATION_HAS_RUNTIME=1 -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS)

test_compatibility: components/test_compatibility.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS)

test_64bit_structures: components/test_64bit_structures.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS)

test_dbnew: components/test_dbnew.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -I$(SYSTEMHEADERDIR) -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS) $(FRAMEWORKS)

paige_text_tests: components/paige_text_tests.c $(TEST_FRAMEWORK) components/macromantoutf8_stub.c components/newclearhandle_stub.c ../Common/source/portable_handles.c ../portable/classic_handle.c ../portable/paige_text_extractor.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -I../Common/headers -I../portable components/paige_text_tests.c framework/test_framework.c components/macromantoutf8_stub.c components/newclearhandle_stub.c ../Common/source/portable_handles.c ../portable/classic_handle.c ../portable/paige_text_extractor.c ../Common/source/logging.c -DFRONTIER_PORTABLE -o $@ $(LDFLAGS)


# UserTalk object test executables
test_usertalk_runner: components/usertalk_objects/test_usertalk_objects.c components/usertalk_objects/test_runner.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@ $(LINK_LIBS)

test_basic_types: components/usertalk_objects/test_basic_types.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_collections: components/usertalk_objects/test_collections.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_complex_objects: components/usertalk_objects/test_complex_objects.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_database_persistence: components/usertalk_objects/test_database_persistence.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@ $(LINK_LIBS)

test_table_sorting: components/usertalk_objects/test_table_sorting.c $(TEST_FRAMEWORK) $(LANG_RUNTIME_SOURCES) ../portable/cli_executor.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable components/usertalk_objects/test_table_sorting.c $(TEST_FRAMEWORK) $(LANG_RUNTIME_SOURCES) ../portable/cli_executor.c headless_shell.c -o $@ $(LINK_LIBS)


# Test targets
test: strings_generated $(RUN_BUILDABLE)
	@echo "Running all tests..."
	@set -e; \
	for test in $(RUN_BUILDABLE) $(RUN_PREBUILT); do \
	  if [ -x "./$$test" ]; then \
	    echo "Running $$test..."; \
	    ./$$test; \
	  else \
	    echo "Skipping $$test (not present)"; \
	  fi; \
	done

test-usertalk: test_usertalk_runner
	@echo "Running UserTalk object tests..."
	./test_usertalk_runner

test-basic: test_basic_types
	@echo "Running basic type tests..."
	./test_basic_types

test-collections: test_collections
	@echo "Running collection tests..."
	./test_collections

test-complex: test_complex_objects
	@echo "Running complex object tests..."
	./test_complex_objects

test-db-persistence: test_database_persistence
	@echo "Running database persistence tests..."
	./test_database_persistence

test-cli: test_cli_integration
	@echo "Running CLI integration tests..."
	./test_cli_integration

test-db: test_database
	@echo "Running database tests..."
	./test_database

test-migration: test_migration
	@echo "Running migration tests..."
	./test_migration

test-compat: test_compatibility
	@echo "Running compatibility tests..."
	./test_compatibility

# Integration test targets (Python-based YAML tests)
test-integration:
	@echo "Running integration tests..."
	@../tools/run_integration_tests.sh

test-integration-verbose:
	@echo "Running integration tests (verbose)..."
	@../tools/run_integration_tests.sh --verbose

# Run all tests (unit + integration)
test-all: test test-integration
	@echo "All test suites completed"

$(STRINGS_COMPILER):
	$(MAKE) -C $(STRINGS_COMPILER_DIR)

$(GENERATED_STRINGS): $(STRINGS_COMPILER) $(STRINGS_INPUTS)
	@mkdir -p $(GENERATED_DIR)
	@if [ -z "$(STRINGS_INPUTS)" ]; then \
		echo "No YAML string tables found under resources/strings." >&2; \
		exit 1; \
	fi
	$(STRINGS_COMPILER) --c-output $(STRINGS_C) --h-output $(STRINGS_H) --manifest $(STRINGS_MANIFEST) $(STRINGS_INPUTS)

strings_generated: $(GENERATED_STRINGS)

# Clean target
clean:
	rm -f $(ALL_TESTS) $(RUN_BUILDABLE)
	rm -f framework/test_framework.o
	rm -f *.root
	rm -f test_*.root
	rm -f $(GENERATED_STRINGS)
	# Paige build artifacts retained under third_party/Paige/build-headless


$(PAIGE_CONFIG):
	@mkdir -p $(PAIGE_BUILD_DIR)
	$(CMAKE_BIN) -S $(PAIGE_DIR) -B $(PAIGE_BUILD_DIR) -DCMAKE_BUILD_TYPE=$(PAIGE_BUILD_TYPE) -DCMAKE_OSX_ARCHITECTURES="$(PAIGE_ARCHES)" -DFRONTIER_TESTS=$(PAIGE_TESTS_FLAG) -DFRONTIER_DISABLE_DOCINFO=ON

$(PAIGE_LIB): $(PAIGE_CONFIG) $(PAIGE_SOURCES)
	$(CMAKE_BIN) --build $(PAIGE_BUILD_DIR) --target paige
# Install target
install: $(ALL_TESTS)
	@echo "Installing test executables..."
	@mkdir -p /usr/local/bin/frontier-tests
	@cp $(ALL_TESTS) /usr/local/bin/frontier-tests/
	@echo "Tests installed to /usr/local/bin/frontier-tests/"

# Uninstall target
uninstall:
	@echo "Uninstalling test executables..."
	@rm -rf /usr/local/bin/frontier-tests
	@echo "Tests uninstalled"

# Help target
help:
	@echo "Frontier Test Suite Makefile"
	@echo "============================"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build all test executables"
	@echo "  test             - Run all tests"
	@echo "  results          - Build+run tests and write tmp/results logs"
	@echo "  test-usertalk    - Run UserTalk object tests"
	@echo "  test-basic       - Run basic type tests"
	@echo "  test-collections - Run collection tests"
	@echo "  test-complex     - Run complex object tests"
	@echo "  test-db-persistence - Run database persistence tests"
	@echo "  test-cli         - Run CLI integration tests"
	@echo "  test-db          - Run database tests"
	@echo "  test-migration   - Run migration tests"
	@echo "  test-compat      - Run compatibility tests"
	@echo "  clean            - Remove all test executables"
	@echo "  install          - Install test executables"
	@echo "  uninstall        - Uninstall test executables"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Test Structure:"
	@echo "  components/usertalk_objects/ - UserTalk object type tests"
	@echo "  components/                   - Integration and system tests"
	@echo "  framework/                    - Test framework"

results:
	@mkdir -p tmp/unit tmp/integration tmp/migration tmp/results
	@ts=$$(date +%Y%m%d_%H%M%S); \
	  bash ./run_all_tests.sh | tee "tmp/results/all_tests.$$ts.log"; \
	  cp "tmp/results/all_tests.$$ts.log" "tmp/results/all_tests.latest.log"

# Verify error message coverage
verify-errors:
	@echo "Verifying error message coverage..."
	@../tools/verify_error_coverage.sh

.PHONY: all test test-integration test-integration-verbose test-all clean install uninstall help results strings_generated verify-errors

check_v6_tables: check_v6_tables.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable check_v6_tables.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c headless_shell.c -o $@ $(LINK_LIBS)
