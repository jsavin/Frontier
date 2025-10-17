# Frontier Test Suite Makefile
# Phase 1.1: Comprehensive Testing Implementation

CC = clang
# Opt-in sanitizers: set SANITIZE=1 to enable ASan/UBSan for easier debugging
SANITIZE ?= 0
SAN_FLAGS =
ifeq ($(SANITIZE),1)
  SAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
  # macOS leak detection can be noisy; disable unless needed
  # export ASAN_OPTIONS=detect_leaks=0  # set in shell if desired
endif
CFLAGS = -Wall -Wextra -std=c99 -g -O0 -fpascal-strings -Wno-deprecated-non-prototype \
         -DFRONTIER_USE_PORTABLE_HANDLES \
         -DLAND_GENERALLOG_LEVEL=LAND_LOGLEVEL_OFF -DLAND_GENERALLOG_TARGET=LAND_LOGTARGET_NONE \
         $(SAN_FLAGS)
FRONTIER_CLI_DIR = ../frontier-cli
SYSTEMHEADERDIR = ../Common/SystemHeaders
HEADERDIR = ../Common/headers
MODERNHEADERDIR = ../build_Xcode_modern
FRAMEWORKS = -framework CoreFoundation -framework Carbon

# Frontier core source files (minimal set suitable for CLI/testing)
# Portable core sources (subset)
PORTABLE_SOURCES = \
    ../portable/handle.c \
    ../portable/classic_handle.c \
    ../portable/platform_null.c \
    ../portable/cli_executor.c \
    ../portable/runtime_stubs_core.c \
    ../portable/runtime_stubs_errors.c \
    ../portable/runtime_stubs_hash.c \
    ../portable/runtime_stubs_lang.c \
    ../portable/runtime_stubs_list.c \
    ../portable/runtime_stubs_system.c \
    ../portable/runtime_stubs_stack.c \
    ../portable/runtime_stubs_string.c \
    ../portable/runtime_stubs_coerce.c \
    ../portable/runtime_stubs_parser.c \
    ../portable/runtime_stubs_outline.c \
    ../portable/runtime_stubs_shell.c

# TODO: progressively add language engine sources adapted for FRONTIER_PORTABLE
FRONTIER_SOURCES = $(PORTABLE_SOURCES) \
    ../Common/source/langscan.c \
    ../Common/source/langparser.c \
    ../Common/source/langvalue.c \
    ../Common/source/langevaluate.c \
    ../Common/source/lang.c

LANG_RUNTIME_SOURCES = \
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
    ../Common/source/langstartup.c \
    ../Common/source/langwarnings.c \
    ../Common/source/langtmpstack.c \
    ../Common/source/langtrace.c \
    ../Common/source/langtree.c \
    ../Common/source/langdate.c \
    ../Common/source/langsystypes.c \
    ../Common/source/memory.c \
    ../Common/source/portable_handles.c \
    ../Common/source/strings.c \
    ../Common/source/FastTimes.c \
    ../Common/source/md5.c \
    ../Common/source/sha1dgst.c \
    ../Common/source/whirlpool.c \
    ../Common/source/strings_extras.c \
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
    ../Common/source/oppack.c \
    ../Common/source/opverbs.c \
    ../Common/source/opxml.c \
    ../Common/source/langxml.c \
    ../Common/source/tableops.c \
    ../Common/source/tablestructure.c \
    headless_lang_runtime_more_stubs.c \
    headless_shellhooks.c \
    headless_table_stubs.c \
    headless_wp_stubs.c \
    headless_pict_stubs.c \
    headless_menu_stubs.c \
    headless_search_stubs.c \
    headless_mac_compat.c \
    headless_langregexp_stub.c \
    \
    headless_langipc_stub.c

# CLI source files
CLI_SOURCES =

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
	db_format_tests

# UserTalk object test executables
USERTALK_OBJECT_TESTS = \
	test_basic_types \
	test_collections \
	test_complex_objects \
	test_database_persistence \
	test_usertalk_runner

# Buildable test executables on this branch
RUN_BUILDABLE = core_tests db_format_tests runtime_tests parser_tests save_migration_tests file_portable_tests file_readline_tests file_verb_tests handle_tests cli_runtime_tests

# Prebuilt or auxiliary test binaries that may be present
RUN_PREBUILT = test_64bit_direct test_migration_verification test_real_databases test_runner verify_migration_success

# All test executables (for reference only)
ALL_TESTS = $(TEST_EXECUTABLES) $(USERTALK_OBJECT_TESTS)

# Default target: build the tests we know are supported here
all: $(RUN_BUILDABLE)

# Headless smoke test
core_tests: core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c headless_shell.c -o $@

db_format_tests: db_format_tests.c ../Common/source/db_format.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders db_format_tests.c ../Common/source/db_format.c -o $@

runtime_tests: runtime_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable runtime_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@

parser_tests: parser_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable parser_tests.c $(LANG_RUNTIME_SOURCES) headless_shell.c -o $@

save_migration_tests: save_migration_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/db.c ../Common/source/db_format.c headless_file_portable.c ../Common/source/odbengine.c headless_odb_stubs.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable save_migration_tests.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/db.c ../Common/source/db_format.c headless_file_portable.c ../Common/source/odbengine.c headless_odb_stubs.c headless_shell.c -o $@

file_portable_tests: file_portable_tests.c headless_file_portable.c headless_shell.c ../Common/source/portable_handles.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_portable_tests.c headless_file_portable.c headless_shell.c ../Common/source/portable_handles.c -o $@

file_readline_tests: file_readline_tests.c headless_file_portable.c headless_shell.c ../Common/source/portable_handles.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_readline_tests.c headless_file_portable.c headless_shell.c ../Common/source/portable_handles.c -o $@

file_verb_tests: file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c headless_file_portable.c headless_file_verbs.c headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c headless_file_portable.c headless_file_verbs.c headless_shell.c -o $@

handle_tests: handle_tests.c ../Common/source/portable_handles.c
	$(CC) $(CFLAGS) -DFRONTIER_USE_PORTABLE_HANDLES -I../Common/headers -I../Common/SystemHeaders handle_tests.c ../Common/source/portable_handles.c -o $@

cli_runtime_tests: cli_runtime_tests.c
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders cli_runtime_tests.c -o $@

# Test framework object
framework/test_framework.o: framework/test_framework.c framework/test_framework.h
	$(CC) $(CFLAGS) -c $< -o $@

# Main test executables
test_usertalk_objects: components/test_usertalk_objects.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@

test_cli_integration: components/test_cli_integration.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@

test_database: components/test_database.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@

test_migration: components/test_migration.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@

test_compatibility: components/test_compatibility.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@

test_64bit_structures: components/test_64bit_structures.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@

test_dbnew: components/test_dbnew.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) -I$(SYSTEMHEADERDIR) -I../Common/SystemHeaders -I$(HEADERDIR) -I$(MODERNHEADERDIR) -I../portable $^ $(FRAMEWORKS) -o $@


# UserTalk object test executables
test_usertalk_runner: components/usertalk_objects/test_usertalk_objects.c components/usertalk_objects/test_runner.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable $^ -o $@

test_basic_types: components/usertalk_objects/test_basic_types.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@

test_collections: components/usertalk_objects/test_collections.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@

test_complex_objects: components/usertalk_objects/test_complex_objects.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@

test_database_persistence: components/usertalk_objects/test_database_persistence.c $(TEST_FRAMEWORK) $(FRONTIER_SOURCES) $(CLI_SOURCES)
	$(CC) $(CFLAGS) -DFRONTIER_PORTABLE -I../portable -I../Common/SystemHeaders -I$(HEADERDIR) -I../portable -I$(FRONTIER_CLI_DIR) $^ -o $@


# Test targets
test: $(RUN_BUILDABLE)
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

# Clean target
clean:
	rm -f $(ALL_TESTS)
	rm -f framework/test_framework.o
	rm -f *.root
	rm -f test_*.root

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
	@echo "  results          - Build+run tests and write _results logs"
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
	@mkdir -p _results
	@ts=$$(date +%Y%m%d_%H%M%S); \
	  bash ./run_all_tests.sh | tee "_results/all_tests.$$ts.log"; \
	  cp "_results/all_tests.$$ts.log" "_results/all_tests.latest.log"

.PHONY: all test clean install uninstall help results
