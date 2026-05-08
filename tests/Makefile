# Frontier Test Suite Makefile
# 2025-10-27 Codex: Link db_format sources into portable component tests (needed by migration suite).
# 2025-11-07 Codex: Add portable include path for core/db_format tests so headless
#                   builds can find classic_handle and friends.
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

FRONTIER_TESTS ?= 1
# OP_HEADLESS_TRACE is referenced in oppack_v7.c under FRONTIER_TESTS but never defined;
# downgrade implicit-declaration from error to warning until that pre-existing latent bug is fixed.
CWARNING_SUPPRESS ?= -Wno-error=implicit-function-declaration
CFLAGS = -Wall -Wextra -std=c99 -g -O0 -fpascal-strings -Wno-deprecated-non-prototype -fcommon \
         -DFRONTIER_USE_PORTABLE_HANDLES -DFRONTIER_HEADLESS \
         -DFRONTIER_PORTABLE_STRINGS \
         -DFRONTIER_PORTABLE_FILE_AVAILABLE -DFRONTIER_PORTABLE_DB_AVAILABLE \
         -DFRONTIER_PORTABLE_APPLEEVENTS \
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

THIRDPARTYDIR = ../third_party
ifeq ($(FRONTIER_TESTS),1)
  CFLAGS += -DFRONTIER_TESTS=1
endif
LIBS =
LDFLAGS = -Wl,-undefined,dynamic_lookup
LINK_LIBS = $(LDFLAGS)

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
    ../frontier-cli/stubs/headless_shell.c \
    components/test_migration_portable.c

# Note: ophoist.c is required because opverbnew() calls oppopallhoists() defined there.
#
# IMPORTANT: LANG_RUNTIME_SOURCES includes kernel_verbs_init.c (auto-generated),
# which initializes all verb processors. Any source file that implements functions
# called during verb initialization (e.g., sysshellcall.c for unixshellcall(),
# tcpverbs.c for tcp_init_context()) MUST be included here — otherwise test
# binaries will segfault on undefined symbols resolved to NULL via dynamic lookup.
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
    ../generated/kernel_verbs_init.c \
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
    ../Common/source/sysshellcall.c \
    ../Common/source/tcpverbs.c \
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
    ../Common/source/ophoist.c \
    ../Common/source/opxml.c \
    ../Common/source/langxml.c \
    $(HEADLESS_VERBS_SOURCES) \
    ../frontier-cli/headless_thread_verbs.c \
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
    ../portable/fileloop_portable.c \
    ../frontier-cli/stubs/headless_lang_runtime_more_stubs.c \
    ../frontier-cli/stubs/headless_shellhooks.c \
    ../frontier-cli/stubs/headless_table_stubs.c \
    ../portable/wptext_runtime.c \
    ../portable/paige_text_extractor.c \
    ../frontier-cli/stubs/headless_pict_stubs.c \
    ../frontier-cli/stubs/headless_menu_stubs.c \
    ../frontier-cli/stubs/headless_menu_globals.c \
    ../Common/source/menuverbs_headless.c \
    ../Common/source/menudata_headless.c \
    ../frontier-cli/stubs/headless_search_stubs.c \
    ../frontier-cli/stubs/headless_mac_compat.c \
    ../frontier-cli/stubs/headless_langregexp_stub.c \
    ../frontier-cli/stubs/headless_langipc_stub.c
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
RUN_BUILDABLE = core_tests db_format_tests runtime_tests parser_tests save_migration_tests test_efp_augmentation file_portable_tests file_readline_tests file_verb_tests handle_tests cli_runtime_tests paige_text_tests oppack_tests hashpack_modern_tests hash_corruption_tests langvalue_64_tests table_operations_integration refcon_tests refcon_phase2_tests op_context_tests table_context_tests time_portability_test test_sys_shell_command_verbs headless_selection_tests opattributes_stub_tests headless_thread_registry_tests test_callback_infrastructure tcp_phase1a_unit_tests menu_v7_refcon_tests menudata_headless_tests menu_list_describe_tests meuserselected_headless_tests test_stringerrorlist pane_compositor_tests mouse_parser_tests terminal_control_tests palette_state_tests palette_render_snapshot_tests palette_arg_inject_tests palette_arg_inject_concurrency_tests repl_palette_source_tests repl_verbs_tests repl_slash_resolver_tests scrollback_pane_tests
# Note: table_verb_tests skipped - pre-existing macOS path detection issue
# Note: test_error_messages skipped - needs full runtime linking (tracked in separate task)
# Note: test_table_sorting skipped - script execution errors cause segfault (Issue #449)

# Prebuilt or auxiliary test binaries that may be present
RUN_PREBUILT = test_64bit_direct test_migration_verification test_real_databases test_runner verify_migration_success

# All test executables (for reference only)
ALL_TESTS = $(TEST_EXECUTABLES) $(USERTALK_OBJECT_TESTS)

# Kernel verbs auto-generation (shared with frontier-cli)
KERNELVERBS_PARSER = ../tools/kernelverbs_parser/parse_kernelverbs.py
KERNELVERBS_RC = ../Common/resources/Win32/kernelverbs.rc
KERNEL_VERBS_C = $(GENERATED_DIR)/kernel_verbs_init.c

$(KERNEL_VERBS_C): $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) headless_verbs.mk
	@mkdir -p $(GENERATED_DIR)
	@echo "Generating kernel_verbs_init.c from kernelverbs.rc..."
	@python3 $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(KERNEL_VERBS_C) --mk-path headless_verbs.mk

kernel_verbs_generated: $(KERNEL_VERBS_C)

# Default target: build the tests we know are supported here
all: strings_generated kernel_verbs_generated $(RUN_BUILDABLE)

# Headless smoke test
core_tests: core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c ../Common/source/logging.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable core_tests.c ../Common/source/shell_api.c ../Common/source/shell_api_headless.c ../Common/source/logging.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

op_context_tests: op_context_tests.c ../Common/source/op_context.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable op_context_tests.c ../Common/source/op_context.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

table_context_tests: table_context/table_context_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../portable table_context/table_context_tests.c -o $@ $(LINK_LIBS)

time_portability_test: time_portability_test.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable time_portability_test.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

test_sys_shell_command_verbs: test_sys_shell_command_verbs.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		test_sys_shell_command_verbs.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c \
		-o $@ $(LINK_LIBS)

headless_selection_tests: headless_selection_tests.c ../Common/source/headless_selection.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		headless_selection_tests.c ../Common/source/headless_selection.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

db_format_tests: db_format_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		db_format_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

oppack_tests: oppack_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		oppack_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

hashpack_modern_tests: hashpack_modern_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		hashpack_modern_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

hash_corruption_tests: hash_corruption_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		hash_corruption_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

langvalue_64_tests: langvalue_64_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		langvalue_64_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

table_verb_tests: table_operations/table_verb_tests.c
	$(CC) -std=c99 -Wall -Wextra -g -O0 table_operations/table_verb_tests.c -o $@

table_operations_integration: table_operations/table_operations_integration.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		table_operations/table_operations_integration.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

refcon_tests: refcon_tests.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		refcon_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

refcon_phase2_tests: refcon_cli_tests/phase2_serialization.c
	$(CC) -std=c99 -Wall -Wextra -g -O0 refcon_cli_tests/phase2_serialization.c -o $@

opattributes_stub_tests: opattributes_stub_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		opattributes_stub_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# terminal_control_tests - PR 3 slash-menu palette UI substrate.
# Standalone (no Frontier runtime needed); links only terminal_control.c.
terminal_control_tests: terminal_control_tests.c ../frontier-cli/terminal_control.c ../frontier-cli/terminal_control.h
	$(CC) -std=c17 -Wall -Wextra -g -O0 -I../frontier-cli \
		terminal_control_tests.c ../frontier-cli/terminal_control.c -o $@

# Thread safety Layer 1 tests - standalone, no full runtime needed
headless_thread_registry_tests: headless_thread_registry_tests.c ../Common/source/threadregistry.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		headless_thread_registry_tests.c ../Common/source/threadregistry.c ../Common/source/logging.c -o $@ -lpthread

# P0a: Parameterized callback infrastructure tests
test_callback_infrastructure: test_callback_infrastructure.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		test_callback_infrastructure.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# TCP Phase 1A unit tests - internal logic validation (no network required)
tcp_phase1a_unit_tests: tcp_phase1a_unit_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		tcp_phase1a_unit_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# Menu v7 refcon table format tests - validates mecreaterefcontable_v7/meunpackrefcontable_v7
menu_v7_refcon_tests: menu_v7_refcon_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/menupack.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		menu_v7_refcon_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/menupack.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# Menudata headless tests - validates menudata_ensure_root() lazy chain creation.
# See planning/architectural_decision_records/ADR-016 for the architectural model.
menudata_headless_tests: menudata_headless_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		menudata_headless_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# Menu list/describe headless tests - validates the leaf-walk and describe-record
# helpers that back the menu.list / menu.describe verbs (ADR-016, sub-PR 2a).
menu_list_describe_tests: menu_list_describe_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		menu_list_describe_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# meuserselected_headless tests - validates the headless dispatch path that
# compiles UserTalk source and schedules a one-shot process. ADR-016 / sub-PR 2b.
meuserselected_headless_tests: meuserselected_headless_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable \
		meuserselected_headless_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# REPL palette source adapter tests (PR 6). Wraps menudata_list_leaves /
# menudata_describe_leaf in the palette_menu_source_t vtable from PR 5.
# Links pane.c + palette.c (CLI side, no runtime calls) plus the full lang
# runtime (which now includes menudata_headless.c via PR 5.5).
repl_palette_source_tests: repl_palette_source_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_palette_source.c ../frontier-cli/palette.c ../frontier-cli/pane.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable -I../frontier-cli \
		repl_palette_source_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_palette_source.c ../frontier-cli/palette.c ../frontier-cli/pane.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# REPL verbs tests (PR 6). Exercises the 5 repl.* kernel verbs through
# the real langbuildtree -> langruncode dispatch path with a fake host
# adapter that records side effects.
repl_verbs_tests: repl_verbs_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_verbs.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable -I../frontier-cli \
		repl_verbs_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_verbs.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

# REPL slash-command resolver tests (PR 7). Exercises the menubar-walk
# resolver that maps a token like "exit" or "exi" or "h" to the leaf
# hashtable inside system.menus.data.repl.REPL.
repl_slash_resolver_tests: repl_slash_resolver_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_slash_resolver.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable -I../frontier-cli \
		repl_slash_resolver_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/repl_slash_resolver.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

runtime_tests: runtime_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable runtime_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

parser_tests: parser_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable parser_tests.c $(LANG_RUNTIME_SOURCES) ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

save_migration_tests: save_migration_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable save_migration_tests.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

refcon_migration_64bit_test: refcon_migration_64bit_test.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable refcon_migration_64bit_test.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

test_efp_augmentation: test_efp_augmentation.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable test_efp_augmentation.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

test_dump_system_paths: test_dump_system_paths.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable test_dump_system_paths.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

file_portable_tests: file_portable_tests.c ../portable/file_portable.c ../frontier-cli/stubs/headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_portable_tests.c ../portable/file_portable.c ../frontier-cli/stubs/headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

file_readline_tests: file_readline_tests.c ../portable/file_portable.c ../frontier-cli/stubs/headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_readline_tests.c ../portable/file_portable.c ../frontier-cli/stubs/headless_shell.c ../Common/source/portable_handles.c ../Common/source/strings.c ../Common/source/logging.c -o $@ $(LINK_LIBS)

file_verb_tests: file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -I../Common/headers -I../Common/SystemHeaders -I../portable file_verb_tests.c $(LANG_RUNTIME_SOURCES) ../Common/source/findinfile.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)

handle_tests: handle_tests.c ../Common/source/portable_handles.c ../portable/classic_handle.c
	$(CC) $(CFLAGS) -DFRONTIER_USE_PORTABLE_HANDLES -I../Common/headers -I../Common/SystemHeaders -I../portable \
		handle_tests.c ../Common/source/portable_handles.c ../portable/classic_handle.c -o $@ $(LINK_LIBS)

test_logging: test_logging.c ../Common/source/logging.c
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders test_logging.c ../Common/source/logging.c -o $@

test_error_messages: test_error_messages.c ../Common/source/logging.c ../Common/source/strings.c ../frontier-cli/stubs/headless_lang_runtime_more_stubs.c ../generated/strings_tables.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable -I../generated \
		test_error_messages.c ../Common/source/logging.c ../Common/source/strings.c ../frontier-cli/stubs/headless_lang_runtime_more_stubs.c ../generated/strings_tables.c -o $@

cli_runtime_tests: cli_runtime_tests.c $(CLI_BIN)
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders cli_runtime_tests.c -o $@ $(LINK_LIBS)

$(CLI_BIN):
	$(MAKE) -C ../frontier-cli frontier-cli

# Pane compositor + SGR 1006 mouse parser tests (pure CLI-side, no runtime)
pane_compositor_tests: pane_compositor_tests.c ../frontier-cli/pane.c ../frontier-cli/pane.h
	$(CC) $(CFLAGS) -I../frontier-cli pane_compositor_tests.c ../frontier-cli/pane.c -o $@ $(LINK_LIBS)

mouse_parser_tests: mouse_parser_tests.c ../frontier-cli/pane.c ../frontier-cli/pane.h
	$(CC) $(CFLAGS) -I../frontier-cli mouse_parser_tests.c ../frontier-cli/pane.c -o $@ $(LINK_LIBS)

# Palette state machine + cascade renderer tests (PR 5).
# Standalone — links pane.c (compositor + mouse parser) and palette.c only.
# No Frontier runtime needed; the palette state machine talks to a synthetic
# in-memory palette_menu_source_t so tests stay fast and isolated.
palette_state_tests: palette_state_tests.c ../frontier-cli/pane.c ../frontier-cli/pane.h ../frontier-cli/palette.c ../frontier-cli/palette.h
	$(CC) $(CFLAGS) -I../frontier-cli palette_state_tests.c ../frontier-cli/pane.c ../frontier-cli/palette.c -o $@ $(LINK_LIBS)

palette_render_snapshot_tests: palette_render_snapshot_tests.c ../frontier-cli/pane.c ../frontier-cli/pane.h ../frontier-cli/palette.c ../frontier-cli/palette.h
	$(CC) $(CFLAGS) -I../frontier-cli palette_render_snapshot_tests.c ../frontier-cli/pane.c ../frontier-cli/palette.c -o $@ $(LINK_LIBS)

# Scrollback pane + palette-aware async output router (issue #593).
# Standalone — links pane.c, scrollback_pane.c, repl_output_async.c.
# No Frontier runtime / linenoise / ODB deps. Tests behavioural shape:
# - ring-buffer storage + bottom-justified projection on the pane
# - z-order under palette panes (compositor blit transparency)
# - palette-active routing of repl_async_output_emit to scrollback
# - thread-safe append from concurrent foreign threads
# - flush-to-stdout drain on palette teardown.
scrollback_pane_tests: scrollback_pane_tests.c ../frontier-cli/pane.c ../frontier-cli/pane.h ../frontier-cli/scrollback_pane.c ../frontier-cli/scrollback_pane.h ../frontier-cli/repl_output_async.c ../frontier-cli/repl_output_async.h
	$(CC) $(CFLAGS) -I../frontier-cli scrollback_pane_tests.c ../frontier-cli/pane.c ../frontier-cli/scrollback_pane.c ../frontier-cli/repl_output_async.c -o $@ $(LINK_LIBS) -lpthread

# Pure helper for arg-injection — no Frontier runtime / handle deps, so it
# compiles with palette_arg_inject.c standalone. The test exercises escape
# rules and the script-source synthesis algorithm.
palette_arg_inject_tests: palette_arg_inject_tests.c ../frontier-cli/palette_arg_inject.c ../frontier-cli/palette_arg_inject.h
	$(CC) $(CFLAGS) -I../frontier-cli palette_arg_inject_tests.c ../frontier-cli/palette_arg_inject.c -o $@ $(LINK_LIBS)

# Cross-thread regression test (issue #597). Exercises the same pure
# helpers under contention to catch a future regression that would
# reintroduce shared state — e.g., a static scratch buffer or a
# process-global pending-arg slot. Links pthread because the test spawns
# real worker threads.
palette_arg_inject_concurrency_tests: palette_arg_inject_concurrency_tests.c ../frontier-cli/palette_arg_inject.c ../frontier-cli/palette_arg_inject.h
	$(CC) $(CFLAGS) -I../frontier-cli palette_arg_inject_concurrency_tests.c ../frontier-cli/palette_arg_inject.c -o $@ $(LINK_LIBS) -lpthread

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

test_stringerrorlist: test_stringerrorlist.c $(TEST_FRAMEWORK) ../frontier-cli/stubs/headless_lang_runtime_more_stubs.c $(STRINGS_C) ../Common/source/logging.c ../portable/classic_handle.c ../Common/source/portable_handles.c
	$(CC) $(CFLAGS) -I../Common/headers -I../Common/SystemHeaders -I../portable -I../generated \
		test_stringerrorlist.c framework/test_framework.c ../frontier-cli/stubs/headless_lang_runtime_more_stubs.c $(STRINGS_C) ../Common/source/logging.c ../portable/classic_handle.c ../Common/source/portable_handles.c -o $@ $(LDFLAGS)

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

test_table_sorting: components/usertalk_objects/test_table_sorting.c $(TEST_FRAMEWORK) $(LANG_RUNTIME_SOURCES) ../portable/cli_executor.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -I../Common/headers -I../Common/SystemHeaders -I../portable components/usertalk_objects/test_table_sorting.c $(TEST_FRAMEWORK) $(LANG_RUNTIME_SOURCES) ../portable/cli_executor.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)


# Test targets
test: strings_generated $(RUN_BUILDABLE)
	@echo "Running all tests..."
	@mkdir -p tmp/unit
	@set -e; \
	for test in $(RUN_BUILDABLE) $(RUN_PREBUILT); do \
	  if [ -x "./$$test" ]; then \
	    echo "Running $$test..."; \
	    ./$$test; \
	  else \
	    echo "Skipping $$test (not present)"; \
	  fi; \
	done
	@echo "Aggregating test results..."
	@python3 ../tools/aggregate_unit_results.py

test-report:
	@echo "Generating unit test OPML report..."
	@python3 ../tools/export_unit_tests_to_opml.py

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

# Python dependency check for integration tests
.PHONY: check-python-deps
check-python-deps:
	@python3 -c "import yaml" 2>/dev/null || (echo "Installing PyYAML..." && pip3 install pyyaml)
	@python3 -c "import pexpect" 2>/dev/null || (echo "Installing pexpect..." && pip3 install pexpect) || true

# Integration test targets (Python-based YAML tests)
test-integration: check-python-deps check-license
	@echo "Running integration tests..."
	@../tools/run_integration_tests.sh

test-integration-verbose: check-python-deps check-license
	@echo "Running integration tests (verbose)..."
	@../tools/run_integration_tests.sh --verbose

# Run CLI migration flag tests (shell-based)
test-migrate-flag:
	@echo "Running CLI migration flag tests..."
	@./test_migrate_flag.sh

# Run custom CLI args tests (shell-based, uses -e mode)
test-custom-args:
	@echo "Running custom CLI args tests..."
	@./custom_cli_args_test.sh

# Run debug protocol tests (Python-based, uses --protocol mode)
test-debug:
	@echo "Running debug protocol tests..."
	@./debug_protocol_test.sh

# Run pre-commit hook tests (fixture-based; no project tooling needed)
test-hooks:
	@../tools/hooks/tests/test_c_indent_hook.sh

# Verify no GPL headers remain in first-party source files (post-relicensing).
# Exits non-zero if any in-scope file still carries a GPLv2 boilerplate header.
check-license:
	@python3 ../tools/relicense_headers.py --check

# Run all tests (unit + integration + CLI migration + custom args + debug + hooks + license check)
test-all: test test-integration test-migrate-flag test-custom-args test-debug test-hooks check-license
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

# Validate BIGSTRING length prefixes across the codebase
validate-bigstrings:
	@echo "Validating BIGSTRING length prefixes..."
	@python3 ../tools/validate_bigstring_lengths.py ..

# Verify error message coverage
verify-errors:
	@echo "Verifying error message coverage..."
	@../tools/verify_error_coverage.sh

.PHONY: all test test-report test-integration test-integration-verbose test-migrate-flag test-custom-args test-debug test-hooks test-all clean install uninstall help results strings_generated kernel_verbs_generated verify-errors validate-bigstrings check-python-deps

check_v6_tables: check_v6_tables.c $(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c
	$(CC) $(CFLAGS) -DFRONTIER_HEADLESS -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB -I../Common/headers -I../Common/SystemHeaders -I../portable check_v6_tables.c \
		$(LANG_RUNTIME_SOURCES) ../Common/source/odbengine.c ../frontier-cli/stubs/headless_shell.c -o $@ $(LINK_LIBS)
