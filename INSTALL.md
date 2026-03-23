# Frontier CLI Installation Guide

Complete installation and setup guide for Frontier CLI.

## System Requirements

- **macOS**: 11.0 (Big Sur) or later
- **Architecture**: Apple Silicon (arm64) or Intel (x86_64)
- **Disk Space**: ~15MB for binary and system database

## Quick Installation

### Option 1: Pre-built Release (Recommended)

Download the latest release from GitHub:

```bash
# Download latest release
VERSION="v1.0.0-alpha.1"  # Replace with desired version
curl -L https://github.com/jsavin/Frontier/releases/download/$VERSION/frontier-cli-${VERSION#v}-macos.zip -o frontier-cli.zip

# Extract
unzip frontier-cli.zip
cd frontier-cli-${VERSION#v}-macos

# Run installer
./install.sh

# Verify installation
frontier-cli --version
```

The installer will:
1. Copy `frontier-cli` binary to `/usr/local/bin` (or `~/.local/bin` if sudo not available)
2. Copy system root database to `~/Library/Application Support/Frontier/`
3. Optionally add to your PATH (if using `~/.local/bin`)

### Option 2: Build from Source

```bash
# Clone repository
git clone https://github.com/jsavin/Frontier.git
cd Frontier

# Checkout desired version (or use develop branch)
git checkout v1.0.0-alpha.1

# Build CLI
cd frontier-cli
make

# Run directly
./frontier-cli --version

# Or install manually
sudo cp frontier-cli /usr/local/bin/
mkdir -p ~/Library/Application\ Support/Frontier
cp ../databases/Frontier.root ~/Library/Application\ Support/Frontier/Frontier.root
```

## Verification

After installation, verify everything works:

```bash
# Check version
frontier-cli --version

# Test inline execution
frontier-cli -e "1+1"

# Start interactive REPL
frontier-cli
```

Expected output:
```
Frontier CLI v1.0.0-alpha.1 (Jan 15 2026)
Copyright (C) 1992-2026 UserLand Software, Inc. and Contributors
This is free software; see the source for copying conditions.
```

## Installation Locations

After installation, files are located at:

```
/usr/local/bin/frontier-cli              # Binary (if installed with sudo)
# OR
~/.local/bin/frontier-cli                 # Binary (if installed without sudo)

~/Library/Application Support/Frontier/   # Data directory
├── Frontier.root                         # System root database (v7 format)
└── databases/                            # User databases (created as needed)
```

## Environment Variables

Frontier CLI recognizes the following environment variables:

### `FRONTIER_ROOT`

Override the system root database location:

```bash
export FRONTIER_ROOT="$HOME/custom/location/Frontier.root"
frontier-cli
```

### Logging Variables

Control log output (useful for debugging):

```bash
# Set log level (TRACE, DEBUG, INFO, WARN, ERROR)
export FRONTIER_LOG_LEVEL=DEBUG

# Filter by component (DB, HASH, LANG, etc.)
export FRONTIER_LOG_COMPONENT=DB

frontier-cli -e "db.new('/tmp/test.root')"
```

## System Root Search Paths

If no `--system-root` argument is provided, Frontier CLI searches for the system root database in this order:

1. `$FRONTIER_ROOT` environment variable
2. `./Frontier.root` (current working directory)
3. Executable directory (`<exe_dir>/Frontier.root`)
4. `~/Library/Application Support/Frontier/Frontier.root`
5. `~/.frontier/Frontier.root` (Linux convention)

## Uninstallation

To completely remove Frontier CLI:

```bash
# Remove binary
sudo rm /usr/local/bin/frontier-cli
# OR
rm ~/.local/bin/frontier-cli

# Remove data directory (WARNING: deletes all databases)
rm -rf ~/Library/Application\ Support/Frontier

# Remove PATH entry (if added to shell rc file)
# Edit ~/.zshrc or ~/.bashrc and remove the Frontier CLI PATH line
```

## Upgrading

To upgrade to a new version:

1. Download the new release
2. Run the installer again (it will replace the old binary)
3. Your data directory (`~/Library/Application Support/Frontier/`) is preserved

```bash
# Download new version
curl -L https://github.com/jsavin/Frontier/releases/download/v1.0.0-alpha.2/frontier-cli-1.0.0-alpha.2-macos.zip -o frontier-cli.zip
unzip frontier-cli.zip
cd frontier-cli-1.0.0-alpha.2-macos
./install.sh

# Verify upgrade
frontier-cli --version
```

## Troubleshooting

### "frontier-cli: command not found"

The installation directory is not in your PATH. Add it manually:

```bash
# For /usr/local/bin (should already be in PATH)
echo $PATH | grep -q '/usr/local/bin' || echo "Not in PATH"

# For ~/.local/bin
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc  # or ~/.bashrc
source ~/.zshrc
```

### "System root not found"

Frontier CLI can't find the system database. Check:

```bash
# Verify database exists
ls -lh ~/Library/Application\ Support/Frontier/Frontier.root

# If missing, reinstall or specify location
frontier-cli --system-root /path/to/Frontier.root
```

### "Permission denied" when running installer

The installer needs to copy files to `/usr/local/bin`. Either:

1. Run with sudo: `sudo ./install.sh`
2. Let it install to `~/.local/bin` (no sudo required)

### Database format errors

If you see database version errors, try migrating:

```bash
# Migrate v6 to v7 explicitly
frontier-cli --system-root ~/path/to/Frontier.root --upgrade-system-root
```

## Getting Help

- **Documentation**: [CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)
- **Issues**: [GitHub Issues](https://github.com/jsavin/Frontier/issues)
- **Discussions**: [GitHub Discussions](https://github.com/jsavin/Frontier/discussions)

## Next Steps

Once installed, see:
- [CLI Usage Guide](docs/CLI_USAGE_GUIDE.md) - Complete CLI reference
- [UserTalk Documentation](docs/usertalk/docserver/) - Language reference
- [Testing Guide](docs/TESTING_GUIDE.md) - Running tests and contributing
