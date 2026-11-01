#!/usr/bin/env bash
# Tier E groundwork: replay the tier-A goldens through libhunmin on a real
# Android runtime, and diff. Skips (77) when no device is attached.
#
#   tools/android-golden-check.sh [data-dir]
#
# Why this exists as its own harness, separate from ctest's Android gate: the
# engine's output must not depend on the C library it was linked against. It
# did, and nothing on the host could see it.
#
# load_compose_file() deduplicated compose rows by sorting first and keeping the
# first of each run of equal keys. qsort is not stable, so for the pairs that
# sin3-p2-yet's two compose tables define *differently* -- 12 of them, e.g.
# 0x11B8+0x11B8 is ssang-cho in the mix table and U+D7A6 in ohi -- which row
# survived came out of the sort's internal pivoting. glibc and bionic disagree
# there, so the same source and the same data file produced 'ᄈᅠᆸ' on this host
# and 'ㅃ' on the emulator. Every host-only tier in docs/TESTING.md was green
# the whole time; only running the engine on bionic exposed it.
#
# A host test cannot catch that class of bug, so the check has to run where the
# other libc is. What this script buys is that the *next* libc-dependent
# divergence is a ctest failure rather than a discovery.
set -uo pipefail

# Device-side paths must reach adb verbatim under Git bash; host paths still
# need MSYS conversion. See tools/android-e2e.sh for the measured failure.
export MSYS2_ARG_CONV_EXCL='/sdcard;/data'

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA="${1:-$ROOT/libhunmin/data}"

# Every prerequisite is a skip, not a failure: this script is registered on
# hosts that have no Android SDK at all, and CMAKE_TOOLCHAIN_FILE is read on the
# first configure only, so the test stays registered for the life of the build
# dir even after the environment goes away. `set -u` plus an explicit test is
# the way to say that; `:?` would abort with 1, which ctest reads as red.
[ -n "${ANDROID_NDK:-}" ] || { echo "skip: ANDROID_NDK unset" >&2; exit 77; }
[ -n "${ANDROID_HOME:-}" ] || { echo "skip: ANDROID_HOME unset" >&2; exit 77; }
ADB="$ANDROID_HOME/platform-tools/adb"
[ -x "$ADB" ] || ADB="$(command -v adb || true)"
[ -n "${ADB:-}" ] && [ -x "$ADB" ] || { echo "skip: no adb" >&2; exit 77; }

"$ADB" get-state >/dev/null 2>&1 || {
  echo "skip: no Android device attached" >&2; exit 77; }
api=$("$ADB" shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r')
[ -n "$api" ] || { echo "skip: device did not report an API level" >&2; exit 77; }
abi=$("$ADB" shell uname -m 2>/dev/null | tr -d '\r')
echo "device: API $api, $abi"

BUILD="$ROOT/build-android/$abi"
# -f, not -x: Windows checkout filesystems (NTFS via MSYS) report the Android
# ELF as non-executable, and chmod cannot change that. The executable bit that
# matters is set on the device with chmod below.
if [ ! -f "$BUILD/jniLibs/$abi/hunmin-sim" ]; then
  echo "skip: no $abi build; run tools/build-android.sh $abi first" >&2
  exit 77
fi

STAGE=$("$ADB" shell mktemp -d /data/local/tmp/hm-gold.XXXXXX | tr -d '\r')
trap '"$ADB" shell rm -rf "$STAGE" >/dev/null 2>&1' EXIT
"$ADB" push "$BUILD/jniLibs/$abi/hunmin-sim" "$STAGE/sim" >/dev/null
"$ADB" push "$BUILD/jniLibs/$abi/libhunmin.so" "$STAGE/" >/dev/null
"$ADB" push "$DATA" "$STAGE/data" >/dev/null
"$ADB" push "$ROOT/tests/golden" "$STAGE/golden" >/dev/null
"$ADB" shell chmod 755 "$STAGE/sim" >/dev/null

pass=0; fail=0; failed=""
for g in "$ROOT"/tests/golden/*.jsonl; do
  name=$(basename "$g" .jsonl)
  # Two kinds of corpus live in tests/golden: the per-key engine transcripts,
  # replayed through hunmin_ic with --check, and the judgement corpus, whose
  # cases drive hunmin_buffered_preedit with --judge-check (the judgement is the
  # shared layer's, docs/FRONTEND-EQUIVALENCE.md §6.2). Same binary, same data
  # push; only the flag differs.
  case "$name" in
    judgement-*) check=--judge-check ;;
    *)           check=--check ;;
  esac
  out=$("$ADB" shell "cd $STAGE && LD_LIBRARY_PATH=. ./sim --data data \
        $check golden/$name.jsonl" 2>&1 | tr -d '\r' | tail -1)
  case "$out" in
    OK*) pass=$((pass + 1)); printf '  ok   %s\n' "$name" ;;
    *)   fail=$((fail + 1)); failed="$failed $name"
         printf '  FAIL %s\n    %s\n' "$name" "$out" ;;
  esac
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ] || { echo "goldens disagree on $abi:" >&2; exit 1; }