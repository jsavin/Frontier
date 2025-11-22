# CMake generated Testfile for 
# Source directory: /Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Utilities/cmcurl
# Build directory: /Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Utilities/cmcurl
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[curl]=] "curltest" "http://open.cdash.org/user.php")
set_tests_properties([=[curl]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Utilities/cmcurl/CMakeLists.txt;1657;add_test;/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Utilities/cmcurl/CMakeLists.txt;0;")
subdirs("lib")
