#!/bin/bash
# Frontier Development Tools Installation Script
#
# This script installs all static analysis and code quality tools needed for Frontier development.
# Tools include: cflow, ctags, clang-tools, ripgrep, tree, bear
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

# List of tools to install
declare -A TOOLS=(
    [cflow]="call graph analysis"
    [universal-ctags]="symbol indexing and cross-reference"
    [clang-tools]="LLVM static analysis"
    [ripgrep]="fast pattern search"
    [tree]="directory visualization"
    [bear]="compilation database generation"
)

# Track installation status
failed=()
succeeded=()

echo -e "${YELLOW}Installing tools...${NC}"
echo ""

for tool in "${!TOOLS[@]}"; do
    description="${TOOLS[$tool]}"
    echo -n "Installing ${BLUE}${tool}${NC} (${description})... "

    if brew install "$tool" &> /dev/null; then
        echo -e "${GREEN}✓${NC}"
        succeeded+=("$tool")
    else
        echo -e "${RED}✗${NC}"
        failed+=("$tool")
    fi
done

echo ""
echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Installation Summary${NC}"
echo -e "${BLUE}================================================${NC}"

if [ ${#succeeded[@]} -gt 0 ]; then
    echo -e "${GREEN}Successfully installed (${#succeeded[@]})${NC}:"
    for tool in "${succeeded[@]}"; do
        echo "  ✓ $tool"
    done
    echo ""
fi

if [ ${#failed[@]} -gt 0 ]; then
    echo -e "${RED}Failed to install (${#failed[@]})${NC}:"
    for tool in "${failed[@]}"; do
        echo "  ✗ $tool"
    done
    echo ""
    echo -e "${YELLOW}Try installing manually:${NC}"
    for tool in "${failed[@]}"; do
        echo "  brew install $tool"
    done
fi

echo ""
echo -e "${YELLOW}Verification:${NC}"
echo ""

# Verify installations
all_good=true
for tool in "${!TOOLS[@]}"; do
    if command -v "$tool" &> /dev/null || brew list "$tool" &> /dev/null; then
        version=$(eval "$tool" --version 2>&1 | head -1 || echo "installed")
        echo -e "  ${GREEN}✓${NC} $tool: $version"
    else
        echo -e "  ${RED}✗${NC} $tool: NOT FOUND"
        all_good=false
    fi
done

echo ""
if [ "$all_good" = true ]; then
    echo -e "${GREEN}✓ All tools installed successfully!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Read the development guide: DEVELOPER_SETUP.md"
    echo "  2. Try analyzing the codebase: cflow Common/source/*.c"
    echo "  3. Generate call graphs: ctags -R Common/"
    exit 0
else
    echo -e "${RED}✗ Some tools failed to install${NC}"
    echo "Please install missing tools manually using: brew install <tool>"
    exit 1
fi
