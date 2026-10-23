cmake_minimum_required(VERSION 3.20)
project(hunmin-tsf LANGUAGES C CXX)

# enable_testing() for the tier-F1 mock harness below: without it add_test
# registers nothing and `ctest --test-dir frontends/tsf/build-msvc` reports
# "No tests were found" on a standalone configure (measured).
include(CTest)

# The logging singleton's inline handles need C++17 (log.h).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources are UTF-8 (comments carry Korean and em dashes). MSVC decodes in the
# active code page by default and emits C4819 for every TU that includes such a
# header; /utf-8 sets source AND execution charset. The top-level engine build
# already passes the same flag (CMakeLists.txt), so this keeps the standalone
# TSF configure equal to the packaged one.
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Same option as the top-level CMakeLists.txt (issue #99): this directory is
# also configured standalone, where the top-level definition never runs —
# the packaging build passes it here too, so the standalone TIP build links
# the same static CRT the packaged engine lib was built with.
option(HUNMIN_MSVC_STATIC_RUNTIME "Link the MSVC CRT statically (/MT); the packaged TIP shape (issue #99)" OFF)
if(HUNMIN_MSVC_STATIC_RUNTIME AND MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(hunmin-tsf MODULE
    src/dllmain.cpp
    src/tsf.cpp
    src/ids.cpp
    src/registration.cpp
    src/delivery.cpp
    src/candidates.cpp
    src/displayattr.cpp
    src/langbar.cpp
    src/keybinding.cpp
    src/config.cpp
    src/settings.cpp
    src/settingsrows.cpp
    src/settingsdialog.cpp
    src/settingslaunch.cpp
    src/settingsprovider.cpp
    src/candwindow.cpp
)

# Component TUs own their caches: the shell includes no .cpp and the
# components include no shell .cpp either (DIP + SAP, docs/
# REFACTOR-TSF-COMPONENTS.md). src/keybinding.cpp is a compiled TU whose
# GetKeyState source is ONE registered provider (keybinding.h,
# hunmin_tsf_keystate_register) - the seam the old per-TU
# #define GetKeyState macro carried.

if(NOT DEFINED ENV{HUNMIN_VS2022_ROOT})
  message(WARNING "HUNMIN_VS2022_ROOT is not set; set it to your Visual Studio 2022 BuildTools root (e.g. C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools)")
endif()

# Export registration helpers via DEF file eventually.
target_compile_definitions(hunmin-tsf PRIVATE UNICODE _UNICODE)
# CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: as a subdirectory of the
# top-level project the latter is the repo root and the engine include path
# would point one level too high (measured: the `builds` guard rebuilt the
# DLL and hit C1083 on hunmin.h).
target_include_directories(hunmin-tsf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libhunmin/include
)
# Link the engine statically (see the note in the top-level CMakeLists.txt).
# A TIP is loaded by its registry path alone: nothing sets a search path for
# its dependencies, and the DLL's own directory is not searched for them, so a
# shared libhunmin.dll resolves to ERROR_MOD_NOT_FOUND in every host process.
# The core is built from the repo root into build-msvc (MSVC, this frontend)
# or build (MinGW/POSIX); search every layout rather than assuming one name.
set(_HUNMIN_BUILD_ROOTS
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build-msvc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build-msvc/Release
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build-msvc/Debug
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build/Release
    ${CMAKE_CURRENT_SOURCE_DIR}/../../build/Debug)
find_library(HUNMIN_STATIC_LIB
    NAMES hunmin_static hunmin_static.lib
    PATHS ${_HUNMIN_BUILD_ROOTS}
    NO_DEFAULT_PATH)
find_library(HUNMIN_LIB
    NAMES hunmin hunmin.lib
    PATHS ${_HUNMIN_BUILD_ROOTS}
    NO_DEFAULT_PATH)
if(HUNMIN_STATIC_LIB)
    set(HUNMIN_ENGINE_LIB ${HUNMIN_STATIC_LIB})
elseif(HUNMIN_LIB)
    message(WARNING "hunmin_static.lib not found; linking the shared engine, so "
                    "hunmin.dll must be resolvable at run time or the TIP will fail to load")
    set(HUNMIN_ENGINE_LIB ${HUNMIN_LIB})
else()
    # Skip, not fatal: a fresh top-level configure builds the core AFTER this
    # directory is processed, and the standalone flow builds the core first.
    # Absent-and-loud beats breaking every configure that happens to precede
    # the core build; reconfigure once hunmin_static exists.
    message(STATUS "libhunmin (.lib) not found: hunmin-tsf and its tier-F1 harness disabled; build the core first")
    return()
endif()
target_link_libraries(hunmin-tsf PRIVATE
    ${HUNMIN_ENGINE_LIB}
    ole32
    oleaut32
    user32
    imm32
)
# Use module definition file for explicit exports, and the icon resource
# (assets/README.md: provenance of hunmin.ico; MSVC's rc.exe compiles it,
# the resource lands as id 1 = the profile icon and the barbar button).
enable_language(RC)
target_sources(hunmin-tsf PRIVATE src/hunmin-tsf.rc)
set_target_properties(hunmin-tsf PROPERTIES
    SUFFIX ".dll"
    LINK_OPTIONS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/hunmin-tsf.def")

# Stage the engine data next to the DLL (issue #66, data-loading half). The
# adapter resolves data from <module>\data before the repo-climb fallback
# (hunmin_ctx_from_module), so the shipped shape — a registered TIP with its
# data beside it — is exactly what this build must produce. Without the
# staging, the DLL found data only inside a checkout, four directories up.
add_custom_command(TARGET hunmin-tsf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../../libhunmin/data"
            "$<TARGET_FILE_DIR:hunmin-tsf>/data"
    COMMENT "Staging libhunmin/data next to hunmin-tsf.dll")

# Tier F1: the mock-TSF harness (issue #80) drives the adapter above over
# in-memory COM doubles and needs no DLL, registry or window. MSVC builds
# it; a POSIX or non-MSVC configure of this directory registers nothing.
# enable_testing() must stand here: configured standalone (the documented
# flow, tools/build-msvc.bat) nothing above this directory enables CTest,
# and without it every add_test below is a silent no-op — ctest reported
# "No tests were found" while the harness itself built fine (measured
# 2026-10-17, this host).
if(WIN32 AND MSVC)
  enable_testing()
  # GoogleTest via FetchContent, mirroring the top-level block (issue #109):
  # the migration moved tsf-test onto GTest::gtest_main, whose target only
  # the top-level configure used to declare -- the standalone configure (the
  # installer's build shape, tools/build-windows-installer.sh) then failed
  # with "GTest::gtest_main ... was not found" (measured 2026-10-20, issue
  # #99). Gated on BUILD_TESTING alone: the gtest target is the test
  # framework itself, while Python3_FOUND (the top-level guard) decides only
  # the trace-loop registrations inside test/. Same offline escape hatch.
  if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.17.0
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_subdirectory(test)
endif()
