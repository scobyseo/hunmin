# SPDX-License-Identifier: LGPL-2.1-or-later
#
# The JNI shim, built by AGP's externalNativeBuild.
#
# This builds ONE small library against the ALREADY-built libhunmin.so. It does
# not compile the engine itself: the engine is cross-compiled by
# tools/build-android.sh, which the repo's own cmake/android.toolchain.cmake
# drives with the same NDK clang. Two build systems compiling the same sources
# would be a way to ship an .so the tier E0 goldens never ran against.
#
# So: find the prebuilt libhunmin.so for this ABI under
# ../../build-android/<abi>/jniLibs/<abi>/ and link it. If it is missing, fail
# loudly -- a JNI shim that silently links nothing fails at System.loadLibrary
# on the device, i.e. the tier E2 run, many minutes later.
#
# AGP passes ANDROID_ABI and the NDK toolchain; this project adds only the
# hunmin.h include dir and the link.
cmake_minimum_required(VERSION 3.22)
project(hunmin_jni LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# THE OUTPUT NAME MUST NOT BE `hunmin`. It was, and that collided with the
# engine's own libhunmin.so: this shim LINKS libhunmin.so, so ninja's output
# path and its link input were the same file. The result was a hardlink back to
# the engine -- the JNI object was compiled and then dropped on the floor, the
# library had zero Java_org_* symbols, and the APK shipped the bare engine with
# no adapter. Every build reported success. `jniLibs` then also had two
# libhunmin.so for the same ABI, and the engine's srcDir won the merge.
#
# Hence libhunjni.so, loaded by System.loadLibrary("hunjni") in HunminNative.kt.
# The name is asserted below so a rename on one side fails the build rather than
# producing an UnsatisfiedLinkError at the first keystroke on a device.
add_library(hunjni SHARED hunmin-jni.c)

# The engine's public header. Its include dir is libhunmin/include in the repo.
# Find the repo root by LOOKING for it, not by counting "../".
#
# The count is six (cpp -> src -> app -> android -> frontends -> repo root) and
# that is not something to hardcode: five, six and seven were each tried here,
# and every wrong value produces the same message, "libhunmin.so not found",
# which reads like a missing build step rather than a miscounted path. Walking up
# until libhunmin/include/hunmin.h appears cannot be miscounted, and it survives
# the file being moved.
set(_hunmin_up "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_i RANGE 1 8)
  if(EXISTS "${_hunmin_up}/libhunmin/include/hunmin.h")
    set(HUNMIN_REPO_ROOT "${_hunmin_up}")
    break()
  endif()
  get_filename_component(_hunmin_up "${_hunmin_up}/.." REALPATH)
endforeach()
if(NOT HUNMIN_REPO_ROOT)
  message(FATAL_ERROR
    "walked 8 levels up from ${CMAKE_CURRENT_SOURCE_DIR} and never found "
    "libhunmin/include/hunmin.h -- is this file still under the repo?")
endif()
message(STATUS "hunmin-jni: repo root is ${HUNMIN_REPO_ROOT}")
target_include_directories(hunjni PRIVATE ${HUNMIN_REPO_ROOT}/libhunmin/include)

# Locate the engine built by tools/build-android.sh for THIS abi.
#
# The path is named DIRECTLY rather than through find_library, because the NDK
# toolchain sets CMAKE_FIND_ROOT_PATH_MODE_LIBRARY to ONLY and re-roots every
# search under the sysroot -- so find_library with NO_DEFAULT_PATH silently
# returns NOTFOUND for a file that is sitting right there. An explicit path is
# also what the repo's own cmake/android.toolchain.cmake comment recommends
# ("find_library cannot find it by name"), and passing the full path to the
# linker is what GNU ld / lld accept.
set(_hunmin_root ${HUNMIN_REPO_ROOT}/build-android/${ANDROID_ABI}/jniLibs/${ANDROID_ABI})
set(HUNMIN_ENGINE_LIB "${_hunmin_root}/libhunmin.so")
if(NOT EXISTS "${HUNMIN_ENGINE_LIB}")
  message(FATAL_ERROR
    "libhunmin.so for ABI '${ANDROID_ABI}' not found at ${HUNMIN_ENGINE_LIB}.\n"
    "Build it first:  ./tools/build-android.sh ${ANDROID_ABI} 24")
endif()

# Link the engine by its exact path. By-name would find a host libhunmin.so or
# none, and a shim that links the wrong engine passes every compile and fails
# only when the composition comes out wrong on a device.
target_link_libraries(hunjni PRIVATE ${HUNMIN_ENGINE_LIB})

# No C++ runtime, no exceptions, no RTTI: the shim is C, links libhunmin (also
# C, and it uses libc's malloc not a C++ allocator), and needs nothing else.
# ANDROID_STL=none is set in build.gradle.kts to match.
target_compile_options(hunjni PRIVATE -Wall -Wextra)

# Assert the shim really is the shim. A shared library that links the engine and
# exports no Java_org_* symbol is a JNI layer that will throw
# UnsatisfiedLinkError on the device, and nothing else in the build notices:
# it compiles, it links, it packages. Checking here turns that into a configure
# error on the build machine. `nm -D` rather than readelf because the NDK's
# llvm-nm is not guaranteed on PATH.
find_program(NM_EXECUTABLE NAMES llvm-nm nm)
if(NM_EXECUTABLE)
  # shellcheck disable=SC2016
  set(_shim "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libhunjni.so")
  add_custom_command(TARGET hunjni POST_BUILD
    COMMAND ${NM_EXECUTABLE} -D --defined-only ${_shim}
    COMMAND sh -c
      "'${NM_EXECUTABLE}' -D --defined-only '${_shim}' | grep -q Java_org_ || { echo 'FATAL: ${_shim} exports no Java_org_* symbol; the JNI shim was not built (is the output name colliding with the engine?)' >&2; exit 1; }"
    VERBATIM)
endif()
