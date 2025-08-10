# Frontier Refactoring Plan

## Project Overview

Frontier is a complex C/C++ application with:
- **Core Components**: Object database engine, scripting runtime, networking, file I/O
- **UI Layer**: Carbon-based window management, menus, buttons, drawing
- **Database**: Custom object database + SQLite/MySQL integration
- **Platform Support**: macOS (Carbon/Cocoa) and Windows (Win32)
- **Build Systems**: Multiple legacy build configurations (CodeWarrior, Visual Studio, Xcode)

## Critical Issues Identified

### 1. **Database Compatibility Crisis**
- **Legacy .odb format** uses custom binary format with 32-bit assumptions
- **Handle-based memory management** throughout database engine
- **Version-specific file formats** with complex migration paths
- **Critical**: Database files contain user data that must be preserved

### 2. **Memory Management Dependencies**
- **Extensive use of Handle types** (Mac OS memory handles)
- **Pointer arithmetic** assumes 32-bit addresses
- **Memory layout** depends on platform-specific alignment
- **Critical**: Must maintain handle semantics across architectures

### 3. **UI Framework Dependencies**
- **Paige text rendering** is deeply integrated and platform-specific
- **Carbon UI** components are tightly coupled to core logic
- **QuickDraw dependencies** throughout the codebase
- **Critical**: UI replacement requires careful separation

### 4. **Platform Permissions & Security**
- **File system access** patterns may violate modern security models
- **Network permissions** need updating for modern OS requirements
- **Sandboxing considerations** for macOS and Windows
- **Critical**: Must work with modern security frameworks

### 5. **Hash Table Performance Problem**
- **Current**: 13 buckets with simple hash function using first/last characters
- **Problem**: Numeric names (like "001", "002") all hash to same buckets
- **Impact**: Severe performance degradation with large tables
- **Solution**: Implement better hash algorithm and dynamic bucket sizing

### 6. **WPText Size Limitation**
- **Current**: 32KB limit inherited from Classic Mac OS
- **Problem**: Insufficient for modern rich text documents
- **Impact**: Breaks large document workflows
- **Solution**: New rich text type with modern size limits

### 7. **Dot Addressing & Scope Management**
- **Current**: Complex scope resolution across multiple .odb files
- **Problem**: Global scope management with multiple databases
- **Impact**: Critical for UserTalk script compatibility
- **Solution**: Maintain backward compatibility while modernizing

## Phase 0: 64-bit & ARM Architecture Foundation (Weeks 1-3)

### 0.1 **Build System Modernization**
- **Update Xcode project** for ARM64 and x86_64 targets
- **Remove legacy build systems** (CodeWarrior, old Visual Studio)
- **Configure modern compiler flags** for 64-bit compatibility
- **Set up cross-compilation** for both architectures
- **Create build automation** scripts for CI/CD

### 0.2 **64-bit Data Type Audit**
- **Audit all integer types** (int, long, size_t, ptrdiff_t)
- **Fix pointer-to-integer conversions** and vice versa
- **Update structure packing** and alignment assumptions
- **Review all bitwise operations** for 64-bit safety
- **Fix endianness assumptions** where necessary

### 0.3 **ARM Architecture Preparation**
- **Audit assembly code** for x86-specific instructions
- **Update memory alignment** for ARM requirements
- **Review SIMD/vector operations** for ARM compatibility
- **Test pointer arithmetic** on ARM architecture
- **Verify atomic operations** compatibility

### 0.4 **Compiler Compatibility**
- **Test with modern Clang/LLVM** toolchain
- **Verify GCC compatibility** for Linux future support
- **Update preprocessor directives** for new architectures
- **Fix compiler warnings** and deprecation notices
- **Ensure C++ standard compliance** where applicable

### 0.5 **Basic Compilation Success**
- **Achieve clean compilation** on ARM64 macOS
- **Achieve clean compilation** on x86_64 macOS
- **Create minimal test builds** to verify basic functionality
- **Document compilation requirements** and dependencies
- **Set up automated build verification** for both architectures

## Phase 1: Foundation & Database Migration (Weeks 4-7)

### 1.1 **Immediate Database Migration Priority**
- **Create .odb2 format** with 64-bit compatibility
- **Implement dual-read capability** (.odb + .odb2)
- **Design new database schema** with improved hash tables
- **Add database validation** and integrity checking tools

### 1.2 **Hash Table Modernization**
- **Replace 13-bucket hash** with dynamic bucket sizing
- **Implement better hash algorithm** (FNV-1a or similar)
- **Add hash collision resolution** strategies
- **Create hash performance monitoring** tools

### 1.3 **Memory Management Modernization**
- **Create handle abstraction layer** for cross-platform compatibility
- **Implement 64-bit safe** memory management
- **Add memory leak detection** and debugging tools
- **Create memory pool management** for better performance

### 1.4 **Command Line Interface Development**
- **Create CLI version** of Frontier core engine
- **Implement script execution** from command line
- **Add database operations** via CLI (read, write, query)
- **Enable headless operation** for automation and testing

### 1.5 **Unit Testing Infrastructure**
- **Set up comprehensive test suite** for database operations
- **Create test databases** with known content
- **Implement automated testing** for all core functions
- **Add performance benchmarking** tools

## Phase 2: Core Engine Separation (Weeks 8-11)

### 2.1 **Database Engine Isolation**
- **Extract database engine** into standalone library
- **Create database API** for external applications
- **Implement database plugins** for different storage backends
- **Add database encryption** and security features

### 2.2 **Scripting Engine Separation**
- **Isolate scripting runtime** from UI dependencies
- **Create script execution API** for external use
- **Implement script debugging** interface
- **Add script performance profiling**

### 2.3 **Networking & I/O Modernization**
- **Update networking code** for modern security requirements
- **Implement HTTPS support** with certificate handling
- **Add async I/O** for better performance
- **Create network abstraction** layer

### 2.4 **Third-Party Library Updates**
- **SQLite**: Update to latest version with ARM support
- **MySQL**: Update client libraries for modern architectures
- **PCRE**: Update regex library for 64-bit compatibility
- **Paige**: Update text rendering library for ARM

## Phase 3: Rich Text & UI Modernization (Weeks 15-22)

### 3.1 **WPText Replacement**
- **Design new rich text type** (WPText2) without size limits
- **Implement modern rich text** storage format
- **Add rich text conversion** utilities
- **Create rich text API** for external applications

### 3.2 **Paige Text Engine Replacement**
- **Evaluate modern text rendering** libraries (Core Text, DirectWrite)
- **Create text rendering abstraction** layer
- **Implement text editing** with modern APIs
- **Add rich text support** with modern standards

### 3.3 **Platform-Specific UI Implementation**
- **macOS**: Replace Carbon with native Cocoa/AppKit
- **Windows**: Update to modern Windows UI frameworks
- **Linux**: Design for GTK/Qt compatibility
- **Create UI abstraction** layer for cross-platform support

### 3.4 **Modern UI Features**
- **Implement responsive design** for different screen sizes
- **Add accessibility support** (VoiceOver, screen readers)
- **Create modern UI components** (tables, outlines, editors)
- **Add theming and customization** support

## Phase 4: Architecture Modernization (Weeks 23-28)

### 4.1 **64-bit and ARM Support**
- **Update all data structures** for 64-bit compatibility
- **Implement ARM-specific optimizations**
- **Add universal binary support** for macOS
- **Create architecture abstraction** layer

### 4.2 **Modern API Integration**
- **Replace deprecated APIs** with modern equivalents
- **Implement modern file system** access patterns
- **Add modern security** features and permissions
- **Create plugin architecture** for extensibility

### 4.3 **Performance Optimization**
- **Implement multi-threading** for database operations
- **Add caching layers** for better performance
- **Optimize memory usage** for large databases
- **Add performance monitoring** tools

### 4.4 **Web UI Preparation**
- **Design REST API** for web frontend
- **Create JSON serialization** for data exchange
- **Implement WebSocket** support for real-time updates
- **Add web UI unit tests**

## Phase 5: Testing & Quality Assurance (Weeks 29-32)

### 5.1 **Comprehensive Testing Strategy**
- **Unit tests for all core functions** (database, scripting, networking)
- **Integration tests** for component interactions
- **Performance tests** for database operations
- **Cross-platform compatibility** tests

### 5.2 **Migration Testing**
- **Test database migration** with real user data
- **Validate data integrity** after migration
- **Performance comparison** with legacy version
- **Backward compatibility** testing

### 5.3 **Security & Permissions Testing**
- **Test modern security** requirements
- **Validate sandboxing** compliance
- **Test network permissions** and HTTPS
- **Security audit** of all components

## Phase 6: Deployment & Integration (Weeks 33-36)

### 6.1 **Modern Build System**
- **Complete CMake configuration** for all platforms
- **Add Linux build support** for future expansion
- **Create automated packaging** for all platforms
- **Implement code signing** and distribution

### 6.2 **Documentation & Migration Tools**
- **Create comprehensive documentation** for new APIs
- **Build migration tools** for user data
- **Create user guides** for new features
- **Add developer documentation** for extensibility

## Success Metrics

- **Compiles and runs** on modern macOS (ARM64 + x86_64)
- **Compiles and runs** on modern Windows (x64)
- **Unit test coverage** > 80%
- **Performance parity** with legacy version
- **Web UI prototype** functional

## Key Implementation Strategies

### **Database Migration Strategy**
1. **Start with CLI tool** for database operations
2. **Implement dual-format support** during transition
3. **Create migration validation** tools
4. **Provide rollback capability** for safety

### **Hash Table Improvement Strategy**
1. **Implement new hash algorithm** (FNV-1a or similar)
2. **Add dynamic bucket sizing** based on table size
3. **Create hash collision resolution** strategies
4. **Add hash performance monitoring** tools

### **Rich Text Modernization Strategy**
1. **Design WPText2 format** without size limits
2. **Implement conversion utilities** from WPText
3. **Create rich text API** for external applications
4. **Add rich text validation** tools

### **Dot Addressing Compatibility Strategy**
1. **Maintain backward compatibility** with existing scripts
2. **Implement scope resolution** for multiple databases
3. **Create dot addressing API** for external use
4. **Add scope debugging** tools

### **Memory Management Strategy**
1. **Create handle abstraction layer** for cross-platform compatibility
2. **Implement 64-bit safe** memory management
3. **Add memory leak detection** and debugging tools
4. **Create memory pool management** for better performance

### **Testing Strategy**
1. **Test-driven development** for all new components
2. **Continuous integration** with automated testing
3. **Performance regression** testing
4. **Cross-platform compatibility** validation

### **Risk Mitigation**
1. **Incremental migration** with fallback options
2. **Extensive testing** with real user data
3. **Performance monitoring** throughout development
4. **User feedback** integration during development

### **Linux Support Preparation**
1. **Use cross-platform libraries** where possible
2. **Create platform abstraction** layers
3. **Design for POSIX compatibility**
4. **Plan for package management** integration

## Build System Priority

1. **Start with Xcode project** modernization (most current)
2. **Create CMake build** system as primary build method
3. **Maintain VS Code** compatibility for cross-platform development
4. **Remove legacy build** systems (CodeWarrior, old Visual Studio)

## Testing Strategy

1. **Unit tests first** - write tests before refactoring
2. **Integration tests** - test component interactions
3. **Performance tests** - ensure no regression
4. **Cross-platform tests** - validate all platforms

## Risk Mitigation

1. **Incremental refactoring** - small, testable changes
2. **Feature flags** - enable/disable new implementations
3. **Backward compatibility** - maintain existing functionality
4. **Rollback strategy** - ability to revert changes

---

*This plan provides a structured approach to modernizing Frontier while maintaining its core functionality and enabling future development with modern tools and architectures.* 