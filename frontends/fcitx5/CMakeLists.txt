# fcitx5 frontend — thin adapter (event mapping + presentation only, rule #4).
# Built from the root project when Fcitx5Core is available; the D1
# test links the fcitx5 TestFrontend module (docs/TESTING.md).
#
# 5.1.7 is the API floor (issue #322, lowered from 5.1.13): Ubuntu 24.04 LTS
# ships exactly that, so the addon compiles against the distro package and the
# .deb installs against the distro fcitx5 -- no source build on either side.
#
# The two APIs that used to force 5.1.13 are version-guarded, not required:
#   - FCITX_ADDON_FACTORY_V2 (5.1.13+): the adapter exports the V1 factory
#     instead, on every version. 5.1.7's loader resolves only V1; 5.1.19's
#     tries V2 then falls back to V1 (both verified against the tags'
#     addonloader_p.h), so the one export loads on either.
#   - CandidateWord::setComment (5.1.9+): below it the gloss rides auxDown,
#     which classicui draws (verified in 5.1.7 inputwindow.cpp) -- the same
#     highlighted-gloss UX as IBus FE16 option A (issue #319).
#   - Instance::showCustomInputMethodInformation (5.1.11+): below it the
#     autoswitch toggle works and is logged, but has no custom popup.
# The guards are the HUNMIN_FCITX_* compile definitions below, derived from
# the version actually found; the D2 test asserts the branch the build took.
#
# History: the floor was 5.1.13 from 2026-10-13 (issue #8), measured from
# upstream tags. An earlier attempt lowered the cmake value alone, on the
# theory that only the add_fcitx5_addon macro was missing; CI's compile error
# (setComment / FCITX_ADDON_FACTORY_V2 undeclared) disproved that. The
# difference now is that the two usages themselves are guarded.
set(REQUIRED_FCITX_VERSION 5.1.7)

# API feature guard for the addon AND its tests (directory-scoped, so both
# subdirectories see the same branch the headers provide). The factory needs
# no guard: the adapter exports V1 on every version (see hunminadapter.cpp).
if(Fcitx5Core_VERSION VERSION_GREATER_EQUAL 5.1.9)
  add_compile_definitions(HUNMIN_FCITX_CANDIDATE_COMMENT)
endif()
# Instance::showCustomInputMethodInformation is @since 5.1.11 -- a third gap
# found while implementing #322, beyond the two the issue's table listed.
if(Fcitx5Core_VERSION VERSION_GREATER_EQUAL 5.1.11)
  add_compile_definitions(HUNMIN_FCITX_CUSTOM_IM_INFO)
endif()
# Test-harness APIs the D1/D2/C runners use, whose absence in 5.1.7 the issue's
# table did not list (the addon itself builds either way; the HARNESS did not).
# Both pinned by reading the upstream headers across tags, not guessed:
#   Instance::eventDispatcher()        @since 5.1.9
#   setupTestingEnvironmentPath (path)  @since 5.1.13
# test/harness_compat.h branches on these so one harness builds on both.
if(Fcitx5Core_VERSION VERSION_GREATER_EQUAL 5.1.9)
  add_compile_definitions(HUNMIN_FCITX_INSTANCE_DISPATCHER)
endif()
if(Fcitx5Core_VERSION VERSION_GREATER_EQUAL 5.1.13)
  add_compile_definitions(HUNMIN_FCITX_TESTING_ENV_PATH)
endif()

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
