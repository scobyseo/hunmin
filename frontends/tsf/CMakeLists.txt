cmake_minimum_required(VERSION 3.20)
project(hunmin-tsf LANGUAGES C CXX)

add_library(hunmin-tsf MODULE
    src/dllmain.cpp
    src/tsf.cpp
)

if(NOT DEFINED ENV{HUNMIN_VS2022_ROOT})
  message(WARNING "HUNMIN_VS2022_ROOT is not set; set it to your Visual Studio 2022 BuildTools root (e.g. C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools)")
endif()

# Export registration helpers via DEF file eventually.
target_compile_definitions(hunmin-tsf PRIVATE UNICODE _UNICODE)
target_include_directories(hunmin-tsf PRIVATE
    ${CMAKE_SOURCE_DIR}/../../libhunmin/include
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
    message(FATAL_ERROR "libhunmin (.lib) not found; build the core first")
endif()
target_link_libraries(hunmin-tsf PRIVATE
    ${HUNMIN_ENGINE_LIB}
    ole32
    user32
)
# Use module definition file for explicit exports.
set_target_properties(hunmin-tsf PROPERTIES
    SUFFIX ".dll"
    LINK_OPTIONS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/hunmin-tsf.def")
