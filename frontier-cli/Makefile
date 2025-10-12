# Frontier CLI Makefile
# Phase 1: CLI-Based UserTalk Invocation Implementation

CC = clang
CFLAGS = -std=c17 -Wall -Wextra -fPIC -arch arm64 -arch x86_64
OBJCFLAGS = -fobjc-arc
FRAMEWORKS = -framework Foundation -framework AppKit -framework Carbon -framework CoreFoundation

# Directories
SRCDIR = .
HEADERDIR = ../Common/headers
SYSTEMHEADERDIR = ../Common/SystemHeaders
MODERNHEADERDIR = ../build_Xcode_modern

# CLI source files
CLI_SOURCES = main.c cli_parser.c cli_utils.c cli_executor.c cli_database.c cli_network.c

# Frontier source files (minimal set for CLI)
FRONTIER_SOURCES = \
	../Common/source/lang.c \
	../Common/source/langevaluate.c \
	../Common/source/langvalue.c \
	../Common/source/db.c \
	../Common/source/file.c \
	../Common/source/memory.c \
	../Common/source/strings.c \
	../Common/source/error.c \
	../Common/source/shell.c \
	../Common/source/shellprivate.c \
	../Common/source/frontierstart.c \
	../Common/source/threads.c

# Headers
HEADERS = \
	$(HEADERDIR)/frontier.h \
	$(HEADERDIR)/standard.h \
	$(HEADERDIR)/db.h \
	$(HEADERDIR)/lang.h \
	$(HEADERDIR)/langevaluate.h \
	$(HEADERDIR)/langvalue.h \
	$(HEADERDIR)/file.h \
	$(HEADERDIR)/memory.h \
	$(HEADERDIR)/strings.h \
	$(HEADERDIR)/error.h \
	$(HEADERDIR)/shell.h \
	$(HEADERDIR)/shellprivate.h \
	$(HEADERDIR)/frontierstart.h \
	$(HEADERDIR)/threads.h \
	$(SYSTEMHEADERDIR)/standard.h \
	$(MODERNHEADERDIR)/frontier_compat.h

# Target
TARGET = frontier-cli

# Default target
all: $(TARGET)

# Compile the CLI executable
$(TARGET): $(CLI_SOURCES) $(FRONTIER_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJCFLAGS) \
		-I$(HEADERDIR) \
		-I$(SYSTEMHEADERDIR) \
		-I$(MODERNHEADERDIR) \
		$(CLI_SOURCES) \
		$(FRONTIER_SOURCES) \
		$(FRAMEWORKS) \
		-o $(TARGET)

# Clean
clean:
	rm -f $(TARGET)
	rm -f *.o

# Test compilation
test: $(TARGET)
	@echo "Compilation successful!"
	@echo "Target: $(TARGET)"
	@echo "Architectures:"
	@file $(TARGET)

# Install (optional)
install: $(TARGET)
	@echo "Installing frontier-cli..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "Installation complete!"

# Uninstall (optional)
uninstall:
	@echo "Uninstalling frontier-cli..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstallation complete!"

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Profile build
profile: CFLAGS += -pg
profile: $(TARGET)

# Static analysis
analyze: CFLAGS += -fsanitize=address -fsanitize=undefined
analyze: $(TARGET)

# Documentation
docs:
	@echo "Generating documentation..."
	@mkdir -p docs
	@echo "# Frontier CLI Documentation" > docs/README.md
	@echo "" >> docs/README.md
	@echo "## Usage" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "\`\`\`bash" >> docs/README.md
	@echo "# Execute a UserTalk script" >> docs/README.md
	@echo "./frontier-cli script.usertalk" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "# Execute inline script" >> docs/README.md
	@echo "./frontier-cli -e \"local(x = 5); x * 2\"" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "# Query database" >> docs/README.md
	@echo "./frontier-cli -d test.root -q \"db.getValue('path')\"" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "# Start HTTP server" >> docs/README.md
	@echo "./frontier-cli --server --port 8080" >> docs/README.md
	@echo "\`\`\`" >> docs/README.md
	@echo "Documentation generated in docs/README.md"

# Help
help:
	@echo "Frontier CLI Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build the CLI executable (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  test       - Build and show target info"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  uninstall  - Remove from /usr/local/bin"
	@echo "  debug      - Build with debug symbols"
	@echo "  profile    - Build with profiling"
	@echo "  analyze    - Build with static analysis"
	@echo "  docs       - Generate documentation"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Environment variables:"
	@echo "  CC         - C compiler (default: clang)"
	@echo "  CFLAGS     - C compiler flags"
	@echo "  TARGET     - Output executable name (default: frontier-cli)"

.PHONY: all clean test install uninstall debug profile analyze docs help
