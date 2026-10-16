cmake_minimum_required(VERSION 3.20)
project(hunmin-tsf LANGUAGES C CXX)

add_library(hunmin-tsf MODULE
    src/dllmain.cpp
    src/tsf.cpp
)

# Export registration helpers via DEF file eventually.
target_compile_definitions(hunmin-tsf PRIVATE UNICODE _UNICODE)
target_include_directories(hunmin-tsf PRIVATE
    ${CMAKE_SOURCE_DIR}/../../libhunmin/include
)
# Link the engine (build it first or point at an existing .lib)
find_library(HUNMIN_LIB
    NAMES hunmin hunmin.lib
    PATHS ${CMAKE_SOURCE_DIR}/../../build
          ${CMAKE_SOURCE_DIR}/../../build/Release
          ${CMAKE_SOURCE_DIR}/../../build/Debug
    NO_DEFAULT_PATH)
if(NOT HUNMIN_LIB)
    message(FATAL_ERROR "libhunmin (.lib) not found; build the core first")
endif()
target_link_libraries(hunmin-tsf PRIVATE
    ${HUNMIN_LIB}
    msctf
    ole32
    user32
)
set_target_properties(hunmin-tsf PROPERTIES SUFFIX ".dll")