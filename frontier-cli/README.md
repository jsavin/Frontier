# Frontier CLI

Command Line Interface for UserTalk Script Execution

## Overview

Frontier CLI is a command-line tool that enables execution of UserTalk scripts without requiring the full Frontier GUI environment. It provides a headless interface for running UserTalk scripts, performing database operations, and serving as a network server for remote execution.

## Features

- **Script Execution**: Execute UserTalk scripts from files or inline code
- **Database Operations**: Query and manipulate Frontier databases
- **Database Migration**: Migrate databases to 64-bit format
- **Network Server**: HTTP and WebSocket servers for remote execution
- **Universal Binary**: Support for both x86_64 and ARM64 architectures
- **Comprehensive Logging**: Verbose and debug output modes
- **Error Handling**: Robust error reporting and recovery

## Building

### Prerequisites

- macOS 10.15 or later
- Xcode Command Line Tools
- clang compiler

### Compilation

```bash
# Build the CLI executable
make

# Build with debug symbols
make debug

# Build with profiling
make profile

# Build with static analysis
make analyze

# Show available targets
make help
```

### Installation

```bash
# Install to /usr/local/bin
make install

# Uninstall
make uninstall
```

## Usage

### Basic Script Execution

```bash
# Execute a UserTalk script file
./frontier-cli script.usertalk

# Execute inline script
./frontier-cli -e "local(x = 5); x * 2"

# Execute with verbose output
./frontier-cli -v -e "local(x = 5); x * 2"

# Execute with debug output
./frontier-cli --debug -e "local(x = 5); x * 2"
```

### Database Operations

```bash
# Query database value
./frontier-cli -d test.root -q "db.getValue('myTable.myValue')"

# Migrate database to 64-bit format
./frontier-cli -d test.root -m migrate

# Create new database
./frontier-cli -d newdb.root --create
```

### Network Server

```bash
# Start HTTP server
./frontier-cli --server --port 8080

# Start WebSocket server
./frontier-cli --websocket --port 8081

# Start with custom port
./frontier-cli --server -p 9000
```

### Help and Version

```bash
# Show help
./frontier-cli --help

# Show version
./frontier-cli --version
```

## Examples

### Simple Arithmetic

```bash
./frontier-cli -e "local(x = 10, y = 20); x + y"
# Output: 30
```

### String Operations

```bash
./frontier-cli -e "local(name = 'World'); 'Hello, ' & name & '!'"
# Output: Hello, World!
```

### Conditional Logic

```bash
./frontier-cli -e "local(x = 15); if (x > 10) then 'Large' else 'Small' end if"
# Output: Large
```

### Database Query

```bash
./frontier-cli -d mydb.root -q "db.getValue('config.version')"
# Output: 1.0.0
```

## API Reference

### Command Line Options

| Option | Description |
|--------|-------------|
| `-e, --execute SCRIPT` | Execute inline UserTalk script |
| `-d, --database FILE` | Specify database file for operations |
| `-q, --query QUERY` | Execute database query |
| `-m, --migrate` | Migrate database to 64-bit format |
| `--server` | Run as HTTP server |
| `--websocket` | Enable WebSocket support |
| `-p, --port PORT` | Network server port (default: 8080) |
| `-v, --verbose` | Verbose output |
| `--debug` | Debug mode |
| `-h, --help` | Show help message |
| `--version` | Show version information |

### HTTP API

When running as an HTTP server, the following endpoints are available:

#### GET /status
Returns server status information.

#### GET /execute?script=...
Execute a UserTalk script via query parameter.

#### POST /execute
Execute a UserTalk script via request body.

Example:
```bash
curl -X POST http://localhost:8080/execute \
  -H "Content-Type: text/plain" \
  -d "local(x = 5); x * 2"
```

Response:
```json
{
  "success": true,
  "data": "10"
}
```

### WebSocket API

Connect to the WebSocket server and send UserTalk scripts as text messages.

Example:
```javascript
const ws = new WebSocket('ws://localhost:8081');
ws.onmessage = function(event) {
    console.log('Result:', event.data);
};
ws.send('local(x = 5); x * 2');
```

## Architecture

### Core Components

- **CLI Parser** (`cli_parser.c`): Command-line argument parsing
- **CLI Executor** (`cli_executor.c`): UserTalk script compilation and execution
- **CLI Database** (`cli_database.c`): Database operations and migration
- **CLI Network** (`cli_network.c`): HTTP and WebSocket server functionality
- **CLI Utils** (`cli_utils.c`): Utility functions and logging

### Integration with Frontier Runtime

The CLI integrates with the existing Frontier runtime components:

- **Language System**: Uses `lang.c` and `langevaluate.c` for script execution
- **Database System**: Uses `db.c` for database operations
- **Memory Management**: Uses Frontier's memory management system
- **Error Handling**: Integrates with Frontier's error reporting

## Development

### Project Structure

```
frontier-cli/
├── main.c                 # CLI entry point
├── cli_parser.c          # Command-line argument parsing
├── cli_executor.c        # UserTalk script execution
├── cli_database.c        # Database operations
├── cli_network.c         # HTTP/WebSocket server
├── cli_utils.c           # Utility functions
├── cli_*.h              # Header files
├── Makefile              # Build configuration
├── test_script.usertalk  # Test script
└── README.md             # This file
```

### Building for Development

```bash
# Build with debug symbols
make debug

# Run with verbose output
./frontier-cli -v --debug -e "local(x = 5); x * 2"

# Test with sample script
./frontier-cli test_script.usertalk
```

### Testing

```bash
# Test basic functionality
./frontier-cli -e "local(x = 1, y = 2); x + y"

# Test database operations
./frontier-cli -d test.root -q "db.getValue('test')"

# Test network server
./frontier-cli --server --port 8080 &
curl http://localhost:8080/status
kill %1
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure Xcode Command Line Tools are installed
2. **Runtime Errors**: Check that Frontier runtime components are available
3. **Database Errors**: Verify database file permissions and format
4. **Network Errors**: Check port availability and firewall settings

### Debug Mode

Enable debug mode for detailed logging:

```bash
./frontier-cli --debug -v -e "local(x = 5); x * 2"
```

### Error Messages

The CLI provides detailed error messages for:
- Invalid command-line arguments
- Script compilation errors
- Database operation failures
- Network server issues

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is part of the Frontier refactoring effort and follows the same license as the main Frontier project.

## Phase 1 Status

✅ **Completed**:
- CLI application skeleton
- Command-line argument parsing
- UserTalk script execution
- Database operations interface
- Network server framework
- Comprehensive logging system
- Universal binary compilation
- Documentation and examples

🔄 **In Progress**:
- Integration testing with real UserTalk scripts
- Performance optimization
- Network server implementation details

📋 **Planned**:
- Advanced database operations
- WebSocket server implementation
- Security features
- Performance benchmarking

## Next Steps

Phase 1 establishes the foundation for CLI-based UserTalk invocation. The next phase will focus on:

1. **Hash Table Modernization**: Updating Frontier's core data structures
2. **Performance Optimization**: Improving execution speed and memory usage
3. **Advanced Features**: Enhanced database operations and network protocols
4. **Integration Testing**: Comprehensive testing with real-world scenarios

For more information about the Frontier refactoring project, see the main project documentation.
