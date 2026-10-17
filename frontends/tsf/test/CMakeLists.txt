# Tier F1: the TSF mock harness (issue #80). A ctest binary that drives the
# real adapter (frontends/tsf/src/tsf.cpp, included as-is by the harness)
# over the mock TSF layer in mock-tsf.h and replays win32 platform traces
# (tests/conformance/win32) for tools/trace-check.py to diff against the
# expectation tier A implies -- docs/TESTING.md tier F1.
#
# Build constraints: MSVC only (msctf.h, the DLL's own compile flags), with
# the engine resolved the same way the TIP above resolved it. On a Windows
# host without MSVC, or without the engine built, nothing is registered
# here rather than something red; the runner itself reports skip (rc 77)
# when the engine data cannot be found at run time.

if(NOT Python3_EXECUTABLE)
  find_package(Python3 QUIET COMPONENTS Interpreter)
endif()

set(HUNMIN_TSF_REPO_ROOT "${CMAKE_SOURCE_DIR}")
if(NOT EXISTS "${HUNMIN_TSF_REPO_ROOT}/libhunmin/data")
  # Standalone configure (frontends/tsf project opened directly).
  get_filename_component(HUNMIN_TSF_REPO_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
endif()

configure_file(tsf-testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/tsf-testdir.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(tsf-trace-emit trace-emit.cpp)
target_compile_definitions(tsf-trace-emit PRIVATE UNICODE _UNICODE)
target_include_directories(tsf-trace-emit PRIVATE
    ${HUNMIN_TSF_REPO_ROOT}/libhunmin/include)
# The harness includes src/tsf.cpp directly; the DLL target's own link
# settings (def file, SUFFIX) do not apply and must not.
target_link_libraries(tsf-trace-emit PRIVATE
    ${HUNMIN_ENGINE_LIB}
    ole32
    user32
    gdi32
    advapi32
    shlwapi)

if(Python3_FOUND)
  # One test per trace file, the names derived from the filename exactly as
  # tier C does it (docs/TESTING.md "Why labels rather than a -R regex").
  file(GLOB HUNMIN_TSF_CONFORMANCE_TRACES
       "${HUNMIN_TSF_REPO_ROOT}/tests/conformance/win32/*.trace.in")
  foreach(t IN LISTS HUNMIN_TSF_CONFORMANCE_TRACES)
    file(RELATIVE_PATH rel "${HUNMIN_TSF_REPO_ROOT}" "${t}")
    string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" name "${rel}")
    add_test(NAME tsf-mock-${name}
      COMMAND ${Python3_EXECUTABLE}
              ${HUNMIN_TSF_REPO_ROOT}/tools/trace-check.py "${t}"
              --runner $<TARGET_FILE:tsf-trace-emit>)
    set_tests_properties(tsf-mock-${name} PROPERTIES
      LABELS "tier-F1"
      SKIP_RETURN_CODE 77
      TIMEOUT 120)
  endforeach()
endif()
