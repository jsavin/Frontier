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
#   - third_party/cmake-src directory must exist with cmake source code
#   - Must be run from the Frontier project root directory
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

# Define paths
CMAKE_SRC_DIR="third_party/cmake-src"
CMAKE_INSTALL_DIR="third_party/cmake-install"
CMAKE_BUILD_STAGE1="third_party/cmake-build-stage1"
CMAKE_BUILD_STAGE2="third_party/cmake-build-stage2"

# Verify source directory exists
if [[ ! -d "$CMAKE_SRC_DIR" ]]; then
    print_error "cmake source directory not found: $CMAKE_SRC_DIR"
    exit 1
fi

if [[ ! -f "$CMAKE_SRC_DIR/bootstrap" ]]; then
    print_error "bootstrap script not found in $CMAKE_SRC_DIR"
    exit 1
fi

print_status "Starting cmake universal binary rebuild"
echo "  Source: $CMAKE_SRC_DIR"
echo "  Target: $CMAKE_INSTALL_DIR"
echo ""

# Clean existing build directories and stale cmake source artifacts
print_status "Cleaning existing build directories..."
rm -rf "$CMAKE_BUILD_STAGE1"
rm -rf "$CMAKE_BUILD_STAGE2"

# Clean any previous bootstrap/build artifacts in cmake-src using git clean
print_status "Cleaning stale cmake source artifacts..."
(cd "$CMAKE_SRC_DIR" && git clean -fdx > /dev/null 2>&1) || true
print_success "Build directories and source artifacts cleaned"

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

# Stage 1: Bootstrap cmake in separate build directory
mkdir -p "$CMAKE_BUILD_STAGE1"
cd "$CMAKE_BUILD_STAGE1"

print_status "Running bootstrap script..."
print_warning "This may take several minutes..."
echo ""

# Get absolute paths
ABSSRCDIR="$(cd ../cmake-src && pwd)"
ABSINSTALLDIR="$(cd .. && pwd)/cmake-install"

# Run bootstrap with parallel build and install prefix
../cmake-src/bootstrap --prefix="$ABSINSTALLDIR" --parallel=$(sysctl -n hw.ncpu) 2>&1 | tee bootstrap.log

if [[ ! -f "bin/cmake" ]]; then
    print_error "Stage 1 failed: cmake binary not created"
    exit 1
fi

print_success "Stage 1 complete: Bootstrap cmake created"

# Verify stage 1 cmake
STAGE1_CMAKE="$(pwd)/bin/cmake"
print_status "Stage 1 cmake version:"
"$STAGE1_CMAKE" --version | head -1

# Go back to project root
cd - > /dev/null

echo ""
print_status "========================================="
print_status "STAGE 2: Build universal binary"
print_status "========================================="
echo ""

# Stage 2: Build universal binary
mkdir -p "$CMAKE_BUILD_STAGE2"
cd "$CMAKE_BUILD_STAGE2"

print_status "Configuring cmake with universal architecture support..."
echo ""

# Configure with universal architectures
"$STAGE1_CMAKE" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$ABSINSTALLDIR" \
    "$ABSSRCDIR" 2>&1 | tee configure.log

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

# Clean up stage build directories
print_status "Cleaning up build directories..."
cd - > /dev/null
rm -rf "$CMAKE_BUILD_STAGE1"
rm -rf "$CMAKE_BUILD_STAGE2"

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

print_status "cmake version:"
"$CMAKE_BINARY" --version | head -1

print_status "Binary architecture:"
file "$CMAKE_BINARY"

print_status "Detailed architecture info:"
lipo -info "$CMAKE_BINARY"

# Verify it's a universal binary
if lipo -info "$CMAKE_BINARY" | grep -q "arm64 x86_64"; then
    print_success "Successfully built universal binary (arm64 + x86_64)"
else
    print_error "Binary is not universal!"
    lipo -info "$CMAKE_BINARY"
    exit 1
fi

# Show sizes
print_status "Binary sizes:"
ls -lh "$CMAKE_BINARY"

echo ""
print_success "========================================="
print_success "CMAKE UNIVERSAL BINARY BUILD COMPLETE"
print_success "========================================="
echo ""
echo "Next steps:"
echo "  1. Test with: third_party/cmake-install/bin/cmake --version"
echo "  2. Rebuild: make -C frontier-cli"
echo "  3. Commit the rebuilt cmake-install directory"
