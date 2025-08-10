cmake_minimum_required(VERSION 3.20)
project(Frontier VERSION 1.0.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable 64-bit compilation
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

# Set minimum deployment target for macOS
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add compiler flags for 64-bit safety
if(APPLE)
    add_compile_options(
        -fPIC
        -Wall
        -Wextra
        -Werror=implicit-function-declaration
        -Werror=return-type
        -Wno-deprecated-declarations
    )
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/../Common/headers
    ${CMAKE_SOURCE_DIR}/../Common/SystemHeaders
    ${CMAKE_SOURCE_DIR}/../Common/Paige/headers
    ${CMAKE_SOURCE_DIR}/../Common/MySQL/osx10.4-i686/include
    ${CMAKE_SOURCE_DIR}/../Common/PCRE
)

# Add source files
file(GLOB_RECURSE FRONTIER_SOURCES 
    "${CMAKE_SOURCE_DIR}/../Common/source/*.c"
    "${CMAKE_SOURCE_DIR}/../Common/source/*.m"
)

# Create executable
add_executable(Frontier ${FRONTIER_SOURCES})

# Link libraries
target_link_libraries(Frontier
    "-framework Foundation"
    "-framework AppKit"
    "-framework Carbon"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreText"
    "-framework Security"
    "-framework SystemConfiguration"
)

# Set target properties
set_target_properties(Frontier PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "Frontier"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.userland.Frontier"
)

# Create Info.plist
configure_file(
    "${CMAKE_SOURCE_DIR}/Info.plist.in"
    "${CMAKE_BINARY_DIR}/Info.plist"
    @ONLY
)
