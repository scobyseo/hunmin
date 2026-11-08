#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Check that every native library the Android app ships is 16KB-aligned.

WHY THIS EXISTS
---------------
2026-10-11: the Hanja dialog opened and the platform reported
"다음 라이브러리가 16KB로 정렬되지 않았습니다. libhunjni.so libhunmin.so".
The cause was a two-build-system split: libhunmin.so is built by
tools/build-android.sh via cmake/android.toolchain.cmake, which sets
`-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384`, but libhunjni.so
is built by AGP's externalNativeBuild from frontends/android/app/src/main/cpp,
a separate CMake project that does NOT inherit the toolchain file. The shim
therefore linked at the default 4KB page size, and the Android 15+ 16KB check
covers *every* library the process loads, so one 4KB library made the whole
install report both as unaligned.

That failure was invisible on the build machine: it compiles, it links, it
packages, it installs. It only surfaces on a 16KB-page device at runtime.
So the check is mechanical, like docs-lint's.

WHAT IT CHECKS
--------------
For each lib/arm64-v8a/*.so and lib/x86_64/*.so inside the debug APK:

  1. Every PT_LOAD program header has p_align == 0x4000 (16384). A 0x1000
     (4096) align is exactly the regression this was written for.
  2. zipalign -c -P 16 accepts the APK, i.e. the stored offsets are 16KB
     aligned too. This is a different property from the ELF header align:
     zipalign can pass while the ELF is 4KB, and vice versa, and the runtime
     check needs the ELF one.

BOTH ARE NEEDED. They are separate mechanisms and neither implies the other.

SKIP, DELIBERATELY
------------------
When the APK is absent the test exits 77, which ctest reads as SKIP_RETURN_CODE.
The inner loop (-LE slow) builds no APK, and a gate that fails because an
artefact was not built is a gate that gets switched off. The APK is built by
tools/build-android.sh + ./gradlew assembleDebug in the Android tier; run this
after those.

llvm-readelf is located from ANDROID_NDK when set, else from the default SDK
path, else from a binutils readelf on PATH. If none is found the test skips
rather than reporting a green pass it did not perform.
"""
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

SKIP = 77
EXPECT_ALIGN = 0x4000

try:
    from _common import ROOT as REPO
except ImportError:
    REPO = Path(__file__).resolve().parent.parent
APK = (REPO / "frontends/android/app/build/outputs/apk/debug/app-debug.apk")


def find_readelf() -> str | None:
    """llvm-readelf from the NDK first (it reads any ELF), else binutils."""
    candidates = []
    ndk = os_env("ANDROID_NDK")
    if ndk:
        candidates.append(
            Path(ndk) / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf")
    sdk = os_env("ANDROID_HOME") or str(Path.home() / "Android/Sdk")
    for ndk_dir in sorted(Path(sdk).glob("ndk/*"), reverse=True):
        candidates.append(
            ndk_dir / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf")
    for c in candidates:
        if c.is_file() and os_access(c):
            return str(c)
    return shutil.which("readelf") or shutil.which("llvm-readelf")


def os_env(name: str) -> str | None:
    import os
    v = os.environ.get(name)
    return v if v else None


def os_access(p: Path) -> bool:
    import os
    return os.access(p, os.X_OK)


def find_zipalign() -> str | None:
    sdk = os_env("ANDROID_HOME") or str(Path.home() / "Android/Sdk")
    for bt in sorted(Path(sdk).glob("build-tools/*/zipalign"), reverse=True):
        return str(bt)
    return shutil.which("zipalign")


def load_alignments(readelf: str, so: Path) -> list[int]:
    """p_align of every PT_LOAD segment, from `readelf -lW`."""
    out = subprocess.run([readelf, "-lW", str(so)],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError(f"readelf failed on {so.name}: {out.stderr.strip()}")
    aligns = []
    for line in out.stdout.splitlines():
        f = line.split()
        # "  LOAD  0x... 0x... 0x... 0x... 0x... R E 0x4000"
        if len(f) >= 2 and f[0] == "LOAD":
            aligns.append(int(f[-1], 16))
    return aligns


def main() -> int:
    if not APK.is_file():
        print(f"align-16k: no APK at {APK}; build it first "
              f"(tools/build-android.sh <abi> && ./gradlew assembleDebug)")
        return SKIP

    readelf = find_readelf()
    zipalign = find_zipalign()
    if not readelf:
        print("align-16k: no readelf found (ANDROID_NDK unset, none on PATH)")
        return SKIP

    failures: list[str] = []
    checked = 0

    with tempfile.TemporaryDirectory(prefix="hunmin-align16k-") as td:
        with zipfile.ZipFile(APK) as zf:
            names = [n for n in zf.namelist()
                     if n.startswith("lib/") and n.endswith(".so")]
            if not names:
                print("align-16k: APK contains no native libraries")
                return 1
            zf.extractall(td, names)

        for name in sorted(names):
            so = Path(td) / name
            checked += 1
            try:
                aligns = load_alignments(readelf, so)
            except RuntimeError as e:
                failures.append(str(e))
                continue
            if not aligns:
                failures.append(f"{name}: no PT_LOAD segment found")
                continue
            bad = [a for a in aligns if a != EXPECT_ALIGN]
            if bad:
                got = " ".join(hex(a) for a in aligns)
                failures.append(
                    f"{name}: PT_LOAD align = {got}, expected "
                    f"{hex(EXPECT_ALIGN)} on every segment "
                    f"(a {hex(0x1000)} is the 4KB default -- the regression "
                    f"this check exists for)")

        # zipalign -P 16 covers the stored offsets, which the ELF align does not.
        # Usage: `zipalign -c -P <pagesize_kb> <align> infile.zip` -- the <align>
        # positional (4 = 32-bit) is still required alongside -P, and omitting
        # it prints the usage text and exits non-zero.
        if zipalign:
            r = subprocess.run([zipalign, "-c", "-P", "16", "4", str(APK)],
                               capture_output=True, text=True)
            if r.returncode != 0:
                failures.append(
                    f"APK failed `zipalign -c -P 16`:\n{r.stdout}{r.stderr}".strip())
        else:
            print("align-16k: zipalign not found; ELF alignment checked only")

    if failures:
        print(f"align-16k: {len(failures)} failure(s) across {checked} librar"
              f"{'y' if checked == 1 else 'ies'}:")
        for f in failures:
            print(f"  - {f}")
        print("\nlibhunmin.so gets its flags from cmake/android.toolchain.cmake; "
              "libhunjni.so is built by AGP's externalNativeBuild and needs the "
              "same -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384 in "
              "frontends/android/app/src/main/cpp/CMakeLists.txt.")
        return 1

    print(f"align-16k: ok — {checked} native librar"
          f"{'y' if checked == 1 else 'ies'} 16KB-aligned, "
          f"zipalign -P 16 {'verified' if zipalign else 'not run'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
