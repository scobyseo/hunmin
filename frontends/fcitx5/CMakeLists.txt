# fcitx5 frontend — thin adapter (event mapping + presentation only, rule #4).
# Built from the root project when Fcitx5Core is available; the D1
# test links the fcitx5 TestFrontend module (docs/TESTING.md).
#
# 5.1.13 is the REAL API floor, measured from upstream tags 2026-10-13
# (issue #8): FCITX_ADDON_FACTORY_V2 (engine.cpp:619) first appears in 5.1.13,
# and setComment (engine.cpp:461) in 5.1.9. Ubuntu 24.04 LTS ships 5.1.7, which
# has NEITHER, so the addon cannot compile against the distro package -- this is
# a genuine API gap, not just the build-macro gap. "Supports Ubuntu 24.04"
# therefore means: CI builds fcitx5 from source and the .deb requires >= 5.1.13;
# a 24.04 user needs that newer fcitx5 too (the source build is the install
# path), not the distro's 5.1.7.
#
# Do not lower this below 5.1.13 without removing the FCITX_ADDON_FACTORY_V2
# usage. An earlier attempt set it to 5.1.7 on the theory that only the cmake
# macro was missing; CI's compile error (setComment / FCITX_ADDON_FACTORY_V2
# undeclared) disproved that and the value was restored.
set(REQUIRED_FCITX_VERSION 5.1.13)

# root project is C-only; the addon and its test are C++
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

# add_fcitx5_addon and FCITX_ADDON_CMAKE_LIBRARY_TYPE ship in
# Fcitx5Macros.cmake, which fcitx5 only packages from 5.1.19. Ubuntu 24.04 LTS
# ships 5.1.7, whose fcitx5-modules-dev has Fcitx5CompilerSettings.cmake but no
# Fcitx5Macros.cmake -- so without the fallback below, configuring the addon
# dies at src/CMakeLists.txt:1 with "Unknown CMake command add_fcitx5_addon",
# which is exactly what broke CI on every run (measured 2026-10-13, issue #8).
#
# This is a build-tooling gap, not a library gap: the addon's C++ uses only
# long-stable fcitx5 APIs (CandidateWord, PageableCandidateList,
# updateUserInterface, InputContextProperty -- all present since 4.x), so a
# built .deb loads against 24.04's 5.1.7 runtime unchanged. We therefore ship
# the one macro locally instead of requiring a rebuilt fcitx5, which keeps
# "supports Ubuntu 24.04" mean: dpkg -i against the distro package, no source
# build, no added dependency.
#
# The macro below is copied in behaviour, not in code (AGENTS.md: read upstream
# for semantics, write the behaviour, copy nothing) -- it is the documented
# contract of add_fcitx5_addon: an addon library target. When the guard finds a
# real Fcitx5Macros.cmake it includes that and defines nothing, so on 5.1.19+
# this file is inert.
if(NOT COMMAND add_fcitx5_addon)
  if(EXISTS "${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5Macros.cmake")
    include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5Macros.cmake")
  endif()
endif()
if(NOT COMMAND add_fcitx5_addon)
  option(BUILD_SHARED_FCITX_ADDON "Build addon as shared library" ON)
  if(BUILD_SHARED_FCITX_ADDON)
    set(FCITX_ADDON_CMAKE_LIBRARY_TYPE MODULE)
    set(FCITX_ADDON_TYPE "SharedLibrary")
  else()
    set(FCITX_ADDON_CMAKE_LIBRARY_TYPE STATIC)
    set(FCITX_ADDON_TYPE "StaticLibrary")
  endif()
  function(add_fcitx5_addon target_name)
    add_library(${target_name} ${FCITX_ADDON_CMAKE_LIBRARY_TYPE} ${ARGN})
    set_target_properties(${target_name} PROPERTIES FCITX_ADDON TRUE)
  endfunction()
endif()

add_subdirectory(src)

if(BUILD_TESTING AND Fcitx5Module_FOUND)
  add_subdirectory(test)
endif()
