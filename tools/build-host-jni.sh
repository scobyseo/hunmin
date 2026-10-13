#!/usr/bin/env bash
# Build the JNI shim for the HOST, so tier E1 can drive the real engine in a JVM
# unit test with no emulator and no Android runtime.
#
#   tools/build-host-jni.sh            -> build/libhunjni-host.so
#
# Why this exists: HunminNative is one C file (frontends/android/app/src/main/
# cpp/hunmin-jni.c) that does nothing but convert between JNI and libhunmin. The
# APK's CMake build compiles it with the NDK clang; this compiles the SAME file
# with the host compiler and links the SAME host build/libhunmin.so the rest of
# the test suite uses. So tier E1 exercises the actual adapter code against the
# actual engine -- not a stub, and not a re-implementation.
#
# What it does NOT prove, and this is the whole reason tier E2 exists: nothing
# about whether the Android platform routes hardware key events to an IME, or
# whether the .so loads on bionic, or how an app's InputConnection behaves. A
# host JNI shim is byte-for-byte the same C; the platform contract is not.
#
# Deliberately NOT in CMakeLists.txt: the repo's build must not require a JDK or
# an Android SDK. This runs only when a JDK is present, and says so.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# The engine .so lives in whatever build dir CMake was given. The default
# build/ is what the linux CI job and a local `cmake -B build` use, so this
# stays correct there. A packaging build (debian/rules) configures into
# obj-x86_64-linux-gnu instead, and HUNMIN_BUILD_DIR points this script at it
# -- without the override the deb build's tier E1 fails with "libhunmin.so
# missing" even though the engine is built, just in the other directory
# (measured on CI 2026-10-13, issue #8).
BUILD_DIR="${HUNMIN_BUILD_DIR:-$ROOT/build}"
OUT="$BUILD_DIR/libhunjni-host.so"
SRC="$ROOT/frontends/android/app/src/main/cpp/hunmin-jni.c"
ENGINE="$BUILD_DIR/libhunmin.so"

if [ ! -f "$ENGINE" ]; then
  echo "  $ENGINE missing -- run 'cmake -B build && cmake --build build' first" >&2
  exit 1
fi

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
if [ ! -x "$JDK/bin/javac" ] && [ ! -x "$JDK/bin/java" ]; then
  echo "skip: no JDK (export JAVA_HOME); tier E1 needs one to compile JNI headers" >&2
  exit 77
fi

# The JNI headers ship with the JDK. jni_md.h lives in a per-arch subdir on
# Linux, hence the second include path.
JNI_INC="$JDK/include"
case "$(uname -m)" in
  x86_64) JNI_MD="$JDK/include/linux" ;;
  aarch64) JNI_MD="$JDK/include/aarch64" ;;
  *) JNI_MD="$JDK/include/$(uname -m | sed 's/^x86-/x86_/')" ;;
esac

mkdir -p "$(dirname "$OUT")"
cc -shared -fPIC -O1 -g -Wall -Wextra \
  -I"$JNI_INC" -I"$JNI_MD" \
  -I"$ROOT/libhunmin/include" \
  -o "$OUT" "$SRC" \
  -L"$BUILD_DIR" -lhunmin \
  -Wl,-rpath,"$BUILD_DIR"

# Proof it is a loadable library for this JVM and not merely a file: ldd shows
# the engine it actually resolved, which is the thing that silently differs if
# the -L path is wrong.
if ! ldd "$OUT" | grep -q libhunmin; then
  echo "  the shim did not link libhunmin -- tier E1 would test nothing" >&2
  exit 1
fi
printf '  engine: %s\n' "$(ldd "$OUT" | grep libhunmin | tr -s ' ' | sed 's/^ *//')"
echo "  -> $OUT"