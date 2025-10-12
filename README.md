# Frontier Refactoring Project

Status
- State: In Progress
- Phase: Multi-Phase Roadmap
- Last Updated: 2025-09-29
- Notes: See planning/INDEX.md for current phases and key docs.

Related Docs
- planning/INDEX.md
- planning/Frontier_Refactoring_Plan.md
- planning/DEVELOPER_QUICKSTART_HEADLESS.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Project Structure

This project has been reorganized for cross-platform compatibility and maintainability:

```
Frontier/                          # Project root
├── tests/                         # Cross-platform test framework
│   ├── framework/                 # Test framework core
│   ├── components/                # Component-specific tests
│   ├── examples/                  # Example tests
│   └── test_runner.c             # Main test runner
├── samples/                       # Sample files for testing
├── planning/                      # Documentation and planning
├── databases/                     # Database files
│   ├── Frontier.root             # System database
│   └── Guest Databases/          # User databases
│       ├── apps/
│       │   ├── Tools/
│       │   ├── mainResponder.root
│       │   └── manila.root
│       └── ops/
│           └── www/
├── app_resources/                 # Application resources
│   ├── Frontier/                 # Frontier app resources
│   │   ├── frontierStartupCommands.txt  # Startup commands
│   │   ├── Extras/              # Frontier-specific extras
│   │   └── [icons, plists, bundles]
│   ├── OPML/                     # OPML app resources
│   └── Radio/                    # Radio app resources
├── docs/                          # Documentation
│   ├── LICENSE.txt               # License file
│   ├── README.txt                # Original README
│   └── sdk/                      # SDK documentation
├── bin/                           # Application binaries (future)
├── build_Xcode_modern/           # Xcode build artifacts
├── build_GNU/                    # GNU build artifacts
├── Common/                       # Source code
│   ├── headers/                  # Header files
│   └── source/                   # Source files
└── [other build dirs]
```

## Key Directories

### `tests/` - Cross-Platform Test Framework
- **Purpose**: Unit tests that work across all build targets
- **Structure**: Organized by component and functionality
- **Usage**: `make -C tests test` to run all tests
- **Status**: [Build Issues Documented](portable/BUILD_ISSUES.md)

### `portable/` - Cross-Platform Runtime Stubs
- **Purpose**: Platform-independent runtime implementations
- **Status**: [Build Issues Documented](portable/BUILD_ISSUES.md)
- **Structure**: 12 focused stub files replacing monolithic runtime_stubs.c

### `samples/` - Test Data
- **Purpose**: Sample files for compatibility testing
- **Content**: Legacy and modern .root files for validation

### `planning/` - Documentation
- **Purpose**: Refactoring plans and progress documentation
- **Format**: Phase-based naming (0.5.7_database_dbnew_testing_success.md)

### `databases/` - Database Files
- **Purpose**: Real Frontier database files for testing
- **Structure**: 
  - `Frontier.root` - System database (required for UserTalk development)
  - `Guest Databases/` - User databases (apps, tools, operations)
  - `apps/mainResponder.root` - Main responder application
  - `apps/manila.root` - Manila web application

### `app_resources/` - Application Resources
- **Purpose**: Application-specific resources and assets
- **Structure**:
  - `Frontier/` - Main Frontier application resources
    - `frontierStartupCommands.txt` - Startup commands
    - `Extras/` - Frontier-specific utilities
    - Icons, plists, and bundles
  - `OPML/` - OPML editor resources
  - `Radio/` - Radio application resources

### `docs/` - Documentation
- **Purpose**: All project documentation and SDK
- **Structure**:
  - `LICENSE.txt` - Project license
  - `README.txt` - Original project README
  - `Manila User's Guide.pdf` - Manila CMS documentation
  - `sdk/FrontierSDK/` - Complete SDK documentation

## Known Issues and Status

### Build and Testing Issues
- **Unit Test Build Issues**: [Documented in BUILD_ISSUES.md](portable/BUILD_ISSUES.md)
- **Runtime Stub Implementation**: Complete with 12 focused files
- **Current Blocking Issue**: `stringtoosttype` linking error affecting all test targets

### Progress Summary
- ✅ **Runtime Stub Implementation**: Successfully split monolithic 2029-line file into 12 focused files
- ✅ **Compilation Issues**: All resolved through systematic signature corrections
- ❌ **Linking Issues**: One remaining function linking problem preventing test execution

## Build Targets

### Xcode Modern Build
```bash
cd build_Xcode_modern
xcodebuild -project Frontier.xcodeproj -configuration Release
```

### Test Framework
```bash
cd tests
make test                    # Run all tests
make test_database          # Run database tests only
make test_architectures     # Test both ARM64 and x86_64
```

## Development Workflow

1. **Code Changes**: Modify files in `Common/`
2. **Testing**: Run tests from `tests/` directory
3. **Validation**: Use real .root files from `databases/`
4. **Documentation**: Update planning files in `planning/`

## Current Phase

**Phase 0.5.8**: Database Compatibility Verification
- Testing against real .root files
- Validating structure compatibility
- Ensuring forward/backward compatibility

## Next Steps

1. Add sample .root files to `databases/`
2. Create comprehensive compatibility tests
3. Validate database operations across architectures
4. Document migration patterns

## Compatibility Goals

- **Forward Compatibility**: New code works with old .root files
- **Backward Compatibility**: Old code works with new .root files
- **Cross-Platform**: Tests pass on ARM64 and x86_64
- **Long-term Maintainability**: Clean, documented code structure
