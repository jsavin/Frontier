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
         -DUNIX_COMPILE -DC_LIBRARY -DNO_OS_INLINE
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

INCLUDES = -I$(HEADERDIR) -I$(SYSTEMHEADERDIR) -I$(PORTABLEDIR) -I../build_Xcode_modern -I../generated

# Paige integration (WPText engine)
THIRDPARTYDIR = ../third_party
PAIGE_DIR = $(THIRDPARTYDIR)/Paige
PAIGE_BUILD_DIR = $(PAIGE_DIR)/build-headless
PAIGE_LIB = $(PAIGE_BUILD_DIR)/libpaige.a
CMAKE_BIN = $(THIRDPARTYDIR)/cmake-install/bin/cmake
PAIGE_ARCHES = arm64;x86_64

INCLUDES += -I$(PAIGE_DIR)/PGHEADER
LIBS = $(PAIGE_LIB)
LDFLAGS = -Wl,-undefined,dynamic_lookup

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
    headless_scripts_loader.c

# Core runtime sources needed for script execution (aligned with tests/runtime_tests)
RUNTIME_SOURCES = \
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
    ../Common/source/tableops.c \
    ../Common/source/tablestructure.c \
    ../Common/source/tableexternal_common.c \
    ../Common/source/tableexternal.c \
    ../Common/source/tablepack.c \
    ../Common/source/tableverbs.c \
    ../Common/source/stringverbs.c \
    ../Common/source/headless_selection.c \
    ../Common/source/shell_api.c \
    ../Common/source/shell_api_headless.c

# Headless adapters mirrored from tests/
HEADLESS_STUBS = \
    $(TESTSDIR)/headless_shell.c \
    $(TESTSDIR)/headless_shellhooks.c \
    $(TESTSDIR)/headless_table_stubs.c \
    ../portable/wptext_runtime.c \
    ../portable/paige_text_extractor.c \
    $(TESTSDIR)/headless_pict_stubs.c \
    $(TESTSDIR)/headless_menu_stubs.c \
    $(TESTSDIR)/headless_search_stubs.c \
    $(TESTSDIR)/headless_mac_compat.c \
    $(TESTSDIR)/headless_langregexp_stub.c \
    $(TESTSDIR)/headless_langipc_stub.c \
    $(TESTSDIR)/headless_lang_runtime_more_stubs.c \
    ../portable/file_portable.c \
    ../portable/file_working_dir.c \
    ../portable/fileverbs_portable.c \
    ../Common/source/file_portable_posix.c \
    ../Common/source/headless_stubs.c \
    ../portable/appleevent_portable.c \
    ../portable/classic_handle.c \
    ../portable/script_portable.c \
    ../portable/wptext_portable.c \
    $(TESTSDIR)/headless_lang_verbs.c \
    $(TESTSDIR)/headless_frontier_verbs.c \
    $(TESTSDIR)/headless_file_verbs.c \
    $(TESTSDIR)/headless_table_verbs.c \
    $(TESTSDIR)/headless_odb_stubs.c \
    $(TESTSDIR)/headless_threadglobals.c \
    $(TESTSDIR)/headless_op_verbs.c \
    $(TESTSDIR)/headless_opattributes_verbs.c \
    $(TESTSDIR)/headless_script_verbs.c \
    $(TESTSDIR)/headless_osa_verbs.c \
    $(TESTSDIR)/headless_menu_verbs.c \
    $(TESTSDIR)/headless_pict_verbs.c \
    $(TESTSDIR)/headless_clock_verbs.c \
    $(TESTSDIR)/headless_date_verbs.c \
    $(TESTSDIR)/headless_dialog_verbs.c \
    $(TESTSDIR)/headless_kb_verbs.c \
    $(TESTSDIR)/headless_mouse_verbs.c \
    $(TESTSDIR)/headless_point_verbs.c \
    $(TESTSDIR)/headless_rectangle_verbs.c \
    $(TESTSDIR)/headless_rgb_verbs.c \
    $(TESTSDIR)/headless_speaker_verbs.c \
    $(TESTSDIR)/headless_target_verbs.c \
    $(TESTSDIR)/headless_bit_verbs.c \
    $(TESTSDIR)/headless_semaphore_verbs.c \
    $(TESTSDIR)/headless_base64_verbs.c \
    $(TESTSDIR)/headless_tcp_verbs.c \
    $(TESTSDIR)/headless_dll_verbs.c \
    $(TESTSDIR)/headless_python_verbs.c \
    $(TESTSDIR)/headless_htmlcontrol_verbs.c \
    $(TESTSDIR)/headless_statusbar_verbs.c \
    $(TESTSDIR)/headless_sys_verbs.c \
    $(TESTSDIR)/headless_string_verbs.c \
    $(TESTSDIR)/headless_db_verbs.c \
    $(TESTSDIR)/headless_html_verbs.c \
    $(TESTSDIR)/headless_re_verbs.c \
    $(TESTSDIR)/headless_sqlite_verbs.c \
    $(TESTSDIR)/headless_mysql_verbs.c \
    $(TESTSDIR)/headless_window_verbs.c \
    $(TESTSDIR)/headless_rez_verbs.c \
    $(TESTSDIR)/headless_search_verbs.c \
    $(TESTSDIR)/headless_filemenu_verbs.c \
    $(TESTSDIR)/headless_editmenu_verbs.c \
    $(TESTSDIR)/headless_launch_verbs.c \
    $(TESTSDIR)/headless_clipboard_verbs.c \
    $(TESTSDIR)/headless_thread_verbs.c \
    $(TESTSDIR)/headless_mainwindow_verbs.c \
    $(TESTSDIR)/headless_searchengine_verbs.c \
    $(TESTSDIR)/headless_mrcalendar_verbs.c \
    $(TESTSDIR)/headless_webserver_verbs.c \
    $(TESTSDIR)/headless_inetd_verbs.c

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

all: strings_generated kernel_verbs_generated $(TARGET)

$(TARGET): $(CLI_SOURCES) $(FRONTIER_SOURCES) $(PAIGE_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) \
		$(CLI_SOURCES) \
		$(FRONTIER_SOURCES) \
		-o $(TARGET) $(LIBS) $(LDFLAGS)

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
# Uses --analyze flag to automatically detect implemented verbs from C source
$(KERNEL_VERBS_C): $(KERNELVERBS_PARSER) $(KERNELVERBS_RC)
	@mkdir -p $(GENERATED_DIR)
	@echo "Generating kernel_verbs_init.c from kernelverbs.rc..."
	@python3 $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(KERNEL_VERBS_C) --analyze

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

clean:
	rm -f $(TARGET)

$(PAIGE_LIB):
	$(CMAKE_BIN) -S $(PAIGE_DIR) -B $(PAIGE_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$(PAIGE_ARCHES)"
	$(CMAKE_BIN) --build $(PAIGE_BUILD_DIR)

.PHONY: all clean strings_generated kernel_verbs_generated
