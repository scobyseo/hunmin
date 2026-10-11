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

**P0–P5 implemented; P6 (Windows TSF) not started.** `ctest`: **green**.

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
| Windows TSF | **not started** |

Not done, explicitly: a soft keyboard (the input view is a layout strip, not
keys); the E3 manual session on a real Bluetooth keyboard, so no device is
signed off; F9 has no AOSP keycode, so the hanja trigger is still an open design
question; and the R8 shrinker path is written but never exercised (it ships
`proguard-rules.pro` and `isMinifyEnabled = true`, but `assembleRelease`
runs in no CI job, so the shrinker has never actually removed anything).

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
dpkg -i hunmin_0.1.0_amd64.deb     # see ~/work for the built artefact

# or build the PPA package yourself — see PLAN.md, gate is the tier-D suite
```

Android: `frontends/android/gradlew assembleDebug` after building the core for
each ABI (`tools/build-android.sh`).

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
| [`docs/MUTATION-TIER-OPTIONS.md`](docs/MUTATION-TIER-OPTIONS.md) | what the mutation self-tests cost and what each way of cutting that loses |
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