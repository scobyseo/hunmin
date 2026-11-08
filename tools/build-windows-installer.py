#!/usr/bin/env python3
# Build the Windows 0.3.0 installer (issue #99): the release package a user
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
# Host requirements (the toolchain's home is docs/ENVIRONMENT.md):
#   - Visual Studio BuildTools (vcvarsall.bat), for the MSVC TIP build
#   - a dotnet SDK, for the settings-ui publish
#   - Inno Setup 6 (ISCC.exe; override with ISCC= if installed elsewhere)
#
# The VS generator and the vcvars location are both HOST facts, so both are
# overridable: HUNMIN_CMAKE_GENERATOR (default the VS 17 2022 generator) and
# HUNMIN_VS2022_ROOT (the VS install root, any VS version).

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

APP_VERSION = "0.3.0"
GENERATOR = os.environ.get("HUNMIN_CMAKE_GENERATOR", "Visual Studio 17 2022")

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-msvc"
TSF_BUILD = ROOT / "frontends" / "tsf" / "build-msvc"
OUT = BUILD / "installer"
STAGE = OUT / "stage"


def win_path(p):
    """Convert an MSYS/POSIX path to a Windows native path via cygpath,
    falling back to a manual /c/... -> C:\... conversion when cygpath is
    unavailable (plain Windows Python)."""
    s = str(p)
    # Already a Windows native path?
    if re.match(r"^[A-Za-z]:[\\/]", s):
        return s.replace("/", "\\")
    # MSYS form: /c/... -> C:\...
    m = re.match(r"^/([a-zA-Z])/(.*)$", s)
    if m:
        return f"{m.group(1)}:\\{m.group(2).replace('/', '\\')}"
    try:
        r = subprocess.run(["cygpath", "-w", s], capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.strip()
    except FileNotFoundError:
        pass
    return s


def find_vcvars():
    root = os.environ.get("HUNMIN_VS2022_ROOT")
    candidates = []
    if root:
        candidates.append(Path(root) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat")
    candidates += [
        Path("C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvarsall.bat"),
        Path("C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvarsall.bat"),
        Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsall.bat"),
    ]
    for c in candidates:
        if c.exists():
            return str(c)
    sys.exit("vcvarsall.bat not found; set HUNMIN_VS2022_ROOT")


def find_iscc():
    iscc = os.environ.get("ISCC")
    if iscc and Path(iscc).exists():
        return iscc
    candidates = [
        Path("C:/Program Files (x86)/Inno Setup 6/ISCC.exe"),
        Path("C:/Program Files/Inno Setup 6/ISCC.exe"),
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Inno Setup 6" / "ISCC.exe",
    ]
    for c in candidates:
        if c.exists():
            return str(c)
    sys.exit("ISCC.exe not found; install Inno Setup 6 or set ICC=")


def run_in_vcvars(cmd_parts, log=None):
    """Run cmd_parts inside a shell where vcvarsall.bat x64 has been called."""
    vcvars = win_path(find_vcvars())
    quoted = " ".join(f'"{c}"' if " " in str(c) else str(c) for c in cmd_parts)
    full = f'cmd /c "call "{vcvars}" x64 && {quoted}"'
    if log:
        with open(log, "a", encoding="utf-8", errors="replace") as lf:
            lf.write(f"> {full}\n")
    result = subprocess.run(full, shell=True, cwd=win_path(ROOT),
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if log:
        with open(log, "a", encoding="utf-8", errors="replace") as lf:
            lf.write(result.stdout)
            lf.write(result.stderr)
    return result


def log_tail(log_path, n):
    if not log_path.exists():
        return []
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    return lines[-n:]


def main():
    os.chdir(ROOT)

    vcvars = find_vcvars()
    iscc = find_iscc()
    print(f"vcvars: {vcvars}")
    print(f"ISCC: {iscc}")

    OUT.mkdir(parents=True, exist_ok=True)
    log = OUT / "build-pkg.log"

    cmake = shutil.which("cmake")
    if not cmake:
        sys.exit("cmake not found on PATH")

    # --- Steps 1-3: core + TIP + dumpbin gate --------------------------
    print("== 1-3/7 core + TIP (MSVC Release, static CRT) + dumpbin gate ==")

    # Configure core
    r = run_in_vcvars([
        cmake, "-S", ".", "-B", win_path(BUILD), "-G", GENERATOR, "-A", "x64",
        "-DHUNMIN_MSVC_STATIC_RUNTIME=ON"
    ], log=log)
    if r.returncode != 0:
        for line in log_tail(log, 40):
            print(line)
        sys.exit(1)

    # Build core
    r = run_in_vcvars([
        cmake, "--build", win_path(BUILD), "--config", "Release", "--target", "hunmin_static"
    ], log=log)
    if r.returncode != 0:
        for line in log_tail(log, 40):
            print(line)
        sys.exit(1)

    # Configure TIP
    r = run_in_vcvars([
        cmake, "-S", "frontends/tsf", "-B", win_path(TSF_BUILD),
        "-G", GENERATOR, "-A", "x64", "-DHUNMIN_MSVC_STATIC_RUNTIME=ON"
    ], log=log)
    if r.returncode != 0:
        for line in log_tail(log, 40):
            print(line)
        sys.exit(1)

    # Build TIP
    r = run_in_vcvars([
        cmake, "--build", win_path(TSF_BUILD), "--config", "Release", "--target", "hunmin-tsf"
    ], log=log)
    if r.returncode != 0:
        for line in log_tail(log, 40):
            print(line)
        sys.exit(1)

    # dumpbin gate
    tip_dll = TSF_BUILD / "Release" / "hunmin-tsf.dll"
    dumpbin = shutil.which("dumpbin")
    if dumpbin:
        r = run_in_vcvars([dumpbin, "-DEPENDENTS", win_path(tip_dll)], log=log)
        if r.returncode != 0:
            for line in log_tail(log, 40):
                print(line)
            sys.exit(1)
    else:
        print("warning: dumpbin not found on PATH, skipping dependency gate")

    for line in log_tail(log, 25):
        print(line)

    log_text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    if re.search(r"VCRUNTIME14|MSVCP14|MSVCR14", log_text, re.I):
        sys.exit("FAIL: the packaged DLL imports the dynamic VC runtime (the clean-VM load failure)")

    # Find the static lib (landing dir differs per generator)
    static_lib = None
    for cand in [BUILD / "Release" / "hunmin_static.lib",
                 BUILD / "libhunmin" / "Release" / "hunmin_static.lib"]:
        if cand.exists():
            static_lib = cand
            break
    if static_lib:
        print(f"static engine lib: {static_lib}")

    # --- Step 4: settings-ui publish (self-contained) ------------------
    print("== 4/7 settings-ui publish (self-contained) ==")
    if STAGE.exists():
        shutil.rmtree(STAGE)
    dotnet = shutil.which("dotnet")
    if not dotnet:
        sys.exit("dotnet not found on PATH")
    settings_ui_proj = ROOT / "frontends" / "tsf" / "settings-ui" / "HunminTsfSettingsUI.csproj"
    r = subprocess.run([
        dotnet, "publish", str(settings_ui_proj),
        "-c", "Release", "-o", win_path(STAGE / "settings-ui")
    ], cwd=win_path(ROOT))
    if r.returncode != 0:
        sys.exit(1)

    # --- Step 5: stage -------------------------------------------------
    print("== 5/7 stage ==")
    STAGE.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(tip_dll), str(STAGE / "hunmin-tsf.dll"))
    shutil.copytree(str(ROOT / "libhunmin" / "data"), str(STAGE / "data"), dirs_exist_ok=True)

    # --- Step 6: Inno Setup compile ------------------------------------
    print("== 6/7 Inno Setup compile ==")
    iss = ROOT / "frontends" / "tsf" / "installer" / "hunmin.iss"
    setup_name = f"hunmin-{APP_VERSION}-setup"
    r = subprocess.run([
        iscc,
        f"/DAppVersion={APP_VERSION}",
        f"/DStageDir={win_path(STAGE)}",
        f"/O{win_path(OUT)}",
        f"/F{setup_name}",
        str(iss)
    ], cwd=win_path(ROOT))
    if r.returncode != 0:
        sys.exit(1)

    # --- Step 7: size budget -------------------------------------------
    print("== 7/7 size budget (measured at packaging time) ==")
    setup_exe = OUT / f"hunmin-{APP_VERSION}-setup.exe"
    lines = [f"hunmin {APP_VERSION} package, measured by tools/build-windows-installer.py"]
    for p in [STAGE / "hunmin-tsf.dll", STAGE / "data", STAGE / "data" / "krdict", STAGE / "settings-ui"]:
        if p.exists():
            size_mb = sum(f.stat().st_size for f in p.rglob("*") if f.is_file()) / 1048576
            lines.append(f"{size_mb:10.1f} MB  {p}")
    if setup_exe.exists():
        size_mb = setup_exe.stat().st_size / 1048576
        lines.append(f"{size_mb:10.1f} MB  the installer")

    size_txt = OUT / "SIZE.txt"
    size_txt.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))

    print(f"OK: {setup_exe}")


if __name__ == "__main__":
    main()
