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

# Include shared headless verb file list
include $(TESTSDIR)/headless_verbs.mk

INCLUDES = -I$(HEADERDIR) -I$(SYSTEMHEADERDIR) -I$(PORTABLEDIR) -I../generated

# Paige integration (WPText engine)
THIRDPARTYDIR = ../third_party
PAIGE_DIR = $(THIRDPARTYDIR)/Paige
PAIGE_BUILD_DIR = $(PAIGE_DIR)/build-headless
PAIGE_LIB = $(PAIGE_BUILD_DIR)/libpaige.a
# Use local cmake-install if it exists and works, otherwise fall back to system cmake
# Check that cmake can actually run (not just executable bit set) by verifying no errors in version output
CMAKE_BIN = $(shell if [ -x $(THIRDPARTYDIR)/cmake-install/bin/cmake ] && ! $(THIRDPARTYDIR)/cmake-install/bin/cmake --version 2>&1 | grep -q "Error"; then echo "$(THIRDPARTYDIR)/cmake-install/bin/cmake"; else echo "cmake"; fi)
PAIGE_ARCHES = arm64;x86_64

INCLUDES += -I$(PAIGE_DIR)/PGHEADER -I$(THIRDPARTYDIR)/linenoise -I$(THIRDPARTYDIR)/cJSON
LIBS = $(PAIGE_LIB)
LDFLAGS = -Wl,-undefined,dynamic_lookup

# Linenoise (cross-platform readline alternative)
LINENOISE_SRC = $(THIRDPARTYDIR)/linenoise/linenoise.c

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
    odb_ops.c \
    ws_server.c \
    ws_frame.c \
    headless_scripts_loader.c \
    terminal_control.c \
    dialog_prompts.c \
    tab_completion.c \
    file_dialog.c \
    file_browser.c \
    completion.c \
    repl.c \
    repl_commands.c \
    repl_eval.c \
    repl_output.c \
    repl_variables.c

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
    ../Common/source/threadregistry.c

# Headless adapters mirrored from tests/
HEADLESS_STUBS = \
    $(TESTSDIR)/headless_shell.c \
    $(TESTSDIR)/headless_shellhooks.c \
    $(TESTSDIR)/headless_table_stubs.c \
    ../portable/wptext_runtime.c \
    ../portable/paige_text_extractor.c \
    $(TESTSDIR)/headless_pict_stubs.c \
    $(TESTSDIR)/headless_menu_stubs.c \
    $(TESTSDIR)/headless_menu_globals.c \
    $(TESTSDIR)/headless_search_stubs.c \
    $(TESTSDIR)/headless_mac_compat.c \
    $(TESTSDIR)/headless_langregexp_stub.c \
    $(TESTSDIR)/headless_langipc_stub.c \
    $(TESTSDIR)/headless_lang_runtime_more_stubs.c \
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
    $(addprefix $(TESTSDIR)/,$(HEADLESS_VERBS_SOURCES))

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

$(TARGET): $(CLI_SOURCES) $(FRONTIER_SOURCES) $(CJSON_SOURCES) $(PAIGE_LIB) $(LINENOISE_SRC)
	$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) \
		$(CLI_SOURCES) \
		$(CJSON_SOURCES) \
		$(FRONTIER_SOURCES) \
		$(LINENOISE_SRC) \
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

# Paige library build with automatic rebuild detection
# Rebuilds if:
#   1. libpaige.a doesn't exist, OR
#   2. CMakeCache.txt doesn't exist, OR
#   3. CMakeCache.txt contains paths from a different worktree
#
# This ensures "make" just works in new worktrees and fresh clones.
PAIGE_CACHE = $(PAIGE_BUILD_DIR)/CMakeCache.txt
CURRENT_PAIGE_DIR := $(abspath $(PAIGE_BUILD_DIR))

$(PAIGE_LIB): check-paige-cache
	@if [ ! -f $(PAIGE_LIB) ]; then \
		echo "Building Paige library (this only happens once per worktree)..."; \
		$(CMAKE_BIN) -S $(PAIGE_DIR) -B $(PAIGE_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$(PAIGE_ARCHES)"; \
		$(CMAKE_BIN) --build $(PAIGE_BUILD_DIR); \
	fi

# Check if CMake cache points to wrong directory (stale from another worktree)
.PHONY: check-paige-cache
check-paige-cache:
	@if [ -f $(PAIGE_CACHE) ]; then \
		cached_path=$$(grep "^paige_BINARY_DIR:" $(PAIGE_CACHE) 2>/dev/null | cut -d= -f2); \
		if [ -n "$$cached_path" ] && [ "$$cached_path" != "$(CURRENT_PAIGE_DIR)" ]; then \
			echo "Detected stale CMake cache from different worktree, cleaning..."; \
			rm -rf $(PAIGE_BUILD_DIR); \
		fi; \
	fi

# Distribution target - creates legacy Frontier 9.5 directory structure with v7 databases
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
	@# Migrate databases to v7 format (fail fast on any error)
	@echo "Migrating databases to v7 format..."
	./$(TARGET) --migrate $(DATABASES_DIR)/Virgin.root --output $(DIST_DIR)/Frontier.root7 || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/apps/mainResponder.root" --output "$(DIST_DIR)/Guest Databases/apps/mainResponder.root7" || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/apps/manila.root" --output "$(DIST_DIR)/Guest Databases/apps/manila.root7" || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/apps/Tools/radioCommunityServer.root" --output "$(DIST_DIR)/Guest Databases/apps/Tools/radioCommunityServer.root7" || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/apps/Tools/serverMonitor.root" --output "$(DIST_DIR)/Guest Databases/apps/Tools/serverMonitor.root7" || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/apps/Tools/TheXmlFiles.root" --output "$(DIST_DIR)/Guest Databases/apps/Tools/TheXmlFiles.root7" || exit 1
	./$(TARGET) --migrate "$(DATABASES_DIR)/Guest Databases/www/prefs.root" --output "$(DIST_DIR)/Guest Databases/www/prefs.root7" || exit 1
	@# Rename .root7 to .root so UserTalk bootstrapping code can find them
	@echo "Renaming v7 databases to .root extension..."
	@find $(DIST_DIR) -name '*.root7' -exec sh -c 'mv "$$1" "$${1%.root7}.root"' _ {} \;
	@echo ""
	@echo "Distribution created in $(DIST_DIR)/"
	@echo "Run with: ./dist/frontier-cli --system-root dist/Frontier.root"

dist-clean:
	rm -rf $(DIST_DIR)

.PHONY: all build clean strings_generated kernel_verbs_generated check-paige-cache dist dist-clean validate-bigstrings
