# Frontier CLI Makefile
# Phase 3 headless bring-up: build the CLI without UI frameworks.

CC = clang
CFLAGS = -std=c17 -Wall -Wextra -O0 -g -fpascal-strings \
         -DFRONTIER_HEADLESS -DFRONTIER_USE_PORTABLE_HANDLES \
         -DHEADLESS_TEST_PORTABLE_FILE -DHEADLESS_LINKS_REAL_DB \
         -DHEADLESS_USE_REAL_TABLEPACK
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

INCLUDES = -I$(HEADERDIR) -I$(SYSTEMHEADERDIR) -I$(PORTABLEDIR) -I../build_Xcode_modern

# CLI sources
CLI_SOURCES = \
    main.c \
    cli_parser.c \
    cli_utils.c \
    cli_executor.c

# Core runtime sources needed for script execution (aligned with tests/runtime_tests)
RUNTIME_SOURCES = \
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
    ../Common/source/memory.c \
    ../Common/source/portable_handles.c \
    ../Common/source/strings.c \
    ../Common/source/strings_extras.c \
    ../Common/source/FastTimes.c \
    ../Common/source/md5.c \
    ../Common/source/sha1dgst.c \
    ../Common/source/whirlpool.c \
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
    ../Common/source/tablepack.c \
    ../Common/source/shell_api.c \
    ../Common/source/shell_api_headless.c

# Headless adapters mirrored from tests/
HEADLESS_STUBS = \
    $(TESTSDIR)/headless_shell.c \
    $(TESTSDIR)/headless_shellhooks.c \
    $(TESTSDIR)/headless_table_stubs.c \
    $(TESTSDIR)/headless_wp_stubs.c \
    $(TESTSDIR)/headless_pict_stubs.c \
    $(TESTSDIR)/headless_menu_stubs.c \
    $(TESTSDIR)/headless_search_stubs.c \
    $(TESTSDIR)/headless_mac_compat.c \
    $(TESTSDIR)/headless_langregexp_stub.c \
    $(TESTSDIR)/headless_langipc_stub.c \
    $(TESTSDIR)/headless_lang_runtime_more_stubs.c \
    $(TESTSDIR)/headless_file_portable.c \
    $(TESTSDIR)/headless_file_verbs.c \
    $(TESTSDIR)/headless_odb_stubs.c

# Database and file runtime sources required for Frontier.root access
DATABASE_SOURCES = \
    ../Common/source/db.c \
    ../Common/source/db_format.c \
    ../Common/source/odbengine.c \
    ../Common/source/findinfile.c

# Aggregate sources
FRONTIER_SOURCES = $(RUNTIME_SOURCES) $(DATABASE_SOURCES) $(HEADLESS_STUBS)

TARGET = frontier-cli

all: $(TARGET)

$(TARGET): $(CLI_SOURCES) $(FRONTIER_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) \
		$(CLI_SOURCES) \
		$(FRONTIER_SOURCES) \
		-o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
