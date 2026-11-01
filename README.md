# hunmin

A portable Hangul (한글) input engine: a C library with a stable ABI, and thin
frontends for fcitx5, Android, and (planned) Windows TSF.

**The design constraint that shapes everything: layout behaviour is data, never
code.** Adding a keyboard layout means adding a JSON file and golden tests — no
C, no rebuild of the engine's rules. The engine knows how to compose syllables
and how to buffer an eojeol; it does not know what a keyboard is.

```
libhunmin.so   the engine (C99, no dependencies)
frontends/      fcitx5 addon (C++), Android IME (Kotlin + JNI)
```

## Status

**P0–P5 implemented; P6 (Windows TSF) in progress.** Milestone A (TSF frontend scaffold, #64) is done — `hunmin-tsf.dll` builds natively on the Windows 11 host and registration was verified end-to-end in Notepad. Milestone B (core input integration, #65) is open. `ctest`: **green**.

| | |
|---|---|
| Engine | two kinds: `std` (byte-exact vs libhangul, parity-swept),
  `h3` (`case` expressions over jamo slots) |
| Layouts | dubeolsik, sebeolsik (3 variants), romaja, 신세벌식 P2, 옛한글 |
| Compose | compound-vowel + 받침 tables (KS X 1001), VU-TRIO,
  pair-aware G-gate |
| Hanja | works — `hunmin_suggest_*` reads the 한국어기초사전 extract |
| Romaja | ported and parity-verified against libhangul |
| fcitx5 addon | tier C/D green, packaged as a `.deb` |
| Android IME | `InputMethodService` + JNI; 4 ctest tiers, verified on an emulator |
| Windows TSF | milestone A done (#64); milestone B (#65) open |

Not done, explicitly: a soft keyboard (the input view is a layout strip, not
keys); the E3 manual session on a real Bluetooth keyboard, so no device is
signed off; F9 has no AOSP keycode, so the hanja trigger is still an open design
question.

Corrected 2026-10-16, because an earlier revision of this paragraph claimed
"the R8 shrinker path is written but never exercised … `assembleRelease` runs in
no CI job". Both halves were false: the shrinker path was measured end-to-end on
2026-10-11 (`assembleRelease` + install + composition on an AVD), recorded in
`tests/manual-checklists/release-r8-shrinker.md`, and CI job `android-release`
builds the release APK. What remains true of it is narrower: that job is gated
on a repository variable, so a green run does not imply a release APK was
built, and no CI job installs or types into the release APK — `tools/android-e2e.sh`
drives the debug one by design. Status lives in `docs/REMAINING-WORK.md`.

The Android side of tier C **is** present — 14 `.in`/`.gold` pairs and
`tools/android-trace-emit.sh`; an earlier revision of this file claimed
otherwise and was wrong.

### Running the tests

`ctest` runs everything; that is the right default and CI does it. Locally you
usually touched one tier, and every test carries labels:

```sh
ctest --test-dir build -N                 # everything, with per-test labels
ctest --test-dir build -L tier-A          # engine unit + goldens
ctest --test-dir build -L tier-D          # the three desktop-integration tiers
ctest --test-dir build -LE slow           # everything except the seven slow tests
ctest --test-dir build -LE mutation       # skip the tiers that rewrite source

tools/ctest-select.sh -L tier-C           # prints what it will run AND what it will not
```

On a 57-test configuration (Android SDK and a JDK exported at configure time),
`-LE slow` runs 50 of the 57 in about 6.5 s against 54 s unfiltered. The seven
excluded are the wall clock: the three tier-D3 tests, both mutation self-tests,
`engine-android-e1` and `docs-lint`. The figure is only meaningful next to that
configuration — a build without the Android toolchain registers fewer tests and
runs the same selection faster. The vocabulary is in
[`docs/TESTING.md`](docs/TESTING.md).

## Try it

```sh
cmake -B build && cmake --build build

./build/hunmin-sim --data libhunmin/data --layout dubeolsik --keys 'dkssud '
./build/hunmin-sim --data libhunmin/data --layout romaja   --keys 'gksrmfd '
./build/hunmin-sim --data libhunmin/data --check tests/golden/ref-dubeolsik.jsonl
```

Two things about reading the output. The trailing space matters: without it the
last syllable is still in the composition buffer. And each line is the **delta**
for that key, not the text so far — `--keys 'gksrmfd '` types 한글, but its last
line reads `commit='ㄷ'` because that is only what the final key contributed.
`--trace` shows the buffer evolving.

`hunmin-sim` is the engine with no IME framework around it: keys in, text out.
Every layout ships as a golden file, so `--check` replays a recorded session and
diffs.

## Install

```sh
# engine + fcitx5 addon
gh release download 0.1.0 --repo scobyseo/hunmin   # needs repo access; see below
dpkg -i hunmin_0.1.0_amd64.deb     # or build it: tools/build-release.sh deb

# or build the PPA package yourself — see PLAN.md, gate is the tier-D suite
```

Android: `frontends/android/gradlew assembleDebug` after building the core for
each ABI (`tools/build-android.sh`). The APK's `versionName` is `0.1.0-p5` —
the release is `0.1.0` and the `-p5` is the Android build-phase label, not a
different release; the keymaps APK carries `0.1.0-p5.5` for the same reason.
The convention is recorded in `docs/MILESTONES.md` §0.1 (issue #9).

**Where the release artefacts come from (issue #8):** CI now builds both —
job `deb` produces the `.deb` and job `android-release` produces the release
APK, each uploaded as a run artefact. The same sequence runs locally with
`tools/build-release.sh` (`deb` / `apk` / `all`). One caveat recorded rather
than hidden: the keystore path was decided 2026-10-13 (issue #1) — it resolves
from `HUNMIN_RELEASE_STORE`, else from gitignored `local.properties`, else by
generating a throwaway key, so a CI-built release APK proves the release
*build*, not a release *signature*. Whether 0.1
requires CI-built artefacts at all, or a local build suffices, is the ruling
issue #8 asks the maintainer for.

**Where a 0.1 user gets them (issue #7):** the channel is a **GitHub Release on
tag `0.1.0`**, with `hunmin_0.1.0_amd64.deb` attached — recorded in
`docs/MILESTONES.md` §0.1, scripted in `tools/release-0.1.sh`. The release is
published and its attached `.deb` is the artefact from that release run, not the
stale earlier build (verified by download and `dpkg -c`, 1,252,740 bytes). **But
this repository is private until after 0.2** (maintainer ruling 2026-10-16), so
the channel is account-gated: `curl` on the download URL returns 404
unauthenticated, and a user without repo access cannot fetch 0.1 at all. That is
the recorded state, not a defect — but it is why issue #7 is still open, whose
third verification item asks whether the channel is reachable from a clean
machine. The `.deb` is unsigned, so `dpkg -i` warns; the signed release APK stays
blocked on issue #1.

**Known limitation (Android voice input):** recognition quality has no
automated assertion — recorded audio cannot be replayed into the recogniser,
so the judgement is a person's (tier E4 design). Measured once on hardware
2026-10-12, by the maintainer, speaking the tier E4 phrases: the run passed.
Unattended runs are not possible — no speaker means no speech. The permission
path and key-path survival are tested (tier E2). `docs/MILESTONES.md` §0.1
names the limitation if shipped unmeasured; the measurement record lives in
`tests/manual-checklists/tier-e4-voice.md` (issue #11).

## Documentation

| | |
|---|---|
| [`PLAN.md`](PLAN.md) | architecture, phases, and why the project is shaped this way |
| [`AGENTS.md`](AGENTS.md) | **start here if you are an agent** — commands, hard rules, current state |
| [`docs/SPEC-P2.md`](docs/SPEC-P2.md) | the layout specification, and how the engine satisfies it |
| [`docs/TESTING.md`](docs/TESTING.md) | the seven verification tiers, A–G |
| [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md) | what this host has installed (Android SDK/NDK, fcitx5) |
| [`docs/HOWTO-NEW-LAYOUT.md`](docs/HOWTO-NEW-LAYOUT.md) | adding a layout without touching code |
| [`docs/D3-ROOT-CAUSE.md`](docs/D3-ROOT-CAUSE.md) | why tier D3 looked green while measuring nothing |
| [`docs/D3-PERFORMANCE.md`](docs/D3-PERFORMANCE.md) | where tier D3's wall clock goes, and the case-count defect it hid |
| [`docs/OPTIONS-MEASURED.md`](docs/OPTIONS-MEASURED.md) | eleven priced questions and their rulings: the mutation tier, the android sandbox, the test framework, the `ch` input, the TSF unregister mirror, whether the Han/Eng key can act as a held modifier, what each single/compound 한/영 assignment costs, which English dictionary `english_valid` looks up, whether the modes judgement's B1 blocker still exists, the units the two modes pass lines can be graded in, and who owns the modes judgement itself |
| [`docs/REMAINING-WORK.md`](docs/REMAINING-WORK.md) | what is left, what is blocked on whom, and the assertions that keep it honest |
| [`docs/MILESTONES.md`](docs/MILESTONES.md) | the release plan (0.1 / 0.2 / 0.3 / 0.4 / 1.0) and which lane each open item sits in |
| [`wiki/`](wiki/) | how we got here: session records, overturned verdicts, open decisions |

## Design notes

**Why a C core with an ABI?** The engine has to be callable from C++, Kotlin/JNI
and (later) C++/WinRT without three implementations of the composition rules.
`hunmin.h` is append-only; adding a symbol is a deliberate act.

**Why is data separate from code?** So that a layout can be lifted from a
normative source — a vendor's `ist.xml`, a keyboard's tables — and replayed as a
golden test against the same input sequence. The flagship layout is lifted
verbatim from [pat.im/1136](https://pat.im/1136)'s 날개셋 files.

**Why so many test tiers?** Because an IME only truly lives inside a foreign
framework. Tier A (engine) runs anywhere; tiers C–E prove that fcitx5 and
Android IMMS actually pass the right string to the client. A Windows frontend
will need the same treatment, and tier C exists so that "engine-correct" is a
checkable claim on a platform with no other test.

## Licence

LGPL-2.1-or-later (engine and frontends). Bundled data carries its own terms —
`libhunmin/data/*/LICENSE.md` per directory; notably the dictionary extract
is **CC-BY-SA 2.0 KR** and is kept in a separate directory for that reason.

Upstream projects read for semantics, never copied: libhangul (BSD-3), kime
(GPL-3), and the 국립국어원 dictionaries (see [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md)).