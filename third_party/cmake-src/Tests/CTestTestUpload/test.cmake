cmake_minimum_required(VERSION 3.5)

# Settings:
set(CTEST_DASHBOARD_ROOT                "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CTestTest")
set(CTEST_SITE                          "M1.local")
set(CTEST_BUILD_NAME                    "CTestTest-Darwin-clang++-Upload")

set(CTEST_SOURCE_DIRECTORY              "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CTestTestUpload")
set(CTEST_BINARY_DIRECTORY              "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CTestTestUpload")
set(CTEST_CMAKE_GENERATOR               "Unix Makefiles")
set(CTEST_CMAKE_GENERATOR_PLATFORM      "")
set(CTEST_CMAKE_GENERATOR_TOOLSET       "")
set(CTEST_BUILD_CONFIGURATION           "$ENV{CMAKE_CONFIG_TYPE}")

CTEST_START(Experimental)
CTEST_CONFIGURE(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_BUILD(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_UPLOAD(FILES "${CTEST_SOURCE_DIRECTORY}/sleep.c" "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt")
CTEST_SUBMIT()
