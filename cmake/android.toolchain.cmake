# Cross-compile libhunmin for Android with the NDK's clang, without invoking
# ndk-build: this library has no Android dependency at all, it only has to be
# loadable by an app that does. cmake/toolchains/ndk-abi.cmake in the NDK does
# the same thing through ndk-build's variables; we have none of those.
#
# Usage:
#   cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=cmake/android.toolchain.cmake \
#         -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-35 \
#         -DBUILD_TESTING=OFF -DHUNMIN_FCITX5=OFF
#   cmake --build build-android
#
# CMAKE_TOOLCHAIN_FILE is read on the FIRST configure only. Re-running cmake on
# an existing build dir with a different ABI silently keeps the old one -- and
# the .so it produces has the other's name in it -- so use one build dir per ABI
# (see tools/build-android.sh).

set(ANDROID_NDK "$ENV{ANDROID_NDK}" CACHE PATH "NDK root")
if(NOT ANDROID_NDK)
  message(FATAL_ERROR "ANDROID_NDK is not set; export it, or pass -DANDROID_NDK=...")
endif()

# The NDK ships the sysroot under the *toolchain* dir, not the NDK root, and
# the clang wrapper needs it to find libc++/libandroid. api 35 == android-35,
# the platform the emulator image in docs/ENVIRONMENT.md runs.
set(ANDROID_ABI "arm64-v8a" CACHE STRING "arm64-v8a | x86_64 | x86 | armeabi-v7a")
set(ANDROID_PLATFORM "android-24" CACHE STRING "lowest supported API level")
string(REGEX REPLACE "^android-" "" ANDROID_API "${ANDROID_PLATFORM}")

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION ${ANDROID_API})

set(_hunmin_tag "$ANDROID_ABI$ANDROID_API")
set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
set(CMAKE_ANDROID_NDK "${ANDROID_NDK}")

# An ABI is a *packaging* name; a target triple wants the *architecture* name.
# The three differ in ways that are easy to get wrong and fail late:
#   arm64-v8a -> aarch64   (not arm64-v8a, not arm64)
#   x86_64    -> x86_64
#   x86       -> i686
#   armeabi-v7a -> armv7a
# Getting it wrong does not error usefully: clang reads `-target x86_6424-linux-
# android` as arch x86_64 with a garbage vendor tag and says "unknown target
# triple 'x86_6424-unknown-linux-android'", which reads like a malformed API
# level rather than a wrong ABI name.
if(ANDROID_ABI STREQUAL "arm64-v8a")
  set(_hunmin_arch "aarch64")
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(_hunmin_arch "x86_64")
elseif(ANDROID_ABI STREQUAL "x86")
  set(_hunmin_arch "i686")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
  set(_hunmin_arch "armv7a")
else()
  message(FATAL_ERROR "unknown ANDROID_ABI '${ANDROID_ABI}'")
endif()

# clang, not gcc: NDK r18+ dropped gcc, and the NDK toolchain dir has a single
# target triple per ABI, so the binutils-style wrappers are gone too. Target the
# architecture directly -- clang resolves the platform from -target.
#
# The API level goes at the END of the triple, not next to the architecture:
# x86_64-linux-android24, not x86_6424-linux-android. There is no separator, so
# the malformed form is not rejected as such -- clang parses "x86_6424" as the
# architecture and reports `unknown target triple 'x86_6424-unknown-linux-
# android'`, which reads like a mangled message rather than a misplaced field.
# The prebuilt tag is host-specific AND varies across NDK releases, so detect
# it by directory existence instead of guessing from uname: whatever tag's
# prebuilt tree actually exists is the one to point at.
file(TO_CMAKE_PATH "${ANDROID_NDK}/toolchains/llvm/prebuilt" _hunmin_prebuilt)
# CMake's foreach restores its loop variable after endforeach, so copy the hit
# into the persistent name inside the loop.
foreach(_hunmin_tag IN ITEMS windows-x86_64 linux-x86_64 darwin-x86_64)
  if(EXISTS "${_hunmin_prebuilt}/${_hunmin_tag}")
    set(_hunmin_host_tag "${_hunmin_tag}")
    break()
  endif()
endforeach()
if(NOT _hunmin_host_tag)
  message(FATAL_ERROR "no NDK prebuilt tree under ${_hunmin_prebuilt}")
endif()
# Windows prebuilts name the driver clang.exe; CMake does not append the
# extension to an explicitly given path, and "clang" without it fails as
# "not a full path to an existing compiler tool".
if(_hunmin_host_tag MATCHES "^windows")
  set(_hunmin_clang "clang.exe")
else()
  set(_hunmin_clang "clang")
endif()

set(CMAKE_C_COMPILER "${_hunmin_prebuilt}/${_hunmin_host_tag}/bin/${_hunmin_clang}")
set(CMAKE_C_COMPILER_TARGET "${_hunmin_arch}-linux-android${ANDROID_API}")
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER_ID_RUN TRUE)
set(CMAKE_C_COMPILER_ID Clang)

set(CMAKE_FIND_ROOT_PATH "${_hunmin_prebuilt}/${_hunmin_host_tag}/sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Everything lands in jniLibs/<abi>/ -- the layout the APK's sourceSets use, so
# the Gradle side needs no knowledge of where CMake actually wrote.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}")

# __ANDROID__ is what libhunmin's POSIX shims test on; without it the data
# loader's HUNMIN_DEFAULT_DATA_DIR branch compiles out and hunmin_ctx_new()
# finds no layouts on a device.
add_compile_definitions(__ANDROID__=1)

# 16KB page alignment for native libraries (Android requirement for
# extractNativeLibs=false / mmap from APK). Both max-page-size and
# common-page-size must be 16384 (16KB) so the .so is page-aligned in the APK.
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")