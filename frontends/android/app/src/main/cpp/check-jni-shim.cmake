# Usage: cmake -DNM=<llvm-nm> -DSHIM=<libhunjni.so> -P check-jni-shim.cmake
#
# Asserts the JNI shim really is the shim. A shared library that links the
# engine and exports no Java_org_* symbol is a JNI layer that will throw
# UnsatisfiedLinkError on the device, and nothing else in the build notices:
# it compiles, it links, it packages. Checking here turns that into a POST_BUILD
# error on the build machine.
#
# Written in script mode rather than `sh -c | grep` so the check runs on every
# host: AGP drives POST_BUILD through cmd.exe on Windows, which cannot parse
# that shell pipeline (measured: "'{' is not recognized...", the build dies
# even though the link itself succeeded).
foreach(v IN ITEMS NM SHIM)
  if(NOT ${v})
    message(FATAL_ERROR "usage: cmake -DNM=<llvm-nm> -DSHIM=<path.so> -P check-jni-shim.cmake")
  endif()
endforeach()
execute_process(
  COMMAND "${NM}" -D --defined-only "${SHIM}"
  OUTPUT_VARIABLE _hunmin_syms
  RESULT_VARIABLE _hunmin_rc
  ERROR_QUIET)
if(NOT _hunmin_rc EQUAL 0 OR NOT _hunmin_syms MATCHES "Java_org_")
  message(FATAL_ERROR
    "FATAL: ${SHIM} exports no Java_org_* symbol; the JNI shim was not built"
    " (is the output name colliding with the engine?)")
endif()
