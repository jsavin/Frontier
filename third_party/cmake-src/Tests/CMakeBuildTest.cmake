# create the binary directory
make_directory("/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly")

# remove the CMakeCache.txt file from the source dir
# if there is one, so that in-source cmake tests
# still pass
message("Remove: /Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/COnly/CMakeCache.txt")
file(REMOVE "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/COnly/CMakeCache.txt")

# run cmake in the binary directory
message("running: ${CMAKE_COMMAND}")
execute_process(COMMAND "${CMAKE_COMMAND}"
  "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/COnly"
  "-GUnix Makefiles"
  -A ""
  -T ""
  WORKING_DIRECTORY "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly"
  RESULT_VARIABLE RESULT)
if(RESULT)
  message(FATAL_ERROR "Error running cmake command")
endif()

# Now use the --build option to build the project
message("running: ${CMAKE_COMMAND} --build")
execute_process(COMMAND "${CMAKE_COMMAND}"
  --build "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly" --config Debug
  RESULT_VARIABLE RESULT)
if(RESULT)
  message(FATAL_ERROR "Error running cmake --build")
endif()

# run the executable out of the Debug directory if using a
# multi-config generator
set(_isMultiConfig 0)
if(_isMultiConfig)
  set(RUN_TEST "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly/Debug/COnly")
else()
  set(RUN_TEST "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly/COnly")
endif()
# run the test results
message("running [${RUN_TEST}]")
execute_process(COMMAND "${RUN_TEST}" RESULT_VARIABLE RESULT)
if(RESULT)
  message(FATAL_ERROR "Error running test COnly")
endif()

# build it again with clean and only COnly target
execute_process(COMMAND "${CMAKE_COMMAND}"
  --build "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Tests/CMakeBuildCOnly" --config Debug
  --clean-first --target COnly
  RESULT_VARIABLE RESULT)
if(RESULT)
  message(FATAL_ERROR "Error running cmake --build")
endif()

# run it again after clean
execute_process(COMMAND "${RUN_TEST}" RESULT_VARIABLE RESULT)
if(RESULT)
  message(FATAL_ERROR "Error running test COnly after clean ")
endif()
