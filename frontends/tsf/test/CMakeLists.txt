# Tier F1: the TSF mock harness (issue #80). ONE test executable (tsf-test,
# issue #109: every runner white-boxes src/tsf.cpp, so N executables compiled
# the heavy TU N times -- measured 12.7 s per adapter touch, ~4 s merged) that
# drives the real adapter (frontends/tsf/src/tsf.cpp) over the mock TSF layer
# in mock-tsf.h and replays win32 platform traces (tests/conformance/win32)
# for tools/trace-check.py to diff against the expectation tier A implies --
# docs/TESTING.md tier F1.
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

add_executable(tsf-trace-emit trace-emit.cpp ../src/tsf.cpp ../src/ids.cpp
                   ../src/registration.cpp ../src/delivery.cpp
                   ../src/candidates.cpp ../src/displayattr.cpp
                   ../src/datadir.cpp ../src/langbar.cpp ../src/keybinding.cpp
                   ../src/config.cpp ../src/settings.cpp ../src/settingsrows.cpp
                   ../src/settingsdialog.cpp ../src/settingslaunch.cpp
                   ../src/settingsprovider.cpp ../src/candwindow.cpp)
target_compile_definitions(tsf-trace-emit PRIVATE UNICODE _UNICODE)
target_include_directories(tsf-trace-emit PRIVATE
    ${HUNMIN_TSF_REPO_ROOT}/libhunmin/include)
# The adapter is LINKED, not included: the component TUs (delivery,
# candidates, keybinding) compile once and the harness links them like the
# DLL does. The DLL target's own link settings (def file, SUFFIX) do not
# apply and must not.
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

# The four case groups ride ONE executable: each fragment is a standalone
# TU now (the include-.cpp white-box seam is retired), the shell's
# Snapshot() observation channel replaces #define-private-public, and the
# key-state seam is the registered provider (tsf-keystate-mock.h). Each
# registered entry selects its group with --gtest_filter, so the
# registered COUNT and the per-group labels are what they were. The rc-77
# skip probes run OUTSIDE the binary (tools/tsf-run-if-data.py): a gtest skip
# reports rc 0, which reads as a pass, so the contract is decided before
# gtest runs. add_test calls are written out literally (no loop variable)
# because tools/docs-lint.py check 6 greps the sources for add_test(NAME id).
add_executable(tsf-test tsf-test.cpp
    tsf-config-test.cpp tsf-settings-test.cpp tsf-candidate-test.cpp
    tsf-langbaritem-test.cpp tsf-datadir-test.cpp tsf-legacy-test.cpp
    ../src/tsf.cpp ../src/ids.cpp ../src/registration.cpp
    ../src/delivery.cpp ../src/candidates.cpp
    ../src/langbar.cpp ../src/displayattr.cpp ../src/datadir.cpp
    ../src/keybinding.cpp ../src/config.cpp
    ../src/settings.cpp ../src/settingsrows.cpp ../src/settingsdialog.cpp
    ../src/settingslaunch.cpp ../src/settingsprovider.cpp
    ../src/candwindow.cpp)
target_compile_definitions(tsf-test PRIVATE UNICODE _UNICODE)
target_include_directories(tsf-test PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${HUNMIN_TSF_REPO_ROOT}/libhunmin/include)
target_link_libraries(tsf-test PRIVATE
    ${HUNMIN_ENGINE_LIB}
    GTest::gtest_main
    ole32
    oleaut32
    user32
    gdi32
    advapi32
    imm32
    shlwapi)

set(HUNMIN_TSF_PROBE ${HUNMIN_TSF_REPO_ROOT}/tools/tsf-run-if-data.py)
# ONE registration for the one executable (maintainer ruling, issue #109):
# the wrapper probes BOTH data conditions (the repo tree AND the staged
# copy beside the executable) and exits 77 with the reason when either is
# absent; all four case groups run in that one process. The registered
# count is 4 entries lower than the four-entry shape (59 -> 56 on the MSVC
# tree) -- the maintainer amended the never-shrinks gate for this merge.
# Literal registration (docs-lint check 6 greps sources for add_test(NAME id)).
add_test(NAME tsf-test
  COMMAND ${Python3_EXECUTABLE} ${HUNMIN_TSF_PROBE}
          --data ${HUNMIN_TSF_REPO_ROOT}/libhunmin/data --staged
          $<TARGET_FILE:tsf-test>)
set_tests_properties(tsf-test PROPERTIES
  LABELS "tier-F1"
  SKIP_RETURN_CODE 77
  TIMEOUT 120)

# Stage the engine data beside both harness executables (issue #66): the
# datadir entry exercises the "<module>\data" branch with the env cleared, so
# the staged copy IS its input; the trace runner and the other entries read
# the repo tree via HUNMIN_TSF_DATA_DIR, and get the staged copy as their
# module-dir fallback anyway.
foreach(tsf_test_exe tsf-trace-emit tsf-test)
  add_custom_command(TARGET ${tsf_test_exe} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${HUNMIN_TSF_REPO_ROOT}/libhunmin/data"
            "$<TARGET_FILE_DIR:${tsf_test_exe}>/data"
    COMMENT "Staging libhunmin/data next to ${tsf_test_exe}")
endforeach()
