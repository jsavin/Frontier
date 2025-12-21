#!/bin/bash
# Frontier Development Tools Installation Script
#
# This script installs all static analysis and code quality tools needed for Frontier development.
# Tools include: cflow, ctags, llvm (clang-tools), ripgrep, tree, bear
#
# Usage: ./scripts/install_dev_tools.sh
# Or:    curl -fsSL https://raw.githubusercontent.com/jsavin/Frontier/develop/scripts/install_dev_tools.sh | bash

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Frontier Development Tools Installation${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo -e "${RED}Error: Homebrew is not installed${NC}"
    echo "Please install Homebrew first: https://brew.sh"
    exit 1
fi

echo -e "${YELLOW}Prerequisites check:${NC}"
echo "✓ Homebrew installed: $(brew --version | head -1)"
echo ""

echo -e "${YELLOW}Installing tools...${NC}"
echo ""

# Track installation status
succeeded=0
failed=0
failed_list=""

# Define tools as separate variables for clarity
install_tool() {
    local package=$1
    local command=$2
    local description=$3

    echo -n "Installing ${BLUE}${package}${NC} (${description})... "

    if brew install "$package" &> /dev/null; then
        echo -e "${GREEN}✓${NC}"
        ((succeeded++))
    else
        echo -e "${RED}✗${NC}"
        ((failed++))
        failed_list="$failed_list\n  $package"
    fi
}

# Install each tool
install_tool "cflow" "cflow" "call graph analysis"
install_tool "universal-ctags" "ctags" "symbol indexing and cross-reference"
install_tool "llvm" "clang-check" "LLVM toolchain and clang-tools"
install_tool "ripgrep" "rg" "fast pattern search"
install_tool "tree" "tree" "directory visualization"
install_tool "bear" "bear" "compilation database generation"

echo ""
echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Installation Summary${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

if [ $succeeded -gt 0 ]; then
    echo -e "${GREEN}Successfully installed ($succeeded tools)${NC}:"
    [ -x "$(command -v cflow)" ] && echo "  ✓ cflow"
    [ -x "$(command -v ctags)" ] && echo "  ✓ universal-ctags"
    [ -x "/opt/homebrew/opt/llvm/bin/clang-check" ] && echo "  ✓ llvm (clang-tools)"
    [ -x "$(command -v rg)" ] && echo "  ✓ ripgrep"
    [ -x "$(command -v tree)" ] && echo "  ✓ tree"
    [ -x "$(command -v bear)" ] && echo "  ✓ bear"
    echo ""
fi

if [ $failed -gt 0 ]; then
    echo -e "${RED}Failed to install ($failed tools)${NC}:"
    echo -e "$failed_list"
    echo ""
fi

echo -e "${YELLOW}Verification:${NC}"
echo ""

# Verify installations
all_good=true
verify_tool() {
    local package=$1
    local command=$2

    if command -v "$command" &> /dev/null; then
        version=$("$command" --version 2>&1 | head -1)
        echo -e "  ${GREEN}✓${NC} $package: $version"
    else
        echo -e "  ${RED}✗${NC} $package: NOT FOUND"
        all_good=false
    fi
}

verify_tool "cflow" "cflow"
verify_tool "universal-ctags" "ctags"
verify_tool "llvm" "/opt/homebrew/opt/llvm/bin/clang-check"
verify_tool "ripgrep" "rg"
verify_tool "tree" "tree"
verify_tool "bear" "bear"

echo ""
if [ "$all_good" = true ]; then
    echo -e "${GREEN}✓ All tools installed successfully!${NC}"
    echo ""
    echo "Note: llvm tools (clang-check, clang-format, clang-tidy) are installed at:"
    echo "  /opt/homebrew/opt/llvm/bin/"
    echo ""
    echo "To use them from the command line, add to your shell profile (~/.bash_profile, ~/.zprofile, etc.):"
    echo "  export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    echo ""
    echo "Next steps:"
    echo "  1. Read the development guide: DEVELOPER_SETUP.md"
    echo "  2. Try analyzing the codebase: cflow Common/source/*.c"
    echo "  3. Generate call graphs: ctags -R Common/"
    exit 0
else
    echo -e "${RED}✗ Some tools failed to install${NC}"
    echo "Please install missing tools manually:"
    echo "  brew install cflow universal-ctags llvm ripgrep tree bear"
    echo ""
    echo "Note: llvm tools are installed to /opt/homebrew/opt/llvm/bin/"
    echo "To use clang tools from the command line, add to your shell profile:"
    echo "  export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    exit 1
fi
