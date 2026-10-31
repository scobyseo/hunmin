#!/usr/bin/env bash
# Build the Windows 0.2 installer (issue #99): the release package a user
# installs by double-clicking, replacing the dev-period regsvr32-over-a-
# build-tree staging.
#
# End to end, under build-msvc/installer/ (gitignored):
#   1. the core, MSVC Release, with the CRT statically linked
#      (-DHUNMIN_MSVC_STATIC_RUNTIME=ON: no VC redist on a clean image —
#      the clean-VM field test's root cause, issue #99 comment 2)
#   2. the TIP, configured standalone (its documented build shape), linked
#      against that static engine lib
#   3. a dumpbin gate: the packaged DLL must import system DLLs only
#   4. a self-contained publish of the settings exe (issue #71's shape)
#   5. the stage: DLL + libhunmin/data + settings-ui
#   6. Inno Setup 6 compile of frontends/tsf/installer/hunmin.iss
#   7. the size budget the issue asks to be measured at packaging time
#
# The installer itself runs regsvr32 on the installed DLL and mirrors it on
# uninstall (issue #84's DllUnregisterServer). The clean-machine round
# (fresh VM / Windows Sandbox) is tests/manual-checklists/tsf-installer-
# fresh-vm.md and stays outside this script.
#
# Host requirements (the toolchain's home is docs/ENVIRONMENT.md):
#   - Visual Studio BuildTools (vcvarsall.bat), for the MSVC TIP build
#   - a dotnet SDK, for the settings-ui publish
#   - Inno Setup 6 (ISCC.exe; override with ISCC= if installed elsewhere)
#
# The VS generator and the vcvars location are both HOST facts, so both are
# overridable: HUNMIN_CMAKE_GENERATOR (default the VS 17 2022 generator) and
# HUNMIN_VS2022_ROOT (the VS install root, any VS version). Measured 2026-10-30
# on a host whose only VS is "18 2026" BuildTools: the pinned generator fails
# at configure with "could not find any instance", before any build starts.

set -euo pipefail
cd "$(dirname "$0")/.."

BUILD=build-msvc
TSF_BUILD="frontends/tsf/$BUILD"
OUT="$BUILD/installer"
STAGE="$OUT/stage"
APP_VERSION=0.2.0
GENERATOR="${HUNMIN_CMAKE_GENERATOR:-Visual Studio 17 2022}"

# --- toolchain locations: env override first, then the known shapes ------
VSVARS="${HUNMIN_VS2022_ROOT:-}/VC/Auxiliary/Build/vcvarsall.bat"
if [ ! -f "$VSVARS" ]; then
  for c in "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools" \
           "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools" \
           "C:/Program Files/Microsoft Visual Studio/2022/Community"; do
    [ -f "$c/VC/Auxiliary/Build/vcvarsall.bat" ] && VSVARS="$c/VC/Auxiliary/Build/vcvarsall.bat" && break
  done
fi
[ -f "$VSVARS" ] || { echo "vcvarsall.bat not found; set HUNMIN_VS2022_ROOT" >&2; exit 1; }

ISCC="${ISCC:-}"
if [ -z "$ISCC" ]; then
  for c in "C:/Program Files (x86)/Inno Setup 6/ISCC.exe" \
           "C:/Program Files/Inno Setup 6/ISCC.exe" \
           "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"; do
    [ -f "$c" ] && ISCC="$c" && break
  done
fi
[ -n "$ISCC" ] && [ -f "$ISCC" ] || { echo "ISCC.exe not found; install Inno Setup 6 or set ISCC=" >&2; exit 1; }

# vcvars is batch-native: quoting the vcvars line through bash -> cmd.exe
# mangles the nested quotes (measured: cmd starts interactive and runs
# nothing). Generate the scratch .bat instead — its cd /d line pins it to
# THIS worktree, and it lives under the gitignored build dir, so no machine
# path enters the repository.
BAT="$OUT/build-pkg.bat"
mkdir -p "$OUT"
{
  echo '@echo off'
  echo "cd /d \"$(cygpath -w "$PWD")\""
  echo "call \"$(cygpath -w "$VSVARS")\" x64"
  echo 'if errorlevel 1 exit /b 1'
  echo 'echo == core: MSVC Release, static CRT =='
  echo "cmake -S . -B build-msvc -G \"$GENERATOR\" -A x64 -DHUNMIN_MSVC_STATIC_RUNTIME=ON"
  echo 'if errorlevel 1 exit /b 1'
  echo 'cmake --build build-msvc --config Release --target hunmin_static'
  echo 'if errorlevel 1 exit /b 1'
  echo 'echo == TIP: standalone MSVC Release, static CRT =='
  echo "cmake -S frontends\\tsf -B frontends\\tsf\\build-msvc -G \"$GENERATOR\" -A x64 -DHUNMIN_MSVC_STATIC_RUNTIME=ON"
  echo 'if errorlevel 1 exit /b 1'
  echo 'cmake --build frontends\tsf\build-msvc --config Release --target hunmin-tsf'
  echo 'if errorlevel 1 exit /b 1'
  echo 'echo == dumpbin gate: system DLLs only =='
  echo 'dumpbin -DEPENDENTS frontends\tsf\build-msvc\Release\hunmin-tsf.dll'
  echo 'if errorlevel 1 exit /b 1'
} > "$BAT"
# cmd.exe's batch parser mis-splits LF-only files (measured: "'build-msvc' is
# not recognized" with the call line never running). CRLF endings, always.
sed -i 's/$/\r/' "$BAT"

echo "== 1-3/7 core + TIP (MSVC Release, static CRT) + dumpbin gate =="
# Backslashes for cmd: with a forward-slash path its switch parser mangles
# the argument (measured: the first path component after the drive gets
# executed as a command).
cmd /c "$(cygpath -w "$BAT")" > "$OUT/build-pkg.log" 2>&1 || {
  tail -40 "$OUT/build-pkg.log" >&2
  exit 1
}
tail -25 "$OUT/build-pkg.log"

TIP_DLL="$TSF_BUILD/Release/hunmin-tsf.dll"
[ -f "$TIP_DLL" ] || { echo "hunmin-tsf.dll did not land at $TIP_DLL (see $OUT/build-pkg.log)" >&2; exit 1; }

if grep -Ei "VCRUNTIME14|MSVCP14|MSVCR14" "$OUT/build-pkg.log"; then
  echo "FAIL: the packaged DLL imports the dynamic VC runtime (the clean-VM load failure)" >&2
  exit 1
fi

# The static lib's landing dir differs per generator; the TIP's
# find_library searches the VS shapes itself — report the measured path.
STATIC_LIB=$(ls "$BUILD/Release/hunmin_static.lib" \
                "$BUILD/libhunmin/Release/hunmin_static.lib" 2>/dev/null | head -1 || true)
if [ -n "$STATIC_LIB" ]; then
  echo "static engine lib: $STATIC_LIB"
fi

echo "== 4/7 settings-ui publish (self-contained) =="
# Clean stage FIRST (a leftover tree from an earlier run would nest the
# next copy INSIDE it — cp -r src dst with dst present copies into dst;
# measured: data/data with every file twice), then publish into it.
rm -rf "$STAGE"
dotnet publish frontends/tsf/settings-ui/HunminTsfSettingsUI.csproj -c Release -o "$(cygpath -m "$PWD/$STAGE/settings-ui")"

echo "== 5/7 stage =="
mkdir -p "$STAGE"
cp "$TIP_DLL" "$STAGE/"
cp -r libhunmin/data "$STAGE/data"

echo "== 6/7 Inno Setup compile =="
"$ISCC" /DAppVersion=$APP_VERSION \
        /DStageDir="$(cygpath -m "$PWD/$STAGE")" \
        /O"$(cygpath -m "$PWD/$OUT")" \
        /Fhunmin-$APP_VERSION-setup \
        frontends/tsf/installer/hunmin.iss

echo "== 7/7 size budget (measured at packaging time) =="
{
  echo "hunmin $APP_VERSION package, measured $(date +%F) by tools/build-windows-installer.sh"
  du -sk "$STAGE/hunmin-tsf.dll" "$STAGE/data" "$STAGE/data/krdict" "$STAGE/settings-ui" | awk '{printf "%10.1f MB  %s\n", $1/1024, $2}'
  ls -l "$OUT/hunmin-$APP_VERSION-setup.exe" | awk '{printf "%10.1f MB  the installer\n", $5/1048576}'
} | tee "$OUT/SIZE.txt"

echo "OK: $OUT/hunmin-$APP_VERSION-setup.exe"
