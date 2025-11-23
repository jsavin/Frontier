if(NOT EXISTS "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/install_manifest.txt")
  message(FATAL_ERROR "Cannot find install manifest: \"/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/install_manifest.txt\"")
endif()

file(READ "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/install_manifest.txt" files)
string(REPLACE "\n" ";" files "${files}")
foreach(file ${files})
  message(STATUS "Uninstalling \"$ENV{DESTDIR}${file}\"")
  if(EXISTS "$ENV{DESTDIR}${file}")
    execute_process(
      COMMAND "/Users/jake/dev/jsavin/Frontier/third_party/cmake-src/Bootstrap.cmk/cmake" -E rm -f "$ENV{DESTDIR}${file}"
      OUTPUT_VARIABLE rm_out
      RESULT_VARIABLE rm_retval
      )
    if("${rm_retval}" STREQUAL 0)
    else()
      message(FATAL_ERROR "Problem when removing \"$ENV{DESTDIR}${file}\"")
    endif()
  else()
    message(STATUS "File \"$ENV{DESTDIR}${file}\" does not exist.")
  endif()
endforeach()
