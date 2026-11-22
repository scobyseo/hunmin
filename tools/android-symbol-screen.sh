#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# android-symbol-screen.sh -- put the symbol screen's data on a device where a
# person can look at it, and set the IME up so typing works while they do.
#
#   tools/android-symbol-screen.sh [--serial SERIAL] [--build] [--no-install]
#                                  [--launch]
#
# Exit 0 ok, 1 fail, 77 skip (no SDK / no usable device / no APK).
#
# WHY THIS EXISTS. Issue #297's deliverable is data, not a screen: the app reads
# `assets/keypad/symbols.json` and the soft-keyboard UI that draws it is a
# separate sub-issue of #285. So there is nothing to tap yet, and "test it on a
# device" would otherwise mean reading a JSON file on a host. This script
# narrows that gap to what is actually assertable today:
#
#   1. the asset really is inside the INSTALLED apk (unzip the pulled apk, not
#      the build tree -- a sync task that wrote into src/main/assets and never
#      packaged it is the exact failure docs/DESIGN-BUILD.md records);
#   2. the copy on the device is byte-identical to the committed file
#      (sha256 against the repo's own);
#   3. the screen renders as the array the app will draw -- KP6's three ruled
#      rows, the navigation row, the KP9 bottom row -- from the DEVICE copy, so
#      what is printed is what the device holds;
#   4. the IME is installed, enabled and selected, and the test editor is
#      launched so hardware typing can be checked for regressions.
#
# What it deliberately does NOT do: draw the symbol grid as touchable keys. That
# is presentation, owned by the keypad-UI sub-issue, and inventing it here would
# be a second home for the layout the data already defines.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="org.hunmin.ime"
SVC="$PKG/.HunminInputMethodService"
EDITOR_PKG="org.hunmin.testeditor"
EDITOR_ACTIVITY="$EDITOR_PKG/.EditorActivity"
ASSET="keypad/symbols.json"
REPO_ASSET="$ROOT/frontends/android/app/src/main/assets/$ASSET"

SERIAL="${ANDROID_SERIAL:-}"
DO_BUILD=0
DO_INSTALL=1
DO_LAUNCH=0
while [ $# -gt 0 ]; do
    case "$1" in
        --serial)    SERIAL="${2:?--serial needs a value}"; shift 2 ;;
        --build)     DO_BUILD=1; shift ;;
        --no-install) DO_INSTALL=0; shift ;;
        --launch)    DO_LAUNCH=1; shift ;;
        -h|--help)   sed -n '2,40p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

skip() { echo "skip: $*" >&2; exit 77; }
fail() { echo "FAIL: $*" >&2; exit 1; }
say()  { printf '\n== %s\n' "$*"; }

: "${ANDROID_HOME:=$HOME/Android/Sdk}"
ADB="$ANDROID_HOME/platform-tools/adb"
[ -x "$ADB" ] || skip "no adb at $ADB (set ANDROID_HOME)"

# --- pick one device, loudly -------------------------------------------------
# `adb devices` exits 0 while a phone sits unauthorized, and every later command
# then answers "more than one device/emulator" (or worse, picks the one the user
# is not looking at). So the choice is explicit and printed.
if [ -n "$SERIAL" ]; then
    "$ADB" -s "$SERIAL" get-state >/dev/null 2>&1 \
        || fail "device '$SERIAL' is not available (adb devices -l)"
else
    ready="$("$ADB" devices | awk 'NR>1 && $2=="device" {print $1}')"
    unauth="$("$ADB" devices | awk 'NR>1 && $2=="unauthorized" {print $1}')"
    n_ready=$(printf '%s\n' "$ready" | grep -c . || true)
    if [ "$n_ready" -eq 0 ]; then
        [ -n "$unauth" ] && fail "no authorized device. Unlock the phone and accept
       the 'Allow USB debugging' prompt, then re-run. Unauthorized: $unauth"
        skip "no device attached (start an AVD, or plug in a phone)"
    fi
    if [ "$n_ready" -gt 1 ]; then
        fail "more than one device is ready; pick one with --serial or
       ANDROID_SERIAL:
$(printf '%s\n' "$ready" | sed 's/^/         /')"
    fi
    SERIAL="$ready"
fi
[ -n "${unauth:-}" ] && echo "note: ignoring unauthorized device(s): $unauth"
ADB() { "$ADB" -s "$SERIAL" "$@"; }
export ANDROID_SERIAL="$SERIAL"
echo "device: $SERIAL ($(ADB shell getprop ro.product.model | tr -d '\r'))"

# Push the committed asset so the device and the repo can be compared even when
# no APK is installed.
[ -f "$REPO_ASSET" ] || fail "missing $REPO_ASSET"

# --- build / install ---------------------------------------------------------
APK=$(find "$ROOT/frontends/android/app/build/outputs/apk/debug" -maxdepth 1 \
        -name 'hunmin-*.apk' 2>/dev/null | head -1)
EDITOR_APK="$ROOT/frontends/android/testeditor/build/outputs/apk/debug/testeditor-debug.apk"
if [ "$DO_BUILD" -eq 1 ]; then
    say "build"
    Java_Home="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
    [ -x "$Java_Home/bin/java" ] || fail "no JDK (export JAVA_HOME)"
    ( cd "$ROOT/frontends/android" \
        && JAVA_HOME="$Java_Home" ANDROID_HOME="$ANDROID_HOME" \
           ./gradlew --no-daemon :app:assembleDebug :testeditor:assembleDebug ) \
        || fail "gradlew assembleDebug failed"
    APK=$(find "$ROOT/frontends/android/app/build/outputs/apk/debug" -maxdepth 1 \
        -name 'hunmin-*.apk' 2>/dev/null | head -1)
fi
[ -n "$APK" ] && [ -f "$APK" ] || fail "no APK yet: re-run with --build"

if [ "$DO_INSTALL" -eq 1 ]; then
    say "install"
    ADB install -r -g "$APK" >/dev/null || fail "install of $APK failed"
    # The grader app is optional: without it there is still a running IME, just
    # no fixed text field to type into.
    if [ -f "$EDITOR_APK" ]; then
        ADB install -r -g "$EDITOR_APK" >/dev/null || echo "warn: test editor install failed"
    else
        echo "note: no test editor APK (build with --build); open any text field instead"
    fi
    # An IME the platform has not enabled AND selected types nothing, and the
    # failure looks like "the keyboard is broken" rather than "it is not chosen".
    # force-stop comes FIRST: stopping the IME *after* selecting it makes the
    # platform fall back to the previous keyboard, measured 2026-11-22 on the
    # emulator (reverted to Gboard) and on an SM-S938N (reverted to Honeyboard),
    # so this script reported success and left the wrong IME selected.
    ADB shell am force-stop "$PKG" >/dev/null 2>&1 || true
    ADB shell ime enable "$SVC" >/dev/null || fail "cannot enable $SVC"
    ADB shell ime set "$SVC" >/dev/null 2>&1 || fail "cannot select $SVC"
    ADB shell ime list -s | tr -d '\r' | grep -qx "$SVC" \
        || fail "$SVC is not the selected IME"
    # Ask the platform, not the settings row: on a device where another IME owns
    # the default, `ime set` answers success and the default quietly reverts.
    current=$(ADB shell settings get secure default_input_method | tr -d '\r')
    [ "$current" = "$SVC" ] || fail "the default IME is '$current', not $SVC"
    echo "IME enabled and selected: $SVC"
fi

# --- the data, from the device ----------------------------------------------
say "the app-read asset, as the device holds it"

# Which apk is installed, and is the asset inside it? Pulled and read with the
# HOST's unzip: the device copy of an apk is the only thing the platform will
# load, so a build-tree file proves nothing about what runs.
INSTALLED_APK=$(ADB shell pm path "$PKG" 2>/dev/null | tr -d '\r' | sed -n 's/^package://p' | head -1)
if [ -n "$INSTALLED_APK" ]; then
    tmp_apk=$(mktemp "${TMPDIR:-/tmp}/hunmin-apk.XXXXXX")
    if ADB pull "$INSTALLED_APK" "$tmp_apk" >/dev/null 2>&1; then
        if command -v unzip >/dev/null 2>&1; then
            if unzip -p "$tmp_apk" "assets/$ASSET" > "${tmp_apk%.apk}.asset" 2>/dev/null \
               && [ -s "${tmp_apk%.apk}.asset" ]; then
                device_asset="${tmp_apk%.apk}.asset"
                echo "packaged in the installed apk: assets/$ASSET"
            else
                echo "NOT PACKAGED: the installed apk has no assets/$ASSET"
            fi
        else
            echo "note: no unzip on the host; skipping the packaged-asset check"
        fi
    else
        echo "note: could not pull $INSTALLED_APK; skipping the packaged-asset check"
    fi
fi

if [ -z "${device_asset:-}" ]; then
    # Fall back to a pushed copy so the render still comes from the device.
    device_asset="/data/local/tmp/hunmin-$ASSET"
    ADB shell "mkdir -p /data/local/tmp/hunmin-keypad" >/dev/null 2>&1 || true
    ADB push "$REPO_ASSET" "/data/local/tmp/hunmin-symbols.json" >/dev/null \
        || fail "could not push the asset"
    device_asset=$(mktemp "${TMPDIR:-/tmp}/hunmin-pushed.XXXXXX")
    ADB pull "/data/local/tmp/hunmin-symbols.json" "$device_asset" >/dev/null \
        || fail "could not pull the pushed asset back"
    echo "note: read from a pushed copy (the apk's own asset was not found)"
fi

# Byte-identity against the committed file. This is the check that catches a
# stale second copy of layout data, the failure AGENTS.md's "layouts are data,
# never code" rule exists to prevent.
host_sum=$(sha256sum "$REPO_ASSET" | awk '{print $1}')
dev_sum=$(sha256sum "$device_asset" | awk '{print $1}')
if [ "$host_sum" = "$dev_sum" ]; then
    echo "sha256 matches the repo: $host_sum"
else
    echo "MISMATCH device=$dev_sum repo=$host_sum"
    fail "the device's symbol data is not the committed data"
fi

# Render the screen from the device copy. Text, not pixels: the drawing code is
# a separate sub-issue, and this is the array it will be given.
python3 - "$device_asset" <<'PY' || fail "could not render the asset"
import json, sys

LABEL = {
    "tab": "[Tab]", "home": "[Home]", "page-up": "[PgUp]",
    "page-down": "[PgDn]", "end": "[End]", "backspace": "[⌫]",
    "language-toggle": "[한/영]", "left": "[←]", "up": "[↑]",
    "down": "[↓]", "right": "[→]",
}
data = json.load(open(sys.argv[1], encoding="utf-8"))
print(f"  {data['id']} (schemaVersion {data['schemaVersion']})")
for row in data["rows"]:
    if row["kind"] == "symbols":
        # Each symbol is one cell; the unshifted/shifted partners sit adjacent
        # by the KP6 ruling, so the row is printed as characters in order.
        print("  " + " ".join(row["keys"]))
    else:
        print("  " + " ".join(LABEL.get(k, f"[{k}]") for k in row["keys"]))
PY

if [ "$DO_LAUNCH" -eq 1 ]; then
    say "launch the test editor"
    ADB shell pm clear "$EDITOR_PKG" >/dev/null 2>&1 || true
    ADB shell am start -n "$EDITOR_ACTIVITY" >/dev/null 2>&1 \
        || echo "note: no test editor installed; open any text field"
    sleep 2
    ADB shell input tap 540 900 >/dev/null 2>&1 || true
    echo "type on the emulator/phone: the IME is '$SVC'"
fi

say "how to look at it by hand"
cat <<'EOF'
  The symbol screen has no UI yet (that is the keypad-UI sub-issue of #285), so
  what can be checked on the device today is the data above plus IME health:
    - tap a text field on the device and type: hardware typing still works;
    - `adb shell dumpsys input_method | grep -i mCurMethodId` names the IME;
    - re-run this script after editing the JSON to see the screen change.
  The grid as drawn (geometry, targets, the frequent-symbol keys) is the UI
  sub-issue's; this script deliberately does not preview a layout it does not own.
EOF