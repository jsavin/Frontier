#!/bin/bash
#
# install_cmake_universal.sh
#
# Downloads and installs cmake universal binary (arm64 + x86_64) for macOS.
#
# This script downloads the official pre-built cmake universal binary from
# cmake.org and installs it to third_party/cmake-install/.
#
# Usage:
#   ./tools/install_cmake_universal.sh
#
# Requirements:
#   - Must be run from the Frontier project root directory
#   - Internet connection to download cmake binary
#

set -e  # Exit on any error
set -u  # Exit on undefined variables

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_status() {
    echo -e "${BLUE}==>${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Verify we're in the project root
if [[ ! -f "frontier-cli/Makefile" ]]; then
    print_error "Must be run from Frontier project root directory"
    exit 1
fi

# Define paths and versions
CMAKE_VERSION="3.29.6"
CMAKE_TARBALL="cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
CMAKE_TARBALL_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_TARBALL}"
CMAKE_TARBALL_PATH="third_party/${CMAKE_TARBALL}"
CMAKE_INSTALL_DIR="third_party/cmake-install"
CMAKE_EXTRACTED_DIR="third_party/cmake-${CMAKE_VERSION}-macos-universal"

print_status "Installing cmake ${CMAKE_VERSION} universal binary"
echo "  Source: ${CMAKE_TARBALL_URL}"
echo "  Target: ${CMAKE_INSTALL_DIR}"
echo ""

# Ensure third_party directory exists
mkdir -p third_party

# Download cmake tarball if not already cached
if [[ ! -f "$CMAKE_TARBALL_PATH" ]]; then
    print_status "Downloading cmake ${CMAKE_VERSION} universal binary..."
    print_warning "This may take a few minutes (~50 MB download)..."
    curl -L -o "$CMAKE_TARBALL_PATH" "$CMAKE_TARBALL_URL"
    print_success "Download complete"
else
    print_status "Using cached tarball: $CMAKE_TARBALL_PATH"
fi

# Backup existing install if it exists
if [[ -d "$CMAKE_INSTALL_DIR" ]]; then
    BACKUP_DIR="${CMAKE_INSTALL_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
    print_warning "Backing up existing installation to: $BACKUP_DIR"
    mv "$CMAKE_INSTALL_DIR" "$BACKUP_DIR"
fi

# Extract tarball
print_status "Extracting cmake binary..."
tar -xzf "$CMAKE_TARBALL_PATH" -C third_party/

# Verify extraction created expected directory
if [[ ! -d "$CMAKE_EXTRACTED_DIR" ]]; then
    print_error "Expected directory not found after extraction: $CMAKE_EXTRACTED_DIR"
    exit 1
fi

# Move extracted directory to install location
mv "$CMAKE_EXTRACTED_DIR" "$CMAKE_INSTALL_DIR"
print_success "Extraction complete"

# Verify cmake binary exists
CMAKE_BINARY="$CMAKE_INSTALL_DIR/CMake.app/Contents/bin/cmake"

if [[ ! -f "$CMAKE_BINARY" ]]; then
    print_error "cmake binary not found at $CMAKE_BINARY"
    exit 1
fi

# Create convenience symlinks in cmake-install root
print_status "Creating convenience symlinks..."
cd "$CMAKE_INSTALL_DIR"
ln -sf CMake.app/Contents/bin bin
ln -sf CMake.app/Contents/share share
cd - > /dev/null
print_success "Symlinks created: bin/ and share/"

echo ""
print_status "========================================="
print_status "VERIFICATION"
print_status "========================================="
echo ""

print_status "cmake version:"
"$CMAKE_BINARY" --version | head -1

print_status "Binary architecture:"
file "$CMAKE_BINARY"

print_status "Detailed architecture info:"
lipo -info "$CMAKE_BINARY"

# Verify it's a universal binary (check for both architectures in any order)
if lipo -info "$CMAKE_BINARY" | grep -q "arm64" && lipo -info "$CMAKE_BINARY" | grep -q "x86_64"; then
    print_success "Successfully installed universal binary (arm64 + x86_64)"
else
    print_error "Binary is not universal!"
    lipo -info "$CMAKE_BINARY"
    exit 1
fi

# Show sizes
print_status "Binary size:"
ls -lh "$CMAKE_BINARY"

echo ""
print_success "========================================="
print_success "CMAKE INSTALLATION COMPLETE"
print_success "========================================="
echo ""
echo "Next steps:"
echo "  1. Test with: third_party/cmake-install/bin/cmake --version"
echo "  2. Build Paige: make -C frontier-cli"
echo ""
