#!/usr/bin/env bash
# Build the JNI shim for the HOST, so tier E1 can drive the real engine in a JVM
# unit test with no emulator and no Android runtime.
#
#   tools/build-host-jni.sh            -> build/libhunjni-host.so
#
# Why this exists: HunminNativeBridge is one C file (frontends/android/app/src/main/
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
    # The root is where a mingw tree keeps the DLL; a multi-config VS tree
    # keeps it under the per-config dir (build-msvc/Release) -- search both
    # (measured 2026-10-19, issue #109: the root probe alone missed it and
    # the runner reported the engine missing on a built tree).
    ENGINE=""
    for e in "$BUILD_DIR/libhunmin.dll" "$BUILD_DIR/hunmin.dll" \
             "$BUILD_DIR/Release/libhunmin.dll" "$BUILD_DIR/Release/hunmin.dll" \
             "$BUILD_DIR/Debug/libhunmin.dll" "$BUILD_DIR/Debug/hunmin.dll"; do
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

# Host C compiler, three sources, most specific first:
#
#   1. $CC -- explicit override, honoured first as before.
#   2. $BUILD_DIR/CMakeCache.txt -- CMAKE_C_COMPILER, the toolchain that built
#      the engine DLL this shim links. The shim is load-coupled to that
#      engine, so that toolchain is the honest default, not whatever a PATH
#      probe happens to find. Measured 2026-10-17, issue #86: with CC unset
#      on the Windows host, the old PATH probe picked the MSVC-target clang
#      in C:/Program Files/LLVM/bin, which compiled hunmin-jni.c against VS 18
#      BuildTools headers (MSVC 14.51.36231 -- newer than the 14.44 recorded
#      in docs/ENVIRONMENT.md) and linked with lld-link, demanding `hunmin.lib`
#      -- a file a mingw build tree never has (it holds libhunmin.dll.a). All
#      16 engine-android-conformance-* tests failed on that one selection.
#      The same probe would also hand the shim a *different* mingw (WinLibs
#      gcc is on that PATH) than the one that built libhunmin.dll (llvm-mingw).
#   3. the old PATH probe, kept as fallback for a build dir with no cache
#      (engine compiled by hand). Its wrong-target failure mode above is why
#      it is no longer first.
#
# A host with none of the three gets the documented skip, not a red test.
# MSVC (cl / clang-cl) is refused rather than used: this script is a
# mingw/POSIX path by contract (see the HOSTOS comment above), cl misreads
# every gcc-style flag below, and falling through keeps the historical
# behaviour for that corner instead of inventing a new one.
CC_BIN="${CC:-}"
if [ -z "$CC_BIN" ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    # The type suffix varies: CMake stores a -D-passed compiler as
    # :UNINITIALIZED until its first normalization, :STRING or :FILEPATH
    # afterwards, and :INTERNAL on some re-configures (all three measured
    # 2026-10-19, issue #109: a fresh build-mingw cache held :UNINITIALIZED,
    # which the old explicit list missed -- the probe then fell through to the
    # PATH clang, and every conformance test died on the missing hunmin.lib
    # again). Anchoring on "CMAKE_C_COMPILER:" with any type leaves
    # CMAKE_C_COMPILER_AR and friends out.
    _cache_cc="$(sed -n 's/^CMAKE_C_COMPILER:[A-Z_]*=//p' \
                       "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
    # A tree configured with --toolchain sets CMAKE_C_COMPILER as a NORMAL
    # variable rather than a cache entry, so the line above can find
    # nothing and this probe falls through to the PATH clang -- the
    # MSVC-targeted one -- whose link then demands hunmin.lib, a file no
    # mingw tree has (measured 2026-10-25: `grep -c '^CMAKE_C_COMPILER:'
    # CMakeCache.txt` = 0 on a cmake/toolchain-mingw-w64.cmake tree, while
    # CMakeFiles/*/CMakeCCompiler.cmake holds the compiler that built the
    # engine). Read that file before giving up on the cache.
    if [ -z "$_cache_cc" ]; then
        for _f in "$BUILD_DIR"/CMakeFiles/*/CMakeCCompiler.cmake; do
            [ -f "$_f" ] || continue
            _cache_cc="$(sed -n 's/^set(CMAKE_C_COMPILER "\(.*\)")$/\1/p' "$_f" | head -n 1)"
            [ -n "$_cache_cc" ] && break
        done
    fi
    case "$_cache_cc" in
        ""|*cl.exe|*cl) ;;                 # absent, or an MSVC tree
        *) [ -f "$_cache_cc" ] && CC_BIN="$_cache_cc" ;;
    esac
fi
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
# The engine's link input beside -L"$BUILD_DIR": a multi-config tree (VS)
# keeps hunmin.lib under the per-config dir (build-msvc/Release), not at the
# root a mingw tree uses; search both rather than dying on the first
# (measured 2026-10-19, issue #109: LNK1181 on '..\..\hunmin.lib').
LIBDIR_FLAGS=(-L"$BUILD_DIR")
if [ "$HOSTOS" = windows ]; then
    for d in "$BUILD_DIR/Release" "$BUILD_DIR/Debug"; do
        if [ -f "$d/hunmin.lib" ] || [ -f "$d/libhunmin.dll.a" ]; then
            LIBDIR_FLAGS+=(-L"$d")
        fi
    done
fi
# --- one builder, not fifteen ---------------------------------------------
# Every android trace test runs this script when the shim is missing, and
# `ctest -j16` starts them together: on a cold tree fifteen compilers wrote the
# SAME "$OUT" while each one's post-link `ldd` read a file another process was
# rewriting, so `ldd` found no libhunmin and the check below failed the test.
# Measured 2026-11-03, issue #234: with the shim deleted, 11 of 15 tier-C
# android tests fail; with it present, 0 of 15. Same tree, same selection.
#
# So the build is a critical section (the flock the classpath cache in
# tools/android-trace-emit.sh already uses, with a mkdir lock for git-bash,
# which ships no flock), the result is published by rename so a reader never
# loads a half-written library, and a caller that finds a shim newer than both
# its inputs compiles nothing instead of doing it fifteen times over.
OUT_TMP="$OUT.tmp.$$"
LOCK="$BUILD_DIR/.build-host-jni.lock"
trap 'rm -f "$OUT_TMP"' EXIT

# The engine is reached through libhunmin.so -> libhunmin.so.0 ->
# libhunmin.so.0.2.0, and `find -newer` compares the mtime of the SYMLINK: a
# relinked engine leaves those untouched, so freshness is decided on the file
# the linker actually read (measured: touching the real .so left this script
# reporting the shim up to date).
ENGINE_REAL="$(readlink -f "$ENGINE" 2>/dev/null || printf '%s' "$ENGINE")"

shim_is_fresh() {
    [ -f "$OUT" ] || return 1
    [ -z "$(find "$SRC" "$ENGINE_REAL" -newer "$OUT" -print -quit 2>/dev/null)" ]
}

build_shim() {
    shim_is_fresh && return 0
    "$CC_BIN" "${LINK_FLAGS[@]}" \
      -I"$JNI_INC" -I"$JNI_MD" \
      -I"$ROOT/libhunmin/include" \
      -o "$OUT_TMP" "$SRC" \
      "${LIBDIR_FLAGS[@]}" -lhunmin
    # mv is atomic within a directory: a concurrent `java` sees either the old
    # library or the new one, never a partial link.
    mv -f "$OUT_TMP" "$OUT"
}

if command -v flock >/dev/null 2>&1; then
    exec {lockfd}>"$LOCK"
    flock "$lockfd"
    build_shim
    flock -u "$lockfd"
    exec {lockfd}>&-
else
    waited=0
    while ! mkdir "$LOCK" 2>/dev/null; do
        waited=$((waited + 1))
        if [ "$waited" -ge 120 ]; then
            echo "  lock $LOCK looks stale, proceeding unlocked" >&2
            break
        fi
        sleep 1
    done
    build_shim
    rmdir "$LOCK" 2>/dev/null || true
fi

# Proof it is a loadable library for this JVM and not merely a file: the
# dependency it actually resolved, which is the thing that silently differs if
# the -L path is wrong. ldd on ELF hosts; objdump on Windows DLLs.
#
# The dependency text is captured first and searched with `case`, never with
# `producer | grep -q`: this script runs under `set -o pipefail`, `grep -q` exits
# at the first match, the producer then dies of SIGPIPE (141), and pipefail
# reports the whole pipeline as failed. Measured 2026-11-03, issue #234:
# `ldd "$OUT" | grep -q libhunmin` under sixteen CPU hogs failed 51 times in
# 300, on a library that was fine every time -- which is what "the shim did not
# link libhunmin" was actually saying in the cold parallel runs.
deps_matching() {
    # Print the first line of $1 containing $2, without a pipeline that can
    # break: a here-string keeps the whole dependency list inside this shell.
    local _line=""
    local _want="$2"
    while IFS= read -r _line; do
        case "$_line" in
            *"$_want"*) printf '%s\n' "$_line"; return 0 ;;
        esac
    done <<< "$1"
    return 1
}

if [ "$HOSTOS" = windows ]; then
    if command -v objdump >/dev/null 2>&1; then
        deps="$(objdump -p "$OUT" 2>&1 || true)"
        if ! engine_line="$(deps_matching "$deps" "hunmin")"; then
            echo "  the shim did not link libhunmin -- tier E1 would test nothing" >&2
            exit 1
        fi
        printf '  engine: %s\n' "$(printf '%s' "$engine_line" | tr -s ' ' | sed 's/^ *//')"
    else
        echo "  note: no objdump on PATH; the dependency proof is skipped" >&2
    fi
else
    deps="$(ldd "$OUT" 2>&1 || true)"
    if ! engine_line="$(deps_matching "$deps" "libhunmin")"; then
        echo "  the shim did not link libhunmin -- tier E1 would test nothing" >&2
        exit 1
    fi
    printf '  engine: %s\n' "$(printf '%s' "$engine_line" | tr -s ' ' | sed 's/^ *//')"
fi
echo "  -> $OUT"