#!/usr/bin/env bash
# Build libhunmin for one Android ABI, and print where the .so landed.
#
#   tools/build-android.sh arm64-v8a [api]        # a real device
#   tools/build-android.sh x86_64 [api]           # the emulator in docs/ENVIRONMENT.md
#
# Deliberately not in CMake: CMAKE_TOOLCHAIN_FILE is honoured on the first
# configure only, so a second ABI in the same build dir would keep the first
# ABI's compiler while producing a library named after the second. One dir per
# ABI, created here, is the only way to make that mistake impossible.
#
# BUILD_TESTING=OFF and HUNMIN_FCITX5=OFF are not optimisations. Both subtrees
# add_test()/add_library() calls that expect a host: the fcitx5 addon needs
# Fcitx5Core's cmake package and links libc++, and the tier A/B goldens would
# register hunmin-sim, which is a host binary with no business in an APK.
set -euo pipefail

ABI="${1:?usage: build-android.sh <abi> [api]}"
API="${2:-24}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

: "${ANDROID_NDK:?export ANDROID_NDK=~/Android/Sdk/ndk/27.3.13750724}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"

OUT="$ROOT/build-android/$ABI"
cmake -S "$ROOT" -B "$OUT" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/android.toolchain.cmake" \
  -DANDROID_NDK="$ANDROID_NDK" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM="android-$API" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=OFF \
  -DHUNMIN_FCITX5=OFF >/dev/null
cmake --build "$OUT" --target hunmin hunmin-sim

# Proof it is the right machine, not just a file with a plausible name. readelf
# is host binutils and reads any ELF; llvm-readelf ships with the NDK.
READELF="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
SO="$OUT/jniLibs/$ABI/libhunmin.so"
"$READELF" -h "$SO" | sed -n 's/^ *Machine: */  machine: /p'
"$READELF" -d "$SO" | sed -n 's/^ *\(NEEDED\|SONAME\).*/  &/p'
printf '  size: %s bytes\n' "$(stat -c %s "$SO")"
echo "  -> $SO"