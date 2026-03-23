#!/bin/bash
#
# Frontier CLI Installation Script
# Installs frontier-cli binary and system root database to standard locations
#

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}Error: $1${NC}" >&2
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}$1${NC}"
}

# Detect architecture
ARCH=$(uname -m)
print_info "Detected architecture: $ARCH"

# Check if frontier-cli binary exists
if [ ! -f "frontier-cli" ]; then
    print_error "frontier-cli binary not found in current directory"
    echo "Please run this script from the directory containing the frontier-cli binary"
    exit 1
fi

# Verify it's a valid binary
if ! file frontier-cli | grep -q "executable"; then
    print_error "frontier-cli is not a valid executable"
    exit 1
fi

# Check for database file
DATABASE=""
if [ -f "Frontier.root" ]; then
    DATABASE="Frontier.root"
else
    print_error "System root database not found (looking for Frontier.root)"
    exit 1
fi

print_info "Found system root database: $DATABASE"

# Determine installation directories
# Try /usr/local/bin first (requires sudo), fall back to ~/.local/bin
INSTALL_DIR=""
DATA_DIR="$HOME/Library/Application Support/Frontier"

# Check if we can write to /usr/local/bin
if [ -w "/usr/local/bin" ] || sudo -n true 2>/dev/null; then
    INSTALL_DIR="/usr/local/bin"
    USE_SUDO=true
else
    print_info "/usr/local/bin not writable without password, will install to ~/.local/bin"
    INSTALL_DIR="$HOME/.local/bin"
    USE_SUDO=false

    # Create ~/.local/bin if it doesn't exist
    mkdir -p "$INSTALL_DIR"
fi

print_info "Installation directory: $INSTALL_DIR"
print_info "Data directory: $DATA_DIR"

# Ask for confirmation
echo ""
echo "This will install:"
echo "  - frontier-cli binary to: $INSTALL_DIR/frontier-cli"
echo "  - System root database to: $DATA_DIR/$DATABASE"
if [ -d "Guest Databases" ]; then
    GUEST_COUNT=$(find "Guest Databases" -name "*.root" | wc -l | tr -d ' ')
    echo "  - Guest databases ($GUEST_COUNT) to: $DATA_DIR/Guest Databases/"
fi
echo ""

read -p "Continue with installation? [Y/n] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]] && [[ ! -z $REPLY ]]; then
    echo "Installation cancelled"
    exit 0
fi

# Create data directory
print_info "Creating data directory..."
mkdir -p "$DATA_DIR"

# Install binary
print_info "Installing frontier-cli binary..."
if [ "$USE_SUDO" = true ]; then
    sudo cp frontier-cli "$INSTALL_DIR/frontier-cli"
    sudo chmod 755 "$INSTALL_DIR/frontier-cli"
else
    cp frontier-cli "$INSTALL_DIR/frontier-cli"
    chmod 755 "$INSTALL_DIR/frontier-cli"
fi
print_success "Binary installed to $INSTALL_DIR/frontier-cli"

# Install database (only if it doesn't exist - preserve user data on upgrade)
if [ -f "$DATA_DIR/$DATABASE" ]; then
    print_info "System root database already exists at $DATA_DIR/$DATABASE"
    echo ""
    read -p "Overwrite existing database with fresh system root? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        cp "$DATABASE" "$DATA_DIR/$DATABASE"
        chmod 644 "$DATA_DIR/$DATABASE"
        print_success "Database replaced with fresh system root"
    else
        print_info "Keeping existing database (user data preserved)"
    fi
else
    print_info "Installing system root database..."
    cp "$DATABASE" "$DATA_DIR/$DATABASE"
    chmod 644 "$DATA_DIR/$DATABASE"
    print_success "Database installed to $DATA_DIR/$DATABASE"
fi

# Install guest databases
if [ -d "Guest Databases" ]; then
    GUEST_DIR="$DATA_DIR/Guest Databases"
    if [ -d "$GUEST_DIR" ]; then
        print_info "Guest databases directory already exists at $GUEST_DIR"
        echo ""
        read -p "Overwrite existing guest databases? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rsync -a --exclude='.DS_Store' "Guest Databases/" "$GUEST_DIR/"
            print_success "Guest databases replaced"
        else
            print_info "Keeping existing guest databases"
        fi
    else
        print_info "Installing guest databases..."
        mkdir -p "$GUEST_DIR"
        rsync -a --exclude='.DS_Store' "Guest Databases/" "$GUEST_DIR/"
        print_success "Guest databases installed to $GUEST_DIR"
    fi
fi

# Check if install directory is in PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    print_info ""
    print_info "Note: $INSTALL_DIR is not in your PATH"

    # Detect shell
    SHELL_RC=""
    if [ -n "$ZSH_VERSION" ]; then
        SHELL_RC="$HOME/.zshrc"
    elif [ -n "$BASH_VERSION" ]; then
        SHELL_RC="$HOME/.bashrc"
        if [ -f "$HOME/.bash_profile" ]; then
            SHELL_RC="$HOME/.bash_profile"
        fi
    fi

    if [ -n "$SHELL_RC" ] && [ "$USE_SUDO" = false ]; then
        echo ""
        read -p "Add $INSTALL_DIR to PATH in $SHELL_RC? [Y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
            echo "" >> "$SHELL_RC"
            echo "# Added by Frontier CLI installer" >> "$SHELL_RC"
            echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$SHELL_RC"
            print_success "Added to PATH in $SHELL_RC"
            print_info "Run: source $SHELL_RC (or restart your terminal)"
        fi
    fi
else
    print_success "$INSTALL_DIR is already in your PATH"
fi

# Verify installation
echo ""
print_info "Verifying installation..."
if command -v frontier-cli &> /dev/null; then
    VERSION=$(frontier-cli --version 2>&1 | head -1)
    print_success "Installation verified: $VERSION"
else
    print_info "frontier-cli command not yet available (you may need to restart your terminal)"
fi

# Print completion message
echo ""
print_success "Installation complete!"
echo ""
echo "Quick start:"
echo "  frontier-cli --version          # Show version"
echo "  frontier-cli                    # Start interactive REPL"
echo "  frontier-cli -e '1+1'          # Execute inline script"
echo ""
echo "Documentation:"
echo "  See INSTALL.md for detailed usage instructions"
echo "  System root: $DATA_DIR/$DATABASE"
echo ""
