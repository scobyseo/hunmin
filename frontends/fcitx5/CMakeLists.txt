# fcitx5 frontend — thin adapter (event mapping + presentation only, rule #4).
# Built from the root project when Fcitx5Core >= 5.1.13 is available; the D1
# test links the fcitx5 TestFrontend module (docs/TESTING.md).
set(REQUIRED_FCITX_VERSION 5.1.13)

# root project is C-only; the addon and its test are C++
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(src)

if(BUILD_TESTING AND Fcitx5Module_FOUND)
  add_subdirectory(test)
endif()
