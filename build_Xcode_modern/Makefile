# Frontier Modern Build Makefile
# Phase 0: 64-bit & ARM Architecture Foundation

CC = clang
CFLAGS = -std=c17 -Wall -Wextra -fPIC -arch arm64 -arch x86_64
OBJCFLAGS = -fobjc-arc
FRAMEWORKS = -framework Foundation -framework AppKit -framework Carbon -framework CoreFoundation

# Directories
SRCDIR = ../Common/source
HEADERDIR = ../Common/headers
SYSTEMHEADERDIR = ../Common/SystemHeaders
MODERNHEADERDIR = .

# Source files (starting with just main_modern.m for testing)
SOURCES = main_modern.m
HEADERS = $(HEADERDIR)/frontier.h $(SYSTEMHEADERDIR)/standard.h

# Target
TARGET = Frontier

# Default target
all: $(TARGET)

# Compile the main executable
$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJCFLAGS) -I$(HEADERDIR) -I$(SYSTEMHEADERDIR) -I$(MODERNHEADERDIR) $(SOURCES) $(FRAMEWORKS) -o $(TARGET)

# Clean
clean:
	rm -f $(TARGET)

# Test compilation
test: $(TARGET)
	@echo "Compilation successful!"
	@echo "Target: $(TARGET)"
	@echo "Architectures:"
	@file $(TARGET)

.PHONY: all clean test
