# Frontier CLI Makefile
# Phase 3 headless bring-up: build the CLI without UI frameworks.

CC = clang
CFLAGS = -std=c17 -Wall -Wextra -O0 -g -fpascal-strings \
         -DFRONTIER_HEADLESS -DFRONTIER_USE_PORTABLE_HANDLES \
         -DFRONTIER_PORTABLE_STRINGS \
         -DFRONTIER_PORTABLE_FILE_AVAILABLE -DFRONTIER_PORTABLE_DB_AVAILABLE \
         -DFRONTIER_PORTABLE_APPLEEVENTS \
         -DHEADLESS_USE_REAL_OPLANGTEXT \
         -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB \
         -DHEADLESS_USE_REAL_TABLEPACK -DHEADLESS_USE_REAL_TABLEVALTOTABLE \
         -DHEADLESS_LINKS_REAL_MENUVERBS \
         -DUNIX_COMPILE -DC_LIBRARY -DNO_OS_INLINE

# Architecture support (for universal binary builds)
# Set ARCHES="arm64 x86_64" to build universal binary
ARCHES ?= $(shell uname -m)
ARCH_FLAGS = $(foreach arch,$(ARCHES),-arch $(arch))

# Version embedding from git
# CLI version: from git tags (e.g., "v1.0.0-alpha.7")
# Product version: CLI major + 10, stage mapped (alpha→a, beta→b, rc→fc)
#   Derived by tools/derive_product_version.sh (e.g., "v1.0.0-alpha.7" → "11.0a7")
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo "1.0.0-dev")
PRODUCT_VERSION ?= $(shell ../tools/derive_product_version.sh "$(VERSION)")
CFLAGS += -DFRONTIER_CLI_VERSION_STRING=\"$(VERSION)\"
CFLAGS += -DFRONTIER_PRODUCT_VERSION_STRING=\"$(PRODUCT_VERSION)\"

SANITIZE ?= 0
ifeq ($(SANITIZE),1)
  CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
endif

# Directories
SRCDIR = .
HEADERDIR = ../Common/headers
SYSTEMHEADERDIR = ../Common/SystemHeaders
PORTABLEDIR = ../portable
TESTSDIR = ../tests
STUBSDIR = ./stubs

# Include shared headless verb file list
include $(TESTSDIR)/headless_verbs.mk

INCLUDES = -I$(HEADERDIR) -I$(SYSTEMHEADERDIR) -I$(PORTABLEDIR) -I../generated

THIRDPARTYDIR = ../third_party

INCLUDES += -I$(THIRDPARTYDIR)/linenoise -I$(THIRDPARTYDIR)/cJSON
LIBS =
LDFLAGS = -Wl,-undefined,dynamic_lookup

# Linenoise (cross-platform readline alternative)
LINENOISE_SRC = $(THIRDPARTYDIR)/linenoise/linenoise.c

# Boxen termbox2 vendor (in-tree under frontier-cli/third_party/termbox2/).
# termbox2 v2.5.0 is a single-header library: termbox2.h contains both the
# API declarations and the implementation. termbox2_impl.c is the one TU that
# activates the implementation (it #defines TB_IMPL before the include).
# All other source files include termbox2.h without TB_IMPL (declarations only).
TB2_DIR = third_party/termbox2
INCLUDES += -I$(TB2_DIR)
TB2_IMPL_SRC = $(TB2_DIR)/termbox2_impl.c

# Boxen substrate (frontier-cli/boxen/).
# All four .c files compile into frontier-cli at A.1 so regressions are caught
# during normal builds. backend_mock.c is intentionally in the prod build;
# unused statics get __attribute__((unused)) if the compiler complains.
BOXEN_DIR = boxen
INCLUDES += -I$(BOXEN_DIR)
BOXEN_SOURCES = \
	$(BOXEN_DIR)/boxen_log.c \
	$(BOXEN_DIR)/boxen_wcwidth.c \
	$(BOXEN_DIR)/backend_tb2.c \
	$(BOXEN_DIR)/backend_mock.c \
	$(BOXEN_DIR)/boxen.c

# Strings compiler integration (shared with tests)
STRINGS_COMPILER_DIR = ../tools/strings_compiler
STRINGS_COMPILER = $(STRINGS_COMPILER_DIR)/strings_compiler
STRINGS_INPUTS = $(wildcard ../resources/strings/*.yaml)
GENERATED_DIR = ../generated
STRINGS_C = $(GENERATED_DIR)/strings_tables.c
STRINGS_H = $(GENERATED_DIR)/strings_tables.h
STRINGS_MANIFEST = $(GENERATED_DIR)/strings_manifest.json
GENERATED_STRINGS = $(STRINGS_C) $(STRINGS_H) $(STRINGS_MANIFEST)

# CLI sources
CLI_SOURCES = \
    main.c \
    cli_parser.c \
    cli_utils.c \
    cli_executor.c \
    cli_json_output.c \
    protocol_handler.c \
    op_handler.c \
    debug_handler.c \
    headless_spawn.c \
    odb_ops.c \
    ws_server.c \
    ws_frame.c \
    base64_util.c \
    headless_scripts_loader.c \
    terminal_control.c \
    dialog_prompts.c \
    tab_completion.c \
    file_dialog.c \
    file_browser.c \
    completion.c \
    repl.c \
    repl_eval.c \
    repl_output.c \
    repl_output_async.c \
    repl_variables.c \
    repl_verbs.c \
    repl_palette_source.c \
    repl_slash_resolver.c \
    pane.c \
    palette.c \
    palette_arg_inject.c \
    scrollback_pane.c \
    window_registry.c \
    ut_sync.c \
    ut_scan.c \
    debugger_tui.c \
    boxen_repl.c \
    boxen_outline.c

# cJSON (vendored JSON parser)
CJSON_SOURCES = $(THIRDPARTYDIR)/cJSON/cJSON.c

# Core runtime sources needed for script execution (aligned with tests/runtime_tests)
RUNTIME_SOURCES = \
    ../Common/source/logging.c \
    ../Common/source/lang.c \
    ../Common/source/langcallbacks.c \
    ../Common/source/langcrypt.c \
    ../Common/source/base64.c \
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
    ../Common/source/langexternal.c \
    ../Common/source/langstartup.c \
    ../Common/source/langwarnings.c \
    ../Common/source/langtmpstack.c \
    ../Common/source/langtrace.c \
    ../Common/source/langtree.c \
    ../Common/source/langdate.c \
    ../Common/source/langsystypes.c \
    ../Common/source/langverbs.c \
    ../Common/source/memory.c \
    ../Common/source/portable_handles.c \
    ../Common/source/strings.c \
    ../Common/source/strings_extras.c \
    ../portable/text_encoding_portable.c \
    ../portable/time_portable.c \
    ../Common/source/timedate.c \
    ../Common/source/md5.c \
    ../Common/source/sha1dgst.c \
    ../Common/source/whirlpool.c \
    ../Common/source/ops.c \
    ../Common/source/op.c \
    ../Common/source/op_context.c \
    ../Common/source/opinit.c \
    ../Common/source/opexpand.c \
    ../Common/source/ophoist.c \
    ../Common/source/opvisit.c \
    ../Common/source/opedit.c \
    ../Common/source/opops.c \
    ../Common/source/oplist.c \
    ../Common/source/opstructure.c \
    ../Common/source/oprefcon.c \
    ../Common/source/oppack_v7.c \
    ../Common/source/legacy/oppack_legacy.c \
    ../Common/source/oplangtext.c \
    ../Common/source/opverbs.c \
    ../Common/source/opxml.c \
    ../Common/source/langxml.c \
    ../Common/source/langhtml.c \
    ../Common/source/tableops.c \
    ../Common/source/tablestructure.c \
    ../Common/source/tableexternal_common.c \
    ../Common/source/tableexternal.c \
    ../Common/source/tablepack.c \
    ../Common/source/tableverbs.c \
    ../Common/source/stringverbs.c \
    ../Common/source/dbverbs.c \
    ../Common/source/tcpverbs.c \
    ../Common/source/headless_selection.c \
    ../Common/source/shell_api.c \
    ../Common/source/shell_api_headless.c \
    ../Common/source/sysshellcall.c \
    ../Common/source/menupack.c \
    ../Common/source/menuverbs.c \
    ../Common/source/menuverbs_headless.c \
    ../Common/source/menudata_headless.c \
    ../Common/source/threadregistry.c

# Headless runtime stubs (owned by frontier-cli)
HEADLESS_STUBS = \
    $(STUBSDIR)/headless_shell.c \
    $(STUBSDIR)/headless_shellhooks.c \
    $(STUBSDIR)/headless_table_stubs.c \
    ../portable/wptext_runtime.c \
    ../portable/paige_text_extractor.c \
    $(STUBSDIR)/headless_pict_stubs.c \
    $(STUBSDIR)/headless_menu_stubs.c \
    $(STUBSDIR)/headless_menu_globals.c \
    $(STUBSDIR)/headless_search_stubs.c \
    $(STUBSDIR)/headless_mac_compat.c \
    $(STUBSDIR)/headless_langregexp_stub.c \
    $(STUBSDIR)/headless_langipc_stub.c \
    $(STUBSDIR)/headless_lang_runtime_more_stubs.c \
    ../portable/file_portable.c \
    ../portable/file_working_dir.c \
    ../portable/fileverbs_portable.c \
    ../portable/fileloop_portable.c \
    ../Common/source/file_portable_posix.c \
    ../Common/source/headless_stubs.c \
    ../portable/appleevent_portable.c \
    ../portable/classic_handle.c \
    ../portable/script_portable.c \
    ../portable/wptext_portable.c \
    $(addprefix $(TESTSDIR)/,$(HEADLESS_VERBS_SOURCES)) \
    ./headless_thread_verbs.c

# Database and file runtime sources required for Frontier.root access
DATABASE_SOURCES = \
    ../Common/source/db.c \
    ../Common/source/db_format.c \
    ../Common/source/db_reader_legacy.c \
    ../Common/source/db_reader_v7.c \
    ../Common/source/db_writer_v7.c \
    ../Common/source/odbengine.c \
    ../Common/source/findinfile.c

# Generated kernel verb initialization
KERNEL_VERBS_C = $(GENERATED_DIR)/kernel_verbs_init.c

# Aggregate sources
FRONTIER_SOURCES = $(RUNTIME_SOURCES) $(DATABASE_SOURCES) $(HEADLESS_STUBS) $(STRINGS_C) $(KERNEL_VERBS_C)

TARGET = frontier-cli

# Kernel verbs compiler and analyzer
KERNELVERBS_PARSER = ../tools/kernelverbs_parser/parse_kernelverbs.py
KERNELVERBS_RC = ../Common/resources/Win32/kernelverbs.rc
KERNELVERBS_CLI = ../tools/kernelverbs_parser/cli.py

# Full distribution build (required for testing mainResponder/Manila)
# This is the default target (first in Makefile) - always keeps dist/ up to date
all: build dist

# Fast build target - just the binary (for quick iteration)
# Use 'make build' when you don't need dist/ updated
build: strings_generated kernel_verbs_generated $(TARGET)

$(TARGET): $(CLI_SOURCES) $(FRONTIER_SOURCES) $(CJSON_SOURCES) $(LINENOISE_SRC) $(TB2_IMPL_SRC) $(BOXEN_SOURCES)
	$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) \
		$(CLI_SOURCES) \
		$(CJSON_SOURCES) \
		$(FRONTIER_SOURCES) \
		$(LINENOISE_SRC) \
		$(TB2_IMPL_SRC) \
		$(BOXEN_SOURCES) \
		-o $(TARGET) $(LIBS) $(LDFLAGS) -lm

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

# Kernel verbs auto-generation from kernelverbs.rc
# Registered processors derived from headless_verbs.mk (single source of truth)
$(KERNEL_VERBS_C): $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(TESTSDIR)/headless_verbs.mk
	@mkdir -p $(GENERATED_DIR)
	@echo "Generating kernel_verbs_init.c from kernelverbs.rc..."
	@python3 $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(KERNEL_VERBS_C) --mk-path $(TESTSDIR)/headless_verbs.mk

kernel_verbs_generated: $(KERNEL_VERBS_C)

# Verb binding verification and reporting targets
.PHONY: verify-verb-bindings regenerate-verb-bindings verb-status test-verb-bindings

verify-verb-bindings:
	@echo "Verifying verb binding analyzer against current state..."
	@python3 $(KERNELVERBS_CLI) verify
	@echo "✓ Verb binding verification passed"

regenerate-verb-bindings:
	@echo "Regenerating kernel_verbs_init.c with automatic verb detection..."
	@rm -f $(KERNEL_VERBS_C)
	@$(MAKE) kernel_verbs_generated
	@echo "✓ Verb bindings regenerated"

verb-status:
	@echo "Generating verb binding coverage report..."
	@python3 $(KERNELVERBS_CLI) report
	@echo "✓ Coverage report generated in reports/coverage/verb-binding/"

test-verb-bindings:
	@echo "Running verb binding unit tests..."
	@cd $$(dirname $(KERNELVERBS_CLI)) && python3 -m unittest test_analyzer -v
	@echo "✓ All verb binding tests passed"

# Validate BIGSTRING length prefixes across the codebase
validate-bigstrings:
	@echo "Validating BIGSTRING length prefixes..."
	@python3 ../tools/validate_bigstring_lengths.py ..

clean:
	rm -f $(TARGET)

# Distribution target - creates legacy Frontier 9.5 directory structure
# Source databases are v7 format — copy directly (no migration needed)
DIST_DIR = ../dist
DATABASES_DIR = ../databases

dist: $(TARGET)
	@echo "Creating distribution in $(DIST_DIR)..."
	@mkdir -p $(DIST_DIR)
	@rm -rf $(DIST_DIR)/*
	@# Copy executable
	cp $(TARGET) $(DIST_DIR)/
	@# Copy static files
	cp $(DATABASES_DIR)/frontierStartupCommands.txt $(DIST_DIR)/
	cp $(DATABASES_DIR)/License.rtf $(DIST_DIR)/
	cp "$(DATABASES_DIR)/Manila User's Guide.pdf" "$(DIST_DIR)/"
	@# Copy directories (excluding .DS_Store)
	rsync -a --exclude='.DS_Store' $(DATABASES_DIR)/Appearance $(DIST_DIR)/
	rsync -a --exclude='.DS_Store' $(DATABASES_DIR)/Extras $(DIST_DIR)/
	rsync -a --exclude='.DS_Store' $(DATABASES_DIR)/DLLs $(DIST_DIR)/
	@# Create Guest Databases structure
	@mkdir -p "$(DIST_DIR)/Guest Databases/apps/Tools"
	@mkdir -p "$(DIST_DIR)/Guest Databases/ops/themes"
	@mkdir -p "$(DIST_DIR)/Guest Databases/ops/discussion group themes"
	@mkdir -p "$(DIST_DIR)/Guest Databases/www"
	@# Copy v7 databases (Virgin.root becomes Frontier.root in dist)
	@echo "Copying v7 databases to dist..."
	cp $(DATABASES_DIR)/Virgin.root $(DIST_DIR)/Frontier.root
	cp "$(DATABASES_DIR)/Guest Databases/apps/mainResponder.root" "$(DIST_DIR)/Guest Databases/apps/mainResponder.root"
	cp "$(DATABASES_DIR)/Guest Databases/apps/manila.root" "$(DIST_DIR)/Guest Databases/apps/manila.root"
	cp "$(DATABASES_DIR)/Guest Databases/apps/Tools/radioCommunityServer.root" "$(DIST_DIR)/Guest Databases/apps/Tools/radioCommunityServer.root"
	cp "$(DATABASES_DIR)/Guest Databases/apps/Tools/serverMonitor.root" "$(DIST_DIR)/Guest Databases/apps/Tools/serverMonitor.root"
	cp "$(DATABASES_DIR)/Guest Databases/apps/Tools/TheXmlFiles.root" "$(DIST_DIR)/Guest Databases/apps/Tools/TheXmlFiles.root"
	cp "$(DATABASES_DIR)/Guest Databases/www/prefs.root" "$(DIST_DIR)/Guest Databases/www/prefs.root"
	@echo ""
	@echo "Distribution created in $(DIST_DIR)/"
	@echo "Run with: ./dist/frontier-cli --system-root dist/Frontier.root"

# Run userland.cleanRoot against the dist copy of Frontier.root.
# This strips runtime state (user.databases, scheduler tasks, serial numbers, etc.)
# while preserving all scripts and verbs. The source databases/Virgin.root is NOT modified.
# Note: cleanRoot also produces dist/Virgin.root as a side artifact (via FileMenu.saveCopy).
# Use before shipping: make dist && make clean-root
clean-root: dist
	@echo "Running userland.cleanRoot on dist/Frontier.root..."
	@# Protocol mode is now default-RW (issue #127); no opt-in flag needed
	@# for cleanRoot to persist its mutations to the dist file.
	@echo '{"op":"script/eval","id":1,"params":{"expression":"userland.cleanRoot()"}}' \
		| $(DIST_DIR)/frontier-cli --protocol --system-root $(DIST_DIR)/Frontier.root 2>&1 \
		| tee $(DIST_DIR)/cleanroot.log > /dev/null; \
	if grep -q '"success":true' $(DIST_DIR)/cleanroot.log; then \
		echo "cleanRoot completed successfully."; \
		rm -f $(DIST_DIR)/cleanroot.log; \
	else \
		echo "ERROR: cleanRoot failed. Output:"; \
		cat $(DIST_DIR)/cleanroot.log; \
		exit 1; \
	fi

dist-clean:
	rm -rf $(DIST_DIR)

# Static analysis via clang-tidy (requires compile_commands.json in project root)
# Generate it with: bear -- make -C frontier-cli
# Usage: make lint           (frontier-cli sources only)
#        make lint-all       (all project sources, slower)
lint:
	@if [ ! -f ../compile_commands.json ]; then \
		echo "Error: compile_commands.json not found. Run: bear -- make -C frontier-cli"; \
		exit 1; \
	fi
	-clang-tidy $(CLI_SOURCES) -p ..

lint-all:
	@if [ ! -f ../compile_commands.json ]; then \
		echo "Error: compile_commands.json not found. Run: bear -- make -C frontier-cli"; \
		exit 1; \
	fi
	-clang-tidy $(CLI_SOURCES) $(FRONTIER_SOURCES) -p ..

lint-python:
	@command -v ruff >/dev/null 2>&1 || { echo "ruff not installed. Run: pip install ruff"; exit 1; }
	-cd .. && ruff check .

lint-shell:
	@command -v shellcheck >/dev/null 2>&1 || { echo "shellcheck not installed. Run: brew install shellcheck"; exit 1; }
	-find .. -name '*.sh' -not -path '../tests/tmp/*' -not -path '../third_party/*' -not -path '../tests/vendor/*' | xargs shellcheck

# termbox2 portability smoke -- compiles termbox2_impl.c + minimal main.
# Reserved for cross-platform CI (Linux/Windows) when test environments are
# available. Not currently run by the macOS-only CI workflow; exists to ensure
# tb2-smoke remains a viable build target so adding Linux/Windows CI later is
# a CI-config change, not a code change.
#
# Two-step compile is required because termbox2_impl.c must be compiled with
# -DTB_IMPL (to activate the single-header implementation) while smoke_main.c
# must NOT get -DTB_IMPL (would cause duplicate symbol errors at link time).
#
# The recipe hardcodes a minimal CFLAGS set instead of inheriting $(CFLAGS) on
# purpose: the project CFLAGS include -fpascal-strings (Apple-clang-only) and
# Mach-O linker flags. The smoke target must compile cleanly on Linux/Windows
# when those CI jobs land, so it sticks to portable flags only.
#
# Object files land in obj/ (local, gitignored) rather than /tmp to avoid the
# predictable-path race risk on shared dev machines.
TB2_SMOKE_SRC = $(TB2_DIR)/smoke_main.c
TB2_SMOKE_BIN = tb2-smoke
TB2_SMOKE_OBJDIR = obj/tb2-smoke

tb2-smoke: $(TB2_IMPL_SRC) $(TB2_SMOKE_SRC)
	@mkdir -p $(TB2_SMOKE_OBJDIR)
	$(CC) -std=c17 -Wall -Wextra -O0 -g -I$(TB2_DIR) \
		-c $(TB2_IMPL_SRC) -o $(TB2_SMOKE_OBJDIR)/tb2_impl.o
	$(CC) -std=c17 -Wall -Wextra -O0 -g -I$(TB2_DIR) \
		-c $(TB2_SMOKE_SRC) -o $(TB2_SMOKE_OBJDIR)/tb2_smoke_main.o
	$(CC) $(TB2_SMOKE_OBJDIR)/tb2_impl.o $(TB2_SMOKE_OBJDIR)/tb2_smoke_main.o -o $(TB2_SMOKE_BIN)

# Phase A.6 split layout demo -- links against the tb2 backend and boxen core.
# Sources: examples/split_demo.c + termbox2_impl.c + all boxen .c files.
SPLIT_DEMO_SRC = $(BOXEN_DIR)/examples/split_demo.c
SPLIT_DEMO_BIN = split_demo

split_demo: $(SPLIT_DEMO_SRC) $(TB2_IMPL_SRC) $(BOXEN_SOURCES)
	$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) \
		$(SPLIT_DEMO_SRC) \
		$(TB2_IMPL_SRC) \
		$(BOXEN_SOURCES) \
		-o $(SPLIT_DEMO_BIN) $(LIBS) $(LDFLAGS) -lm

.PHONY: all build clean strings_generated kernel_verbs_generated dist dist-clean clean-root validate-bigstrings lint lint-all lint-python lint-shell tb2-smoke split_demo
