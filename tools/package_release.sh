#!/bin/bash
#
# Frontier CLI Release Packaging Script
# Creates release distribution archives for GitHub releases
#
# Usage: ./tools/package_release.sh [VERSION]
#   VERSION: Git tag (e.g., v1.0.0-alpha.1)
#            If not provided, uses current git describe output
#

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_error() {
    echo -e "${RED}Error: $1${NC}" >&2
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}$1${NC}"
}

# Get version from argument or git
if [ -n "$1" ]; then
    VERSION="$1"
else
    VERSION=$(git describe --tags --always 2>/dev/null || echo "v1.0.0-dev")
fi

# Strip leading 'v' if present for directory naming
VERSION_CLEAN=${VERSION#v}

print_info "Packaging Frontier CLI $VERSION"

# Get repository root
REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
cd "$REPO_ROOT"

# Create dist directory
DIST_DIR="$REPO_ROOT/dist"
mkdir -p "$DIST_DIR"

# Create staging directory for package contents
STAGE_DIR="$DIST_DIR/frontier-cli-${VERSION_CLEAN}-macos"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

print_info "Building universal binary..."

# Build universal binary (arm64 + x86_64)
cd "$REPO_ROOT/frontier-cli"
make clean > /dev/null 2>&1
ARCHES="arm64 x86_64" VERSION="$VERSION" make

if [ ! -f "frontier-cli" ]; then
    print_error "Build failed - frontier-cli binary not found"
    exit 1
fi

# Verify universal binary
if ! file frontier-cli | grep -q "universal binary"; then
    print_error "Build produced non-universal binary"
    exit 1
fi

print_success "Universal binary built (arm64 + x86_64)"

# Copy binary to staging
cp frontier-cli "$STAGE_DIR/"
print_success "Binary copied to staging"

# Migrate database to v7 if needed
cd "$REPO_ROOT"
if [ -f "databases/Frontier.root7" ]; then
    print_info "Using existing v7 database"
    cp databases/Frontier.root7 "$STAGE_DIR/"
elif [ -f "databases/Frontier.root" ]; then
    print_info "Migrating v6 database to v7..."
    # Run migration
    ./frontier-cli/frontier-cli --migrate databases/Frontier.root
    if [ -f "databases/Frontier.root7" ]; then
        cp databases/Frontier.root7 "$STAGE_DIR/"
        print_success "Database migrated and copied"
    else
        print_error "Migration failed - Frontier.root7 not created"
        exit 1
    fi
else
    print_error "No system root database found (databases/Frontier.root or Frontier.root7)"
    exit 1
fi

# Copy installation script
cp install.sh "$STAGE_DIR/"
chmod +x "$STAGE_DIR/install.sh"
print_success "Installation script copied"

# Create README for the release package
cat > "$STAGE_DIR/README.txt" << 'EOF'
Frontier CLI - Pre-Release Distribution
========================================

This package contains:
  - frontier-cli      Universal binary (arm64 + x86_64)
  - Frontier.root7    System root database
  - install.sh        Installation script

QUICK START
-----------

1. Extract this archive:
   unzip frontier-cli-VERSION-macos.zip
   cd frontier-cli-VERSION-macos

2. Run the installer:
   ./install.sh

   This will:
   - Copy frontier-cli to /usr/local/bin (or ~/.local/bin)
   - Copy Frontier.root7 to ~/Library/Application Support/Frontier/
   - Optionally add to your PATH

3. Verify installation:
   frontier-cli --version

4. Start using Frontier:
   frontier-cli                # Interactive REPL mode
   frontier-cli -e "1+1"       # Execute inline script

SYSTEM REQUIREMENTS
-------------------

- macOS 11.0 (Big Sur) or later
- Apple Silicon (arm64) or Intel (x86_64) Mac

DOCUMENTATION
-------------

For detailed documentation, see:
  https://github.com/jsavin/Frontier/blob/develop/INSTALL.md
  https://github.com/jsavin/Frontier/blob/develop/docs/CLI_USAGE_GUIDE.md

SUPPORT
-------

Report issues: https://github.com/jsavin/Frontier/issues

EOF

print_success "README.txt created"

# Create changelog excerpt if CHANGELOG.md exists
if [ -f "CHANGELOG.md" ]; then
    print_info "Extracting changelog for $VERSION..."
    # Extract this version's changelog section
    awk "/^## \[?${VERSION_CLEAN}\]?/,/^## \[?[0-9]/" CHANGELOG.md | head -n -1 > "$STAGE_DIR/CHANGELOG.txt" 2>/dev/null || true
    if [ -s "$STAGE_DIR/CHANGELOG.txt" ]; then
        print_success "Changelog excerpt included"
    else
        rm -f "$STAGE_DIR/CHANGELOG.txt"
    fi
fi

# Create zip archive
print_info "Creating release archive..."
cd "$DIST_DIR"
ZIP_NAME="frontier-cli-${VERSION_CLEAN}-macos.zip"
zip -r "$ZIP_NAME" "frontier-cli-${VERSION_CLEAN}-macos" > /dev/null
print_success "Created $ZIP_NAME"

# Create separate compressed database for optional download
print_info "Creating compressed database archive..."
gzip -c "$STAGE_DIR/Frontier.root7" > "$DIST_DIR/Frontier.root7.gz"
print_success "Created Frontier.root7.gz"

# Generate checksums
print_info "Generating checksums..."
cd "$DIST_DIR"
shasum -a 256 "$ZIP_NAME" > "${ZIP_NAME}.sha256"
shasum -a 256 "Frontier.root7.gz" > "Frontier.root7.gz.sha256"
print_success "Checksums generated"

# Print summary
echo ""
print_success "Release package created successfully!"
echo ""
echo "Distribution files:"
echo "  $DIST_DIR/$ZIP_NAME"
echo "  $DIST_DIR/$ZIP_NAME.sha256"
echo "  $DIST_DIR/Frontier.root7.gz"
echo "  $DIST_DIR/Frontier.root7.gz.sha256"
echo ""

# Get file sizes
ZIP_SIZE=$(du -h "$ZIP_NAME" | cut -f1)
DB_SIZE=$(du -h "Frontier.root7.gz" | cut -f1)

echo "Package size: $ZIP_SIZE"
echo "Database size: $DB_SIZE (compressed)"
echo ""
echo "Ready to upload to GitHub Release: $VERSION"
echo ""
