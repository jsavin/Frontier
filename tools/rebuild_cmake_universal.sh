#!/bin/bash
#
# rebuild_cmake_universal.sh
#
# Rebuilds cmake as a universal binary (arm64 + x86_64) from source.
#
# Two-stage build process:
#   Stage 1: Bootstrap cmake from source to get a working cmake binary
#   Stage 2: Use that cmake to rebuild itself with universal architecture support
#
# Usage:
#   ./tools/rebuild_cmake_universal.sh
#
# Requirements:
#   - Must be run from the Frontier project root directory
#   - Internet connection to download cmake source (first run only)
#

set -e  # Exit on any error
set -u  # Exit on undefined variables

# Cleanup function to remove temporary directory
cleanup() {
    if [[ -n "${CMAKE_TEMP_BUILD_DIR:-}" ]] && [[ -d "$CMAKE_TEMP_BUILD_DIR" ]]; then
        print_status "Cleaning up temporary build directory..."
        rm -rf "$CMAKE_TEMP_BUILD_DIR"
    fi
}

# Register cleanup on EXIT
trap cleanup EXIT

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
CMAKE_VERSION="3.31.6"
CMAKE_VERSION_SHORT="3.31"
CMAKE_TARBALL="cmake-${CMAKE_VERSION}.tar.gz"
CMAKE_TARBALL_URL="https://cmake.org/files/v${CMAKE_VERSION_SHORT}/${CMAKE_TARBALL}"
CMAKE_TARBALL_SHA256="653427f0f5014750aafff22727fb2aa60c6c732ca91808cfb78ce22ddd9e55f0"
CMAKE_TARBALL_PATH="third_party/${CMAKE_TARBALL}"
CMAKE_INSTALL_DIR="third_party/cmake-install"
CMAKE_TEMP_BUILD_DIR="/tmp/cmake-${CMAKE_VERSION}-build-$$"

print_status "Starting cmake universal binary rebuild"
echo "  Version: ${CMAKE_VERSION}"
echo "  Target: $CMAKE_INSTALL_DIR"
echo ""

# Ensure third_party directory exists
mkdir -p third_party

# Download cmake source tarball if not already cached
if [[ ! -f "$CMAKE_TARBALL_PATH" ]]; then
    print_status "Downloading cmake ${CMAKE_VERSION} source tarball..."
    print_warning "This may take a few minutes..."
    curl -L -o "$CMAKE_TARBALL_PATH" "$CMAKE_TARBALL_URL"
    print_success "Download complete"
else
    print_status "Using cached tarball: $CMAKE_TARBALL_PATH"
fi

# Verify SHA256 checksum
print_status "Verifying tarball checksum..."
ACTUAL_SHA256=$(shasum -a 256 "$CMAKE_TARBALL_PATH" | cut -d' ' -f1)
if [[ "$ACTUAL_SHA256" != "$CMAKE_TARBALL_SHA256" ]]; then
    print_error "SHA256 checksum mismatch!"
    echo "  Expected: $CMAKE_TARBALL_SHA256"
    echo "  Got:      $ACTUAL_SHA256"
    print_warning "Removing potentially corrupted tarball..."
    rm -f "$CMAKE_TARBALL_PATH"
    exit 1
fi
print_success "Checksum verified"

# Extract to temporary directory
print_status "Extracting source to temporary directory..."
mkdir -p "$CMAKE_TEMP_BUILD_DIR"
tar -xzf "$CMAKE_TARBALL_PATH" -C "$CMAKE_TEMP_BUILD_DIR" --strip-components=1
print_success "Extraction complete: $CMAKE_TEMP_BUILD_DIR"

# Backup existing install if it exists
if [[ -d "$CMAKE_INSTALL_DIR" ]]; then
    BACKUP_DIR="${CMAKE_INSTALL_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
    print_warning "Backing up existing installation to: $BACKUP_DIR"
    mv "$CMAKE_INSTALL_DIR" "$BACKUP_DIR"
fi

# Remove install directory
rm -rf "$CMAKE_INSTALL_DIR"
print_success "Install directory cleaned"

echo ""
print_status "========================================="
print_status "STAGE 1: Bootstrap cmake (host arch)"
print_status "========================================="
echo ""

# Stage 1: Bootstrap cmake from temporary directory
cd "$CMAKE_TEMP_BUILD_DIR"

print_status "Running bootstrap script..."
print_warning "This may take several minutes..."
echo ""

# Run bootstrap with parallel build
./bootstrap --parallel=$(sysctl -n hw.ncpu) 2>&1 | tee bootstrap.log

print_success "Bootstrap configuration complete"
print_status "Building stage 1 cmake..."
echo ""

# Build cmake after bootstrap
make -j$(sysctl -n hw.ncpu) 2>&1 | tee make.log

if [[ ! -f "bin/cmake" ]]; then
    print_error "Stage 1 failed: cmake binary not created"
    exit 1
fi

print_success "Stage 1 complete: Bootstrap cmake created"

# Verify stage 1 cmake
STAGE1_CMAKE="$(pwd)/bin/cmake"
print_status "Stage 1 cmake version:"
"$STAGE1_CMAKE" --version | head -1

echo ""
print_status "========================================="
print_status "STAGE 2: Build universal binary"
print_status "========================================="
echo ""

# Get absolute path to source and install directory
PROJECT_ROOT="$(cd /Users/jake/dev/jsavin/Frontier-universal-cmake-binary && pwd)"
CMAKE_SRC_DIR="$CMAKE_TEMP_BUILD_DIR"
INSTALL_PREFIX="$PROJECT_ROOT/$CMAKE_INSTALL_DIR"

# Stage 2: Build universal binary in a separate build directory (not inside temp dir)
CMAKE_BUILD_STAGE2="$PROJECT_ROOT/third_party/cmake-build-universal"
rm -rf "$CMAKE_BUILD_STAGE2"
mkdir -p "$CMAKE_BUILD_STAGE2"
cd "$CMAKE_BUILD_STAGE2"

print_status "Configuring cmake with universal architecture support..."
echo ""

# Configure with universal architectures
"$STAGE1_CMAKE" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    "$CMAKE_SRC_DIR" 2>&1 | tee configure.log

print_success "Configuration complete"

print_status "Building cmake with $(sysctl -n hw.ncpu) parallel jobs..."
print_warning "This may take several minutes..."
echo ""

# Build with parallel jobs
make -j$(sysctl -n hw.ncpu) 2>&1 | tee build.log

print_success "Build complete"

print_status "Installing to $CMAKE_INSTALL_DIR..."
make install 2>&1 | tee install.log

print_success "Installation complete"

# Clean up stage 2 build directory
print_status "Cleaning up stage 2 build directory..."
rm -rf "$CMAKE_BUILD_STAGE2"

# Return to project root
cd "$PROJECT_ROOT"

echo ""
print_status "========================================="
print_status "VERIFICATION"
print_status "========================================="
echo ""

# Verify the binary
CMAKE_BINARY="$CMAKE_INSTALL_DIR/bin/cmake"

if [[ ! -f "$CMAKE_BINARY" ]]; then
    print_error "cmake binary not found at $CMAKE_BINARY"
    exit 1
fi

print_status "cmake binary location: $CMAKE_BINARY"
echo ""

print_status "File type:"
file "$CMAKE_BINARY"
echo ""

print_status "Architecture information:"
lipo -info "$CMAKE_BINARY"
echo ""

# Check if it's actually universal
if lipo -info "$CMAKE_BINARY" 2>&1 | grep -q "arm64.*x86_64\|x86_64.*arm64"; then
    print_success "SUCCESS: cmake is a universal binary with both arm64 and x86_64"
else
    print_error "FAILED: cmake is not a universal binary"
    print_warning "lipo output:"
    lipo -info "$CMAKE_BINARY"
    exit 1
fi

print_status "cmake version:"
"$CMAKE_BINARY" --version | head -1
echo ""

# Display sizes
print_status "Binary sizes:"
echo "  Stage 1 (bootstrap): $(du -h "$CMAKE_TEMP_BUILD_DIR/bin/cmake" | cut -f1)"
echo "  Stage 2 (universal): $(du -h "$CMAKE_BINARY" | cut -f1)"
echo ""

print_success "cmake universal binary rebuild complete!"
echo ""
print_status "Next steps:"
echo "  1. Test the cmake binary: $CMAKE_BINARY --version"
echo "  2. Try building frontier-cli with the new cmake"
echo ""
print_status "Note: Temporary build directory will be automatically cleaned up"
echo ""
