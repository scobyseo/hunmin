# hunmin — Development Environment

Audited 2026-09-26 on `~/work` host (Ubuntu "resolute", gcc 15, cmake).
Re-audited 2026-10-02 while un-skipping tier D3; deltas noted inline.

**Already present:** `/dev/kvm` (Android emulator + Windows VM), `/dev/uinput` (module
loaded), `qemu-system-x86 10.2.1`, gcc 15, cmake,
full `dpkg-buildpackage` stack (fcitx5/libhangul debs were built on this machine),
`fcitx5-modules-dev 5.1.19`, extra-cmake-modules, git,
**`xvfb` 2:21.1.22 and `x11-utils` 7.7** (both installed — tier D3 needs them; `xdotool`
present too), stock `fcitx5` 5.1.19 with its XIM/dbus/xcb frontends.
`Xephyr` is installed as of 2026-10-03 (`xserver-xephyr`), for the fcitx5
wiki's nested-server debug recipe. It made no difference to D3 — see
FCITX5-ADDON-NOTES.md §11c for that and the other refuted guesses.
`libxcb-imdkit` source (1.0.9) is unpacked at `xcb-imdkit-1.0.9/` for reading
the XIM packet path; it is upstream source, not ours, and is untracked.

**Never `pkill -9 dbus-daemon`.** Doing that while cleaning up test processes
kills the *session* bus, and the compositor attached to it goes down with it —
this host runs KDE Plasma Wayland, and `kwin_wayland` was restarted as a result.
Clean up by PID: keep the `$!` handles from backgrounding and kill only those.
A `pkill` pattern that matches a daemon other processes share is not a cleanup
step. **No window manager is installed at all** —
not under Xvfb, not anywhere — so `XSetInputFocus` is the only focus
mechanism available and `_NET_ACTIVE_WINDOW` does not exist. That is not by
itself a defect: measured directly, `XSetInputFocus` under Xvfb takes effect
and XTEST keys reach the focused window. See
`docs/FCITX5-ADDON-NOTES.md` §11b for what is and is not thereby explained.

**Missing:** clang, ninja-build, python3-evdev, mingw-w64, swtpm,
JDK/Android SDK. Install in batches below, each batch small and phase-scoped.

Python tooling lives in `tools/.venv` (project-local). Never reuse the workspace-root
`~/work/.venv` — that is an unrelated 8 GB personal ML stack. **This bites on this host:**
the ambient `python3` resolves to `~/work/.venv/bin/python3`, so any script that shells
out to bare `python3` (D3's `xtest-type.py` does) silently inherits that stack. Pass an
explicit interpreter, or activate `tools/.venv` first.

## This host is not an fcitx5 desktop

The session input method is **kime**, not fcitx5. That matters before any
`apt purge fcitx5*`:

- purging `fcitx5` drags out `fcitx5-hangul` and `libime-*` as dependents. Reinstall
  only what the build and D3 need: `fcitx5 fcitx5-modules fcitx5-modules-dev
  fcitx5-config-qt fcitx5-data`. Losing `fcitx5-hangul` here costs nothing.
- if an `apt` run is interrupted (sudo auth timeout, Ctrl-C), dpkg is left
  half-configured and every later `apt` fails. Recover with
  `sudo dpkg --configure -a` first, then retry.
- because fcitx5 is not the live IM here, a stale `Xvfb :9x` / `fcitx5` left by
  an earlier debugging session is easy to forget. Check
  `pgrep -a -f 'Xvfb|fcitx5|xim-harness'` and `/tmp/.X*-lock` **before** trusting
  a confusing e2e result — a leftover X server also holds display numbers that
  `run-d3.sh` would otherwise pick.

## Batch 1 — P0–P4 (core, goldens, fcitx5 addon) — this machine

```sh
sudo apt install -y clang llvm ninja-build \
    python3-evdev python3-venv \
    xvfb x11-utils \
    devscripts debhelper dh-cmake equivs
```

- clang+llvm → libFuzzer harnesses, `llvm-cov` (tier A fuzz; gcc cannot drive libFuzzer)
- xvfb + x11-utils → tier D2/D3 (installed; D3 injects via **XTEST**, not uinput — Xvfb has
  no evdev backend and `/dev/uinput` is root-only in CI, so keys enter at the X server)
- python venv: `python3 -m venv tools/.venv && tools/.venv/bin/pip install evdev pytest`
- addon dev headers already installed (see above) — nothing extra needed for D1

### Running D3 from a desktop session

`tests/e2e/run-d3.sh` must not inherit the host desktop session. On this KDE/Wayland
workstation fcitx5 prefers `$WAYLAND_DISPLAY` over `$DISPLAY` and silently attaches to the
real compositor, leaving the Xvfb instance unused (XIM never registers, keys reach the
client raw). The script therefore unsets `WAYLAND_DISPLAY`, `XDG_SESSION_TYPE`,
`XDG_CURRENT_DESKTOP`, `XDG_SESSION_DESKTOP` and friends before launching fcitx5. If you
invoke fcitx5 by hand for debugging, do the same or you will chase the wrong display.

## Batch 2 — P5/P5.5 (Android IME + KCM APK) — INSTALLED 2026-10-03

**No sudo was required.** `sudo -n` fails on this host (interactive auth only),
so the JDK came from Adoptium and the SDK from Google's own zip — both into
`~`, both sha256-verified against the publisher's checksum. The apt route
(`openjdk-17-jdk`) below still works and is simpler if you have a terminal.

| what | version | path | pin |
|---|---|---|---|
| JDK | Temurin 17.0.20.1+1 | `~/toolchains/jdk-17.0.20.1+1` | `3808d1d1…726e08e` |
| cmdline-tools | 19.0 (build 13114758) | `~/Android/Sdk/cmdline-tools/latest` | `7ec96528…15317ee` |
| platform-tools / adb | 1.0.41 (r37.0.1) | `~/Android/Sdk/platform-tools` | — |
| platforms;android-35 | rev 2 | `~/Android/Sdk/platforms/android-35` | — |
| build-tools;35.0.0 | 35.0.0 | `~/Android/Sdk/build-tools/35.0.0` | — |
| emulator | 37.2.12.0 | `~/Android/Sdk/emulator` | — |
| system-images;android-35;google_apis;x86_64 | rev 9 | `~/Android/Sdk/system-images/…` | — |
| ndk | **27.3.13750724** | `~/Android/Sdk/ndk/27.3.13750724` | — |

The NDK was added on 2026-10-03, one step ahead of the Kotlin frontend: the
core could be cross-compiled and *proved* on the emulator before any adapter
exists. `sdkmanager --install "ndk;27.3.13750724"`; r27 is the first series whose
clang defaults to API 24+ sanitisers-free defaults and which the shipped
`cmake/android.toolchain.cmake` was written against.

```sh
export JAVA_HOME=~/toolchains/jdk-17.0.20.1+1
export ANDROID_HOME=~/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.3.13750724
export PATH=$JAVA_HOME/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/emulator:$PATH
```

**`/dev/kvm` is usable even though `scoby` is not in the `kvm` group** — the device
carries an ACL, verified by opening it O_RDWR. The emulator boots **headless in
~20 s** (`-no-window -no-audio -no-boot-anim -gpu swiftshader_indirect
-no-snapshot`), `sys.boot_completed` reaches 1, API 35. AVD `hunmin-e2e`
(pixel_6 profile). 4.8 GB SDK + 318 MB JDK; 322 GB free after.

### Signing a release APK by hand, for the debuggable gate

`assembleRelease` produces an **unsigned** apk, and an unsigned apk cannot be
installed at all — adb answers `INSTALL_PARSE_FAILED_NO_CERTIFICATES`. So
"build release, install it, check that it refuses the trace broadcast" is not a
test that can run as written.

This SDK has **no `debug.keystore`**, and `keytool` is not on `PATH` — it lives in
the JDK (`~/toolchains/jdk-17.0.20.1+1/bin/keytool`), and `apksigner` is a shell
wrapper that `exec`s a bare `java`, so it needs `JAVA_HOME/bin` on `PATH` too.
Neither failure says why in a way you would notice, and both look like a broken
apk rather than a missing tool.

```sh
export JAVA_HOME=~/toolchains/jdk-17.0.20.1+1
export PATH="$JAVA_HOME/bin:$PATH"          # apksigner execs a bare `java`
export ANDROID_HOME=~/Android/Sdk

keytool -genkeypair -keystore $ANDROID_HOME/debug.keystore \
  -storepass android -keypass android -alias androiddebugkey \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -dname "CN=Android Debug,O=Android,C=US"

cd frontends/android && ./gradlew assembleRelease
BT=$ANDROID_HOME/build-tools/35.0.0
$BT/zipalign -f 4 app/build/outputs/apk/release/app-release-unsigned.apk /tmp/aligned.apk
$BT/apksigner sign --ks $ANDROID_HOME/debug.keystore \
  --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey /tmp/aligned.apk
adb install -r -g /tmp/aligned.apk
```

This is for **verifying the gate**, not for release packaging — it signs with a
throwaway key. P5's real packaging (a `signingConfigs` entry, a version-code
scheme, R8 rules) is still open; see PLAN.md.

A release apk installed this way correctly **ignores** the tier E3 trace broadcast
and **refuses** `run-as`, which is the `FLAG_DEBUGGABLE` gate doing its job.

### Debugging the emulator over MCP (`android`)

Configured in `~/.hermes/config.yaml`, 25 tools enabled:

```sh
hermes mcp list                       # shows android ✓ enabled
hermes mcp test android               # ✓ Connected, 25 tools
```

`android-mcp-server` v1.3.0 (npm, MIT, TypeScript, 70 stars / 2 open issues,
pushed 2026-03) — https://github.com/martingeidobler/android-mcp-server.
Verified live against AVD `hunmin-e2e`: `list_devices` returns
`emulator-5554 [device] model:sdk_gphone64_x86_64`, `get_logs` streams real
logcat, `get_ui_tree` returns the node tree with resource ids and bounds.

⚠️ **Its PATH must contain `~/.local/bin`.** The entry point is
`#!/usr/bin/env node`, so a trimmed PATH that omits node's directory makes the
server fail to start with `Connection closed` — a bare, uninformative error.
That is how it first failed here.

Tools, grouped by what tier E needs:

- **logcat**: `get_logs` (filter by package/level/lines), `clear_logs`
- **UI**: `get_ui_tree` (resource-id/text/bounds), `wait_for_element`,
  `scroll_to_element`
- **APK**: `install_apk`, `launch_app`, `get_current_activity`, `pull_file`
- **device**: `list_devices`, `list_avds`, `start_emulator` (boots an AVD by
  name, waits 60s), `get_device_info`, `screenshot`
- **input**: `tap`, `tap_element`, `tap_and_wait`, `type_text`, `press_key`,
  `swipe`, `long_press`, `double_tap`, `multi_tap`, `tap_sequence`
- **escape hatch**: `adb_shell`, with a persistent session. This is the
  IME-specific one — `ime list -s -a`, `ime enable/set`,
  `settings put secure default_input_method` and `dumpsys input_method` all
  reachable.

⚠️ **The tools appear in the NEXT session, not this one.** `hermes mcp add`
writes the config, but the running agent loaded its tool list at startup. A
session that installs a server must still use `adb` on the command line; the
next session gets the tools. So do not conclude the install failed because the
tool names are absent — check `hermes mcp list` and `hermes mcp test android`
instead.

⚠️ **`tools/keymap-fixture.py` needs `JAVA_HOME`.** It verifies the Android
keycode table against `android.jar` via `javap`; without a JDK it prints a skip
and exits 0. That is deliberate — CI's linux job has no SDK, and failing there
would make the gate unusable rather than honest about the gap — but it means a
green `keymap-fixture` on a bare host has NOT checked the Android column.

**Why this one, and what it cost.** A first pass chose
`@landicefu/android-adb-mcp-server` (npm's top hit for "adb mcp", 10 tools) and
documented two gaps: no logcat tool, no UI-dump tool. A parallel search found
this one, which covers exactly those two gaps and adds emulator lifecycle
management, so the first choice was replaced. The alternatives measured:
`yume-chan/ya-webadb` (3,236 stars, the most maintained ADB library there is)
and `callstack/agent-device` (4,873 stars) are both substantially larger and
better maintained — `agent-device` also records `.ad` scripts that **replay in
CI**, which is what tier E2 will eventually want. Neither is a plain MCP server
for this shape of work, and both carry a larger surface than a thin adb wrapper
needs. `agent-device` is installed and is the right thing to layer on when the
conformance corpus becomes recorded CI tests.

Dead ends worth recording: `srmorete/adb-mcp` (55 stars) is archived;
`minhalvp/android-mcp-server` (811 stars) was last pushed 2025-05 and its own
README says the device path is "not tested" — star count is misleading there;
`tanbro/uiautomator2-mcp-server` has 76 tools but no logcat and needs an
`init` step that pushes binaries to the device.

⚠️ **Correction to TESTING.md tier E2:** the documented injection command is
wrong. `adb shell input -source keyboard keyevent N` fails with "Unknown
command: -source" — the source is a *positional* argument that comes BEFORE the
command:

```sh
adb shell input keyboard keyevent 29     # correct
adb shell input -source keyboard keyevent 29   # what TESTING.md said; does not exist
```

### Batch 2 (original plan, for reference)

```sh
sudo apt install -y openjdk-17-jdk unzip

```sh
sudo apt install -y openjdk-17-jdk unzip
# Android cmdline-tools (Google download) -> $ANDROID_HOME/cmdline-tools/latest, then:
sdkmanager --licenses
sdkmanager "platform-tools" "emulator" \
    "platforms;android-35" "build-tools;35.0.0" "cmake;3.22.1" \
    "ndk;27.2.12479018" \
    "system-images;android-35;google_apis;x86_64"
export ANDROID_HOME=... ANDROID_SDK_ROOT=$ANDROID_HOME
```

- Gradle is vendored in-repo via wrapper (no system gradle)
- NDK 27 + SDK cmake match fcitx5-android's documented NDK/CMake requirements, keeping the
  tier-C trace harness portable between our IME and a possible future plugin port
- emulator runs on `/dev/kvm` (`-no-window` in CI, headful for device-matrix work)

## Batch 3 — tier-B portability cross-builds — same machine

```sh
sudo apt install -y mingw-w64
cmake --toolchain x86_64-w64-mingw32 ...   # builds libhunmin.dll + run conformance traces
```

MSVC proper is **not** needed locally, but the honest reason changed 2026-10-13:
the `windows-latest` CI job is **disabled** (issue #51 — "for windows, it is not
scope of this version. so in the issue we will disable windows test."), so there
is **no MSVC coverage in CI at all** right now. `mingw` (same core target, GCC)
is what runs. If MSVC support comes back it is its own issue, with its own
measurement — the job was red before it was disabled, on
`cl : command line error D8021: invalid numeric argument '/Wextra'` from
`add_compile_options(-Wall -Wextra)` in `CMakeLists.txt:10`.
Wine is deliberately excluded (TSF support incomplete → invalid test signal).

## Batch 4 — P6 (Windows TSF TIP) — first real Windows need

Local VM (preferred for interactive COM debugging) on existing qemu + `/dev/kvm`:

```sh
sudo apt install -y swtpm ovmf virt-manager    # Win11 needs TPM2.0+vTPM: swtpm emulates
```

- Win11 evaluation ISO + VHDX (Microsoft-provided, license-free for eval), virtio-net/block
- Inside the VM: **Visual Studio 2022** Community with workloads: *Desktop development
  with C++*, *C++ ATL*, *Windows SDK*; plus **TAT** (TSF Accommodation Tool, from the
  Windows SDK extras) and Python + `pywinauto` for F2 UIA assertions
- Physical BT keyboard checks (F3): pass a USB BT dongle through to the VM
  (`-device usb-host`); otherwise fall back to injected keys (automatable but less real)
- ⚠️ CI does **not** carry this burden yet. As of 2026-10-13 the `windows-latest`
  job is disabled (`if: false`, issue #51 — Windows is out of scope for this
  version), and it only ever built the core library: the addon is skipped there
  ("Fcitx5Core not found") and no TIP exists to build. So "MSVC builds core + TIP
  and runs the tier-C trace" was already an overstatement of the job, and with the
  job disabled nothing of it runs. Whatever comes after P6 has to carry it.

## LSP — off by default, one word to turn it on

opencode's LSP is **disabled unless configured**, and it is not hot-reloaded, so the
switch is which command you launch:

```sh
opencode      # LSP off (project opencode.json deliberately omits "lsp")
hunmin-lsp    # LSP on  (~/.local/bin/hunmin-lsp; adds .opencode/lsp.json)
```

`.opencode/lsp.json` is committed and diffable; it is inert on its own, because
`opencode.json` never references it. Verified with `opencode debug config`:
`lsp` is `null` without the env var and populated with it.

### What each server is for here, and what it actually reports

Measured 2026-10-09 by running `opencode debug lsp diagnostics <file>` against the
**clean** tree. A zero-diagnostic result is ambiguous — no server and no problems
look identical — so every server below was given a deliberate defect first and had
to report it before its clean baseline was believed.

| server | source | clean-tree result |
|---|---|---|
| `clangd` 23.1.0 | auto-downloaded to `~/.cache/opencode/bin` | 0 in `libhunmin/src/ic.c`; 2 in `frontends/fcitx5/src/engine.cpp` (unused private fields `engine_`, `ic_` — clang-tidy class, not a live bug) |
| `pyright` 1.1.414 | `uv tool install pyright` | **29 errors / 34 files** in `tools/*.py` |
| `bash-language-server` | auto-downloaded | 3 in `tools/android-e2e.sh`, 0 in the other 11 |
| `yaml-ls` | auto-downloaded | 0 in `.github/workflows/ci.yml` |
| `kotlin-ls` | — | **unavailable, see below** |

`clangd` needs the compilation database, which `CMakeLists.txt` now exports
(`CMAKE_EXPORT_COMPILE_COMMANDS ON`). clangd searches `build/` subdirectories by
itself, so no symlink or `--compile-commands-dir` is needed. Without that export
clangd still starts and still returns `[]` — it just parses every TU with guessed
flags, which is the "measured nothing" failure in `docs/D3-ROOT-CAUSE.md`.

**bash-language-server is inert without `shellcheck` on PATH.** It reported nothing
at all — not even a deliberate syntax error — until `uv tool install shellcheck-py`
put a binary there. Same shape as the clangd case: a green empty result that was
really a dead server. `shellcheck-py` ships the binary via pip, so no sudo.

### kotlin-ls is disabled, and why

opencode's built-in kotlin-ls builds a download URL from two strings in its own
binary:

```
https://api.github.com/repos/Kotlin/kotlin-lsp/releases/latest
https://download-cdn.jetbrains.com/kotlin-lsp/${version}/${kotlin-lsp-${version}-${platform}-${ts}.zip}
```

The GitHub half works: `Kotlin/kotlin-lsp` returns 200, tag `kotlin-lsp/v263.6379.0`,
**zero GitHub-hosted assets** (the release publishes links, not uploads). The CDN
half is what breaks — JetBrains moved the standalone build, changing both the path
segment and the filename:

| | opencode asks for | JetBrains serves (verified) |
|---|---|---|
| path | `download-cdn.jetbrains.com/kotlin-lsp/<v>/` | `download-cdn.jetbrains.com/language-server/kotlin-server/<v>/` |
| file | `kotlin-lsp-<v>-<platform>-<ts>.zip` | `kotlin-server-<v>.tar.gz` (also `.sit`, 6 `.vsix`) |
| version | from tag `kotlin-lsp/v263.6379.0` | bare `263.6379.0` |

so the request 404s on a host that is otherwise healthy — the correct URL returns
206 to a byte-range request. Because the failure is a CDN rename rather than a dead
repository, this is fixable by pinning opencode at the new path, and it should be
re-checked on the next opencode upgrade rather than assumed permanent.

Left enabled it makes every launch wait on a download that cannot succeed, and it
fails *silently* — `opencode debug lsp diagnostics <file>.kt` returns `{}` with no
error, which is indistinguishable from a clean file. It is disabled explicitly in
`.opencode/lsp.json` for that reason.

Kotlin is not left ungated: `./gradlew assembleDebug` compiles all 8 files, which
is the same argument that makes clangd low-value for the C++ addon.

### The 29 pyright findings

Not noise, and not something LSP introduced — a pre-existing condition that had no
gate at all, since CI runs `cmake`/`ctest` and nothing for Python. 26 of the 29 are
the same real defect: `re.match()`/`re.search()` returns `Optional[Match]`, and the
result is used without a None check, so `.group()`/`.findall()` raises `AttributeError`
on any input that does not match. The other 3 are deliberate lazy imports
(`mwparserfromhell`, `kiwipiepy`) that the code already guards with `sys.exit`.

They will surface as diagnostics on every tools/*.py edit until they are fixed.

### One real bug found by this, not yet fixed

`tools/android-e2e.sh:54` uses `local` at top level (between the one-line `fail()`
at line 39 and `dump_diag()` at line 74). With `set -euo pipefail` that is an
immediate `exit 1` with a locale-dependent bash error message — instead of the
intended `skip "no AVD named ..."`. The line is reached only when `adb get-state`
fails, i.e. when no device is attached, which CI never is because it boots the
emulator first. So the no-device path of a tier-E2 script has never been executed.
Verified independently of shellcheck:

```console
$ bash -c 'set -euo pipefail; X=1; local avd="x"; echo REACHED'
bash: local: can only be used in a function      # exit 1, REACHED never printed
```

### clangd diagnostics race on a cold index — run twice

`opencode debug lsp diagnostics` starts a **fresh, short-lived clangd** per
invocation. Whatever the background index has finished by the time the response is
built is what you get, so the first run after the index is invalidated reports
against a partial AST. Measured on `frontends/fcitx5/src/engine.cpp`, unchanged
between runs and `git diff`-clean throughout:

```
run 1 (cold):  line 354, 355
run 2 (warm):  line 360, 361
run 3 (warm):  line 360, 361
```

A one-off `No member named 'Shift_L' in 'fcitx::KeyState'` also appeared on a cold
run at line 167 and never came back — `KeyState::Shift_L` does not exist and
`KeyState::Hyper` does (`/usr/include/Fcitx5/Utils/fcitx-utils/keysym.h:35`), so the
line genuinely has nothing wrong with it. The file compiles and ctest is green.

**So: a single clangd diagnostic run is not a measurement.** Run it twice and take
the second, or warm the index first. The same caution applies to a `0 diagnostics`
result — it is what both "clean" and "crashed before answering" look like, which is
why every server here had to be given a deliberate defect and report it before its
clean baseline was believed.

## CI (from P0) — the "second environment" so no dual-boot is ever required

**What CI actually does, re-verified against `.github/workflows/ci.yml` on
2026-10-13.** The 2026-10-09 version of this table listed four jobs and named a
`nightly` that did not exist; both are corrected below. Seven job blocks, one of
them disabled:

| Job | Runner | Contents |
|---|---|---|
| linux | ubuntu-24.04 | full `ctest` (tiers A–G), `-DHUNMIN_TRACE_BUILD=ON` |
| android | ubuntu-24.04 + AVD | NDK 27.3.13750724 cross-build (both ABIs), goldens replayed on the AVD, E0/E1/E2 |
| deb | ubuntu-24.04 | `dpkg-buildpackage -us -uc -b -d`, uploads the `.deb`; the suite runs inside the package build |
| android-release | ubuntu-24.04 | `assembleRelease`, throwaway keystore; **gated** on `vars.HUNMIN_BUILD_RELEASE == 'true'` and a push to `master` |
| mingw | ubuntu-24.04 | cross-build `libhunmin.dll` (`-DBUILD_TESTING=OFF`) |
| windows | windows-latest | **disabled** (`if: false`, issue #51) — kept in the file so "was Windows built here, and why did it stop" stays answerable |
| nightly-fuzz | ubuntu-24.04 | LeakSanitizer build + a random-key sweep; `schedule` only (03:00 UTC) |

**Listed here but still not implemented — do not plan work assuming they exist:**
the libFuzzer 8h and differential-vs-libhangul 1h jobs (the `nightly-fuzz` job
above is a different, much smaller thing), and any TIP/TAT step (that is P6, and
P6 has not started).

**MSVC is not in the matrix at all.** No job compiles the core with `cl` while
the `windows` job is disabled; `mingw` covers the same target under GCC.

## Sequencing rule

No Windows environment blocks any work before P6; no Android environment blocks anything
before P5. Install each batch when its phase starts.

## Orca GUI (installed terminal/editor — worktree orchestration)

**Installed.** Orca 1.4.219 (`/opt/Orca/orca-ide`, Electron, Wayland) with its
CLI shim on `PATH` (`~/.config/orca/linux-orca-cli-shim/orca`). It is a
GUI-managed terminal/editor with its own worktree registry, and **a Hermes
session may run inside an Orca terminal** — that changes how worktree work
should be driven (below). The Android SDK/NDK/JDK are installed separately;
Orca only orchestrates the terminals that use them.

### Detecting the environment

```sh
# "orca" when the shell runs inside Orca's GUI; "plain" otherwise
[ "$TERM_PROGRAM" = Orca ] || [ -n "$ORCA_WORKTREE_ID" ] && echo orca || echo plain
command -v orca   # shim present?
```

Inside Orca, `ORCA_WORKTREE_ID` / `ORCA_WORKSPACE_ID` name the current
Orca-managed worktree as `<repoId>::<path>` (e.g.
`820bd866-…::/home/scoby/work/hunmin`).

### Driving worktree work so it is visible in the Orca window

When inside Orca, launch work through `orca worktree create` rather than a
background `git worktree add`, so the maintainer can watch and control it:

```sh
# create + a visible Hermes worker, based off master
orca worktree create --name task-<id>-<slug> --agent hermes \
  --base-branch master --prompt "<task brief>" --activate --json
# find its terminal handle, then read / wait / send
orca terminal list  --worktree <repoId>::<newPath> --json
orca terminal read  --terminal <handle> --json
orca terminal wait  --terminal <handle> --for tui-idle --timeout-ms 120000 --json
orca terminal send  --terminal <handle> --text "<follow-up>" --enter --json
# cleanup (merge rules still apply — see docs/ENGINEERING.md "Worktree cleanup")
orca worktree rm    --worktree <repoId>::<newPath> --force --json
```

`--activate` reveals the new worktree in the Orca app; without it the worker
starts in the background and is invisible. The worktree id is the whole
`<repoId>::<path>` value — copy it intact. The full flag set is in
`orca agent-context --json`.

### Two ways this conflicts with AGENTS.md "Worktree discipline" — override both

AGENTS.md mandates `~/work/<repo>-<task>` off master. An Orca `worktree create`
defaults to **`<repo>/<name>`** (nested *inside* the repo, e.g.
`/home/scoby/work/hunmin/orca-probe`) and bases off the **current branch**, not
master. Measured 2026-10-10:

- **Location cannot be forced** through the CLI — there is no `--path` flag.
  A nested worktree dirties the main worktree's `git status` (`?? <name>/`).
  Do **not** `mv` an Orca-managed worktree; Orca loses track of it. If the
  `~/work/<repo>-<task>` rule must hold, use the git-native `git worktree add`
  path instead and accept that the work is not in an Orca window.
- **Base and name can be overridden**: pass `--base-branch master` (otherwise
  the child is cut from the current feature branch — stacked work) and
  `--name task-<id>-<slug>` (`--name` is also the branch name, so this keeps
  the `task/…` convention).

### `worker-start` is not `worktree create` — the branch name is prefixed

Measured 2026-10-11 with Orca **1.4.219**, through the orchestration path
(`orca orchestration worker-start --worktree new-child --name task/4-r8-shrinker
--base-branch master`):

- **The branch came out `refs/heads/scobyseo/task-4-r8-shrinker`**, not
  `refs/heads/task-4-r8-shrinker`. Orca prefixes the repository owner. The
  `task/<id>-<slug>` convention in `AGENTS.md` is therefore **not** preserved by
  this path; the coordinator has to read the real branch back
  (`git -C <worktree> branch --show-current`) and tell the worker.
- The nested location behaved exactly as measured on 2026-10-10:
  `/home/scoby/work/hunmin/task-4-r8-shrinker`, and `?? task-4-r8-shrinker/`
  in the main checkout's `git status`.
- `--issue` is **not** a `worker-start` flag (`orca worktree create` has one), so
  Orca's own `linkedIssue` stays `null` when the link rides in the Task spec.

The 2026-10-10 bullet above was measured with `orca worktree create`, a
different command, and is **not** restated here as corrected: whether
`worktree create` prefixes the branch the same way is **not measured**.
The full picture is `docs/ORCHESTRATION.md` §5.

Whether a nested worktree breaks any tool that walks the tree (a glob that
would now descend into `orca-probe/`) has **not** been measured. `orca worktree
rm --force` cleans up both the git entry and the directory — no `rm -rf`.
