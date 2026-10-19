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
# A configured-but-never-found interpreter is common: -DPython3_EXECUTABLE on
# the cache skips find_package above, leaving Python3_FOUND false and the
# whole tier-F1 corpus unregistered. Validate the path instead of the variable.
if(Python3_EXECUTABLE AND NOT Python3_FOUND)
  if(EXISTS "${Python3_EXECUTABLE}")
    set(Python3_FOUND TRUE)
  elseif(NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 QUIET COMPONENTS Interpreter)
  endif()
endif()

# Key the guard on the executable, not Python3_FOUND: when the interpreter
# path is passed explicitly (-DPython3_EXECUTABLE=...) the find_package call
# above is skipped, Python3_FOUND stays unset, and the tier silently
# registers zero tests (measured 2026-10-17, this host).

set(HUNMIN_TSF_REPO_ROOT "${CMAKE_SOURCE_DIR}")
if(NOT EXISTS "${HUNMIN_TSF_REPO_ROOT}/libhunmin/data")
  # Standalone configure (frontends/tsf project opened directly).
  get_filename_component(HUNMIN_TSF_REPO_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
endif()

configure_file(tsf-testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/tsf-testdir.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(tsf-trace-emit trace-emit.cpp ../src/config.cpp ../src/settings.cpp
                   ../src/candwindow.cpp)
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

if(Python3_EXECUTABLE)
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

# Issue #66 tier F1 additions. The config store + slot-resolution test and
# the module-dir data test include src/tsf.cpp directly (same white-box seam
# as tsf-trace-emit above) and resolve the engine the same way it did. The
# add_test calls are written out literally (no loop variable) because
# tools/docs-lint.py check 6 greps the sources for add_test(NAME <id>).
foreach(tsf_test_exe tsf-config-test tsf-datadir-test tsf-settings-test tsf-candidate-test)
  add_executable(${tsf_test_exe} ${tsf_test_exe}.cpp ../src/config.cpp
                   ../src/settings.cpp ../src/candwindow.cpp)
  target_compile_definitions(${tsf_test_exe} PRIVATE UNICODE _UNICODE)
  target_include_directories(${tsf_test_exe} PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}
      ${HUNMIN_TSF_REPO_ROOT}/libhunmin/include)
  target_link_libraries(${tsf_test_exe} PRIVATE
      ${HUNMIN_ENGINE_LIB}
      GTest::gtest
      ole32
      oleaut32
      user32
      gdi32
      advapi32
      imm32
      shlwapi)
endforeach()
# tsf-candidate-test has no skip probe of its own, so its main() comes from
# the framework: gtest_main, not a hand-written one (issue #109). The other
# three runners keep main() — the rc-77 skip contract needs a probe that
# exits before RUN_ALL_TESTS, which gtest_main cannot express.
target_link_libraries(tsf-candidate-test PRIVATE GTest::gtest_main)
# The settings-dialog TU cannot be #included by the harness (it owns its own
# COM classes + the Win32 dialog loop), it is linked: it shares tsf.cpp's
# hunmin_ctx_from_module / hunmin_tsf_apply_config_store through the test
# binary's tsf.cpp include and adds oleaut32 (SysAllocString) only here.
target_sources(tsf-settings-test PRIVATE ../src/settings.cpp)

# Literal registrations (docs-lint check 6 greps sources for add_test(NAME id)).
add_test(NAME tsf-config-test COMMAND tsf-config-test)
add_test(NAME tsf-datadir-test COMMAND tsf-datadir-test)
add_test(NAME tsf-settings-test COMMAND tsf-settings-test)
add_test(NAME tsf-candidate-test COMMAND tsf-candidate-test)
set_tests_properties(tsf-config-test tsf-datadir-test tsf-settings-test tsf-candidate-test PROPERTIES
  LABELS "tier-F1"
  SKIP_RETURN_CODE 77
  TIMEOUT 120)

# Stage the engine data beside every harness executable (issue #66): the
# datadir test exercises the "<module>\data" branch with the env cleared, so
# the staged copy IS its input; the trace runner and the config test read the
# repo tree via HUNMIN_TSF_DATA_DIR, and get the staged copy as their
# module-dir fallback anyway.
foreach(tsf_test_exe tsf-trace-emit tsf-config-test tsf-datadir-test tsf-settings-test tsf-candidate-test)
  add_custom_command(TARGET ${tsf_test_exe} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${HUNMIN_TSF_REPO_ROOT}/libhunmin/data"
            "$<TARGET_FILE_DIR:${tsf_test_exe}>/data"
    COMMENT "Staging libhunmin/data next to ${tsf_test_exe}")
endforeach()
