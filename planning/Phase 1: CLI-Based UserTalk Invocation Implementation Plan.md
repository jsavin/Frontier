## Future Considerations

### WPText Object Modernization
**Critical Legacy Component** - WPText objects represent one of the most challenging legacy components:

#### Current State:
- **Rich Text Objects**: Similar to TextEdit documents in Frontier GUI
- **32KB Size Limit**: Legacy Mac Toolbox format restriction
- **8-bit ASCII Only**: No Unicode support, severely limiting internationalization
- **Platform-Specific**: Mac Toolbox binary format not portable

#### Modernization Goals:
1. **Unicode Support**: Convert to UTF-8/UTF-16 for international character support
2. **Size Limit Removal**: Eliminate 32KB restriction for modern text handling
3. **Cross-Platform Format**: Replace Mac Toolbox format with portable representation
4. **CLI Integration**: Enable WPText manipulation via command-line tools
5. **Migration Tools**: Provide utilities for converting legacy WPText objects

#### Implementation Phases:
- **Phase 1**: Ensure current 32-bit to 64-bit migration preserves WPText data
- **Phase 2**: Design new Unicode-based rich text format specification
- **Phase 3**: Implement format conversion utilities and validation
- **Phase 4**: Update database schema to support new WPText format
- **Phase 5**: Add CLI tools for WPText conversion, validation, and manipulation

#### Technical Challenges:
- **Data Integrity**: Ensuring no data loss during format conversion
- **Performance**: Handling large Unicode text objects efficiently
- **Backward Compatibility**: Maintaining access to legacy WPText objects
- **Cross-Platform**: Ensuring new format works across all target platforms

This modernization will be essential for making Frontier truly cross-platform and supporting modern international text requirements.
