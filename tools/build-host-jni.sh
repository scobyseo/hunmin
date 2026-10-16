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
SRC="$ROOT/frontends/android/app/src/main/cpp/hunmin-jni.c"
# Convert per-host differences once: llvm-mingw keeps the lib/ prefix on
# Windows shared libraries (measured: build-mingw/ holds libhunmin.dll +
# libhunmin.dll.a); Linux keeps libhunmin.so. MSVC can exist only as the
# hunmin.lib import, which the TIP links -- this script is a mingw/POSIX
# path, documented as such at the no-compiler exit below.
HOSTOS=posix
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOSTOS=windows ;;
esac
if [ "$HOSTOS" = windows ]; then
    ENGINE=""
    for e in "$BUILD_DIR/libhunmin.dll" "$BUILD_DIR/hunmin.dll"; do
        if [ -f "$e" ]; then ENGINE="$e"; break; fi
    done
    OUT="$BUILD_DIR/libhunjni-host.dll"
    JNI_MD_ARCH=win32
else
    ENGINE="$BUILD_DIR/libhunmin.so"
    OUT="$BUILD_DIR/libhunjni-host.so"
    case "$(uname -m)" in
        x86_64) JNI_MD_ARCH=linux ;;
        aarch64) JNI_MD_ARCH=aarch64 ;;
        *) JNI_MD_ARCH="$(uname -m | sed 's/^x86-/x86_/')" ;;
    esac
fi

if [ ! -f "$ENGINE" ]; then
  echo "  $ENGINE missing -- run 'cmake -B build && cmake --build build' first" >&2
  exit 1
fi

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
# JDK probe: POSIX java lives as java (needs -x), Windows as java.exe (-f).
if [ -f "$JDK/bin/java.exe" ]; then
    JAVA_OK=1
elif [ -x "$JDK/bin/java" ]; then
    JAVA_OK=1
else
    JAVA_OK=0
fi
if [ "$JAVA_OK" != 1 ]; then
  echo "skip: no JDK (export JAVA_HOME); tier E1 needs one to compile JNI headers" >&2
  exit 77
fi

# The JNI headers ship with the JDK.
JNI_INC="$JDK/include"
JNI_MD="$JDK/include/$JNI_MD_ARCH"

# Host C compiler. Honour CC first; the common names next. A host without
# one gets the documented skip, not a red test.
CC_BIN="${CC:-}"
if [ -z "$CC_BIN" ]; then
    for c in cc clang gcc; do
        if command -v "$c" >/dev/null 2>&1; then CC_BIN="$(command -v "$c")"; break; fi
    done
fi
if [ -z "$CC_BIN" ]; then
  echo "skip: no C compiler on PATH (export CC); the host JNI shim needs one" >&2
  exit 77
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # The compiler and the JVM here are native Windows programs and reject
        # /c/... arguments (measured: "no such file or directory:
        # /c/...hunmin-jni.c" from llvm-mingw clang); cygpath -m gives the
        # C:/... form both accept. POSIX hosts are untouched by the branch.
        ROOT="$(cygpath -m "$ROOT")"
        BUILD_DIR="$(cygpath -m "$BUILD_DIR")"
        [ -n "$ENGINE" ] && { ENGINE="$(cygpath -m "$ENGINE")" || true; }
        OUT="$(cygpath -m "$OUT")"
        SRC="$(cygpath -m "$SRC")"
        JNI_INC="$(cygpath -m "$JNI_INC")"
        JNI_MD="$(cygpath -m "$JNI_MD")"
        ;;
esac

mkdir -p "$(dirname "$OUT")"
LINK_FLAGS=(-shared -O1 -g -Wall -Wextra)
if [ "$HOSTOS" = posix ]; then
    # -fPIC and rpath are ELF-side; Windows DLLs need neither, and lld on
    # mingw does not take -Wl,-rpath.
    LINK_FLAGS+=(-fPIC -Wl,-rpath,"$BUILD_DIR")
fi
"$CC_BIN" "${LINK_FLAGS[@]}" \
  -I"$JNI_INC" -I"$JNI_MD" \
  -I"$ROOT/libhunmin/include" \
  -o "$OUT" "$SRC" \
  -L"$BUILD_DIR" -lhunmin

# Proof it is a loadable library for this JVM and not merely a file: the
# dependency it actually resolved, which is the thing that silently differs if
# the -L path is wrong. ldd on ELF hosts; objdump on Windows DLLs.
if [ "$HOSTOS" = windows ]; then
    if command -v objdump >/dev/null 2>&1; then
        if ! objdump -p "$OUT" | grep -qi "DLL Name:.*hunmin"; then
            echo "  the shim did not link libhunmin -- tier E1 would test nothing" >&2
            exit 1
        fi
        printf '  engine: %s\n' "$(objdump -p "$OUT" | grep -i "DLL Name" | head -n 1 | tr -s ' ' | sed 's/^ *//')"
    else
        echo "  note: no objdump on PATH; the dependency proof is skipped" >&2
    fi
else
    if ! ldd "$OUT" | grep -q libhunmin; then
        echo "  the shim did not link libhunmin -- tier E1 would test nothing" >&2
        exit 1
    fi
    printf '  engine: %s\n' "$(ldd "$OUT" | grep libhunmin | tr -s ' ' | sed 's/^ *//')"
fi
echo "  -> $OUT"