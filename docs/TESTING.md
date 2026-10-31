# hunmin — Verification & Testing Plan

## Strategy in one paragraph

The engine's *behavior* is data + pure C logic, so it is unit-testable to near-100% on a
plain host. The hard part — an IME only truly lives inside foreign frameworks (fcitx5,
Android IMMS, Windows TSF) — is handled by making every frontend a thin *adapter* (<~700
LOC) whose only jobs are (1) event mapping and (2) buffer/candidate presentation. Tests
therefore attack a pyramid: golden engine vectors (A), ABI/data contracts (B), a
**platform-neutral conformance-trace suite** that every adapter must reproduce (C), then
the smallest possible real-framework end-to-end layer per platform (D/E/F). Any behavior
bug must be reproducible at tier A or C; tiers D–F exist only to catch *adapter* bugs.

Legend: 🤖 fully automated in CI · 🧑 manual (scripted checklist) · 📦 required per-release gate

---

## Test labels — selecting part of the suite

`ctest -L <regex>` selects tests by label, `-LE <regex>` excludes them, and
every test in this repo carries at least one. **Labels are for the local loop,
never for a gate**: `.github/workflows/ci.yml` runs the plain unfiltered
`ctest`, because a filtered run cannot prove what it left out.

**What that costs, measured 2026-10-14 for issue #59: 269 s of the linux job's
8 m 24 s, and 402 s of the deb job's 13 m 30 s.** The per-job figures are in
`docs/CI-TIMING.md`; the short version is that three tests are 252 of the
linux job's 269 test-seconds and 386 of the deb job's 402, so the unfiltered
run and a 1–2 minute job are not currently compatible.

**The deb job is now the one exception, and it says so where it runs.**
`debian/rules`' `override_dh_auto_test` runs `ctest -j4 -LE 'tier-E1'`, because
the .deb does not ship the Android adapter and the adapter's own gate is the
`android` job. That is a filtered run inside a package build, which is exactly
the shape the paragraph above warns about, so the exclusion is written as a
label rather than a name list and the reason is recorded in the recipe's own
comment — the one place a reader of the package build will look. The linux job
still runs the plain unfiltered `ctest`, and the property above is unchanged
there: a label-selected run cannot prove what it left out, which is why the
exception is one recipe with its reason attached rather than a workflow-wide
`-LE`.

### Why labels rather than a `-R` regex over names

Test names are partly machine-generated. The Android tier-C registrations are
named from the corpus filename —
`engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`
— so renaming a case label silently breaks anyone's saved `-R` expression, with
no error anywhere. A label is declared once next to the test that earns it and
does not move when the corpus does.

### The vocabulary

Two **independent** axes. Axis 1 answers "which tier did I touch?"; axis 2
answers "what does this test cost me, and what does it do to my working tree?"

| Label | Meaning |
|---|---|
| `tier-A` | engine unit + golden vectors, host, C |
| `tier-B` | ABI and data contracts (`abi`, `schema-contract`, `krdict-fixture`) |
| `tier-C` | cross-platform adapter conformance + the keycode fixtures |
| `tier-D1` / `tier-D2` / `tier-D3` | the three fcitx5 tiers below |
| `tier-E0` / `tier-E1` / `tier-E2` | the automated Android tiers below |
| `tier-G` | process: `docs-lint`, `modes-corpus-selfcheck` |
| `mutation` | the test rewrites repo source and rebuilds, to prove a guard can fail |
| `slow` | measured ≥ 2 s on the host where the label was assigned |
| `guard` | cheap enough to run on **every commit**; orthogonal to the tier axis |

Two membership facts that are not derivable from the tier name:

- **The mutation sandbox inherits the parent's dependency prefixes.**
  `tools/frontend-mutations.py` configures a copy of the tree, so a dependency
  installed outside the system prefix (fcitx5 5.1.13 under `$HOME` on the WSL
  host) must reach it twice: `CMAKE_PREFIX_PATH` from the parent cache, and a
  `PKG_CONFIG_PATH` derived from it for the build. Both are handled since
  2026-10-25; a build tree that lives OUTSIDE the repo (the WSL layout keeps one
  in `$HOME`) has no parent cache to read, so export both before invoking ctest
  — with neither, the sandbox reports `initial build failed`, which reads as a
  code failure and is not one. The harness now prints the tail of its build log
  so that message names the missing target or header instead of asserting
  itself.
- **`frontend-mutations` carries both `tier-D1` and `tier-D2`.** It plants each
  mutation and requires something to catch it, and what catches it is
  `ctest -R engine-d1-fcitx5` and `ctest -R engine-d2-ui`
  (`tools/frontend-mutations.py:334-338`). It belongs to both tiers and to
  neither alone, so it says so with two labels rather than being hidden behind
  a synthetic "tier-D-mutations".
- **`tier-C` covers both platforms.** The 87 X11 `conformance-*` and the 14
  Android `engine-android-conformance-*` tests are the same tier on different
  adapters — that is what makes the corpus a cross-platform contract. Narrow to
  one with `-R conformance-` / `-R engine-android-conformance-`.

`slow` is a measured cut, not a round number: on this host the seven slowest
tests take 2.8–50.3 s and the eighth takes 1.7 s, so any cut in (1.7, 2.8] s
yields the same partition and the label only has to be re-measured when that
gap closes. It is **host-relative**, and `engine-android-e1` sits at the
boundary — 2.8 s against a warm Gradle cache, 15.9 s cold.

Both mutation tests are also `slow` today, so `-LE 'slow|mutation'` currently
selects exactly what `-LE slow` selects. That is a fact about these two tests,
not a redundancy in the scheme; the composition starts to differ the moment a
cheap mutation test exists.

### Selecting

```sh
ctest --test-dir build -N -L tier-D          # list, run nothing
ctest --test-dir build -j16 -L tier-A        # run
ctest --test-dir build -j16 -L 'tier-A|tier-B'   # several -L's are OR'd
ctest --test-dir build -j16 -LE slow         # everything cheap
ctest --test-dir build -j16 -LE mutation     # nothing that rewrites your tree
ctest --test-dir build -j4 -L guard          # the commit-level check (see below)
```

`-L`/`-LE` take a **regex matched unanchored against each label**, which is what
makes the families work: `-L tier-D` selects tier-D1, -D2 and -D3 together, and
`-L tier-E` selects E0, E1 and E2. There is no `tier-D` label of its own, and
that is intentional — one that matched only itself would be a label whose
meaning depends on the regex you typed.

Counts are **configuration-dependent** and so are the numbers below. Measured
2026-10-10 on the maintainer's host, `-DCMAKE_BUILD_TYPE=RelWithDebInfo`,
one run each, `ctest -j16`:

| configuration | tests |
|---|---|
| default (no Android toolchain in the environment) | 56 |
| `JAVA_HOME` + `ANDROID_HOME` exported — **the row below** | 58 |
| … plus `ANDROID_NDK` and `adb`, which register tier E0 and E2 | 60 |
| … plus `-DHUNMIN_TRACE_BUILD=ON` (all of the above) | 178 |

This branch adds **one** registration, `engine-android-espresso`
(`tools/android-espresso.sh`), and it appears only where the Android toolchain
is present — measured 0 occurrences in a no-toolchain configure, so it is
inside the `ANDROID_NDK` + `adb` guard with E0/E2 as intended.

Re-measured 2026-10-13 in a clean `env -i`, master then this branch, `ctest -N`
on a freshly configured tree:

| configuration | master | this branch |
|---|---|---|
| default (no Android toolchain) | 60 | 61 |
| `JAVA_HOME` + `ANDROID_HOME` | 61 | 61 |
| … plus `ANDROID_NDK` and `adb` | 63 | 64 |
| … plus `-DHUNMIN_TRACE_BUILD=ON` | 181 | 182 |

**The first row was re-measured 2026-10-15 (issue #53):** master now
configures 60 tests, this branch 61 — the +1 is `engine-d3-candidate`, the F9
candidate-window tier (see tier-D3-candidate below). Only the default-config
row was re-measured on this host; the three Android/trace rows above are the
2026-10-13 figures and were **not** re-measured here (no Android toolchain in
this environment), so they are carried forward, not re-verified. A later
measurement with the toolchain present should re-check them rather than trust
this line.

**The first two rows are not this change's** — `engine-android-espresso` does
not register there at all, so its only row is the third: +1, and only where the
Android toolchain is present. Rows 1 and 2 carry master's own drift since the
table above was written (+3 each); rows 3 and 4 drifted +1 each on master. The
drift is left un-attributed here rather than claimed either way.

**Re-measured again 2026-10-13, after the registration was actually added.** The
first measurement of this table was taken when the branch had no `add_test` at
all — only `tools/android-espresso.sh` and the CI step — so its branch column read
63/181 one lower than the branch's real count. A table that credits a
registration the tree does not contain is the defect issue #13 records, one
level down from the directory it started as; the numbers above come from
`ctest -N` on a freshly configured tree, `env -i` with only the row's variables.

Per-label, in the 59-test configuration. No test fails in this configuration
in a fresh worktree. The dump-dependent data selfcheck was unregistered
2026-10-17 (maintainer instruction — revisit at its own dev stage), so the
krdict converter's registered guard is `krdict-fixture`
(`tests/fixtures/krdict/`); the dump-dependent full run is the maintainer-only
`tools/selfcheck-krdict.sh`. On the maintainer's primary tree, where the dump
is present, the full check still runs by hand.

| Selection | Tests | Wall clock, `ctest -j16` |
|---|---|---|
| everything, no filter | 57 | 79.25 s |
| `-L tier-A` | 26 | 0.89 s |
| `-L 'tier-A\|tier-B'` | 29 | 0.96 s |
| `-L tier-B` | 3 | 0.77 s |
| `-L tier-C` | 16 | 1.26 s |
| `-L 'tier-D1\|tier-D2' -LE mutation` | 4 | 0.09 s |
| `-L tier-D` | 8 | 49.76 s |
| `-L tier-E1` | 2 | 27.22 s |
| `-L tier-G` | 2 | 3.32 s |
| `-L slow` | 7 | 78.15 s |
| `-L mutation` | 2 | 54.05 s |
| `-LE slow` | 50 | 1.87 s |
| `-LE mutation` | 55 | 52.81 s |

The shape worth knowing: **95% of the serial sum is seven tests** — tier D3 ×3,
the two mutation tiers, `docs-lint` and `engine-android-e1` — and every other
test in the suite costs under 2 s. `-LE slow` is therefore not a marginal
convenience: it turns a 78 s run into a 1.9 s one. `-L tier-D` at 49.8 s is
entirely its three D3 tests; D1 and D2 together are 0.23 s and
`frontend-invariants` 0.03 s, so a change to the fcitx5 adapter that does not
touch the full stack should select `-L 'tier-D1|tier-D2' -LE mutation` (0.09 s),
not `-L tier-D`.

ctest also prints a **Label Time Summary** at the end of any labelled run, which
is the cheapest way to see where a selection's time went:

```
Label Time Summary:
tier-A    =   1.47 sec*proc (26 tests)
```

### The three levels: commit < PR < nightly == release

The maintainer's ladder, 2026-10-16, with what each rung runs. **Cumulative** —
each level contains the ones below it, which is what makes "nightly == release"
mean something: a release gate that is stricter than what every PR ran has not
been measuring the same thing the PR measured.

| Level | Command | Measured, this host | Contains |
|---|---|---|---|
| **commit** | `ctest --test-dir build -j4 -L guard` | **19.4 s**, 7 tests | 4 guaranteed + 3 conditional — see below |
| **PR** | `ctest --test-dir build -j4 -LE 'tier-E1'` | ~78 s projected for CI; **251 s** local at `-j4` | everything above, plus tiers A–D, E0/E2, the mutation self-tests |
| **nightly == release** | `tools/release-gate.sh` | 234 s at `-j4`, 184 tests, 2 declared skips | everything, tier E1 included, plus the artefacts |

Two properties make the rungs trustworthy rather than aspirational, and both are
the reason the local numbers are quoted next to the CI ones rather than instead
of them (§2 above, and `docs/CI-TIMING.md`):

1. **A level's exclusions are declared, not implied.** `ctest -L` prints what it
   selected and nothing about what it did not, so a green selection is
   indistinguishable on its face from a green full run —
   `wiki/raw/measurements/2026-10-10-d3-case-coverage.md`. `tools/ctest-select.sh`
   closes that for a selection, and `tools/release-gate.sh` closes it for the
   release rung by failing on any skipped test that is not in its exception
   table, **and on a declared exception that stopped skipping** — an exception
   that no longer applies is a stale claim, not a free pass.
2. **The commit rung has to be under a minute**, or it stops being run — and it
   has to survive an **empty cache**, because the repository's Actions caches are
   currently empty (`total_count 0`: no job has executed since 2026-10-13, so
   neither `actions/cache` step ever completed). So the rung is split in two:

   | | tests | cost | if unavailable |
   |---|---|---|---|
   | **guaranteed** | `abi`, `schema-contract`, `docs-lint` (which also runs `wiki-lint` and verifies every mutation anchor), `shell-lint` | **~5 s** | cannot be unavailable — no SDK, no Gradle, no cache |
   | **conditional** | `builds` (engine), `builds-fcitx5-addon`, `builds-android` | **~14 s** warm | **skip, by name, with the cause printed** |

   The conditional legs are not free coverage and must never be read as it. A
   skip is exit 77, which ctest prints by name, and each one says what it did
   *not* check:

   ```
   builds-fcitx5-addon: SKIPPED -- no hunmin_addon target, which means
   Fcitx5Core >= 5.1.13 was not found when this tree was configured
   ```

   That is the shape that makes "the cache will be warm" unnecessary. On a cold
   Actions cache the fcitx5 addon cannot compile inside this rung at all — the
   linux job builds fcitx5 5.1.13 from source for 197 s because Ubuntu 24.04
   ships 5.1.7, below the addon's real floor (`docs/CI-TIMING.md`) — so on a cold
   cache the addon leg skips, and the rung is still honest and still under a
   minute. **A green `guard` therefore means "the guaranteed checks passed and
   the conditional ones that could run did"; it never means "the frontends
   compiled".**

   All three behaviours are verified by planting the failure, not by reading the
   green: a compile error in `libhunmin/src/ic.c` turns `builds` red in 0.11 s, a
   script without a shebang turns `shell-lint` red, and a tree configured with
   `-DCMAKE_DISABLE_FIND_PACKAGE_Fcitx5Core=ON` makes `builds-fcitx5-addon`
   **Skipped** with the message above rather than Passed.

**What the commit rung cannot do, stated so nobody assumes it:** with a cold
cache it compiles only `libhunmin` — the fcitx5 addon and the Android frontend
skip by name. It runs no tier tests beyond `abi`/`schema-contract`, and no
linting of C or Kotlin: `shell-lint` covers the shell at `-S error` severity,
and `docs-lint` covers the Python by type-checking `tools/` with pyright (the
(M5-CLOSED row). That check is evaluated on whatever host runs it, and `tools/`
holds Windows host tools: `pyrightconfig.json` pins `pythonPlatform` to `Windows`
so a Linux build resolves `winreg` instead of reporting every attribute of it as
unknown (measured 2026-10-29 on the .deb gate: 21 errors on Linux, all `winreg`
in `tools/tsf-devhost-round.py`; 0 with the setting on either platform).
clang-tidy and clang-format are on the GitHub image and unused;
adding CI gating for them is a separate decision about how much noise this
project is willing to gate on. clang-format itself *is* the tree's formatting
standard since 2026-10-17 (issue #77, option 2) — retuned 2026-10-24 to the
user-requested whole-tree shape: a root `.clang-format` for the C core
(4-space, `ColumnLimit: 150`, reflowed comments, sorted includes) and
per-directory configs for the two C++ frontends (4-space, `public:` flush
with `class` via `AccessModifierOffset: -4`, `ColumnLimit: 150`), plus a
`.clang-format-ignore` for the vendored
and generated trees. The machine-checkable state is `clang-format -n` reporting
no changes on tracked files after a clean application commit; run
`clang-format -i` on the files you touch and let the second pass be idempotent.

**Why the commit rung is cheap is not obvious and is worth stating:** the two
real defects this ladder exists to catch were both *text* drifting from *text*.
A rename in `hunminadapter.cpp` moved a mutation anchor out from under
`frontend-mutations` and no `.deb` could be built (2026-10-16); the same class of
drift had already broken three `ci.yml` comments and two `README.md` paragraphs.
`docs-lint` check 7 verifies every mutation anchor against its source in
milliseconds, which is the guard that ran 26 seconds earlier than the self-test
did and with a build in between.

### Make the selection visible

`ctest -L` prints what it selected and says nothing about what it did not, so a
green selected run is indistinguishable on its face from a green full run. That
is the exact shape of the defect in
`wiki/raw/measurements/2026-10-10-d3-case-coverage.md`, where `engine-d3-x11`
reported `Passed` in 5.01 s having run one of its eight cases and nothing
errored. `tools/ctest-select.sh` is the wrapper that closes it:

```sh
tools/ctest-select.sh -L tier-D      # prints the selection AND the omissions, then runs
tools/ctest-select.sh -n -L tier-D   # print only
```

It names every omitted test before running anything, and it derives the
selected set by asking `ctest -N` for it rather than re-reading the label
vocabulary — a wrapper that selects something other than what it printed is
worse than no wrapper.

### One CMake trap, recorded because it is silent

`LABELS` is a test **property**, not an `add_test()` keyword:

```cmake
add_test(NAME x COMMAND y LABELS tier-A)      # WRONG, and it does not error
set_tests_properties(x PROPERTIES LABELS "tier-A")   # right
```

The first form appends `LABELS` and `tier-A` to the command line as two more
`argv` entries, `ctest -N` still reports the same test count, and configure
still succeeds. It was measured here: the first pass of this change used it for
all 57 registrations and `ctest -N -L tier-A` reported `Total Tests: 0`.

---

## Tier A — Engine unit + golden tests (host, C) 🤖📦

- **Golden transcripts** `tests/golden/<layout>.jsonl` — one file per layout, one case per
  line: `{keys: [{k,mods}], expect: [{commit?, preedit?, candidates?, consumed}]}`.
  Runner: `hunmin-sim --check tests/golden/*.jsonl`; regen for review: `tools/regen-goldens.sh`
  (NEVER blind-accept; diffs reviewed line-by-line). There is no `make retrace` — this repo
  has no Makefile; the script is the command.
  Sources: pat.im worked examples (갭/겹, `cq`→ㄱㅅ, `cQ`→ㄳ, 있 repeat, tier samples),
  kime test expectations (transcribed facts, clean-room), one case per line of SPEC-P2.md.
- **Property fuzz** (`-fsanitize=fuzzer,address,undefined`, corpus = jamo key streams):
  invariants only — every committed string NFC-valid & re-decomposes to current-buffer
  jamo; `press` never leaks (LSan); state always resettable; preedit length bounded.
- **Differential fuzz vs libhangul** (system `.so`, dlopen harness): random streams on the
  *shared* layouts (2/2y/32/39/3f/3s/3y/ro/ahn) must produce byte-identical commit +
  preedit sequences. This is the compatibility guarantee for "others" adopting hunmin.

## Tier B — ABI & data contracts 🤖

- **ABI guard**: `libhunmin/tests/abi/symbols.txt` vs `nm --defined-only -g libhunmin.so`
  (the path is under `libhunmin/`, not `tests/`); any removal/rename fails CI. The policy
  and the exact re-emit command live in `AGENTS.md` §Hard rules and in
  `libhunmin/include/hunmin.h` — not twice here — and the failing `abi` test prints the
  command, so CI hands it to whoever broke it.
  Header compiles as C99/C11/C++17 `-Werror` on gcc/clang/**MSVC/MinGW/NDK-clang** matrix
  (portability is a tested claim, not a hope).
  ⚠️ Partly aspirational: CI builds the core on linux-gcc and mingw, and there is no
  C99/C11/C++17 `-Werror` header compile. The toolchains do get real coverage; the
  language-standard matrix is not tested. *(Corrected 2026-10-09: this used to also claim
  "no NDK-clang job at all" on the evidence of `grep -c android .github/workflows/ci.yml`
  = 0. That grep is 19, and the `android` job installs NDK 27.3.13750724 and cross-compiles
  the core with its clang — so the NDK half of the claim was already false.)*
  *(Corrected again 2026-10-13: the MSVC half is now false too — the `windows` job is
  disabled per issue #51, so no job compiles the core with `cl`. The bold "MSVC" in the
  claim above still stands as the intent, not as coverage.)*
- **`hunmin-doctor`** (data-file gate for external contributors): JSON-schema validation +
  semantic lints — duplicate triggers, unreachable modifier combos, symbol-tier key
  collisions, rule tables referencing unknown jamo ids, hanja rules dangling. Nonzero exit
  with actionable messages; run in CI on every `data/` PR.
  ⚠️ Two corrections. "hanja rules dangling" is **not implemented** — `tools/doctor.py`
  globs `compose/` and `layouts/` only, so the three hanja directories are unlinted and a
  data PR touching them is not actually gated. And "every `data/` PR" is narrower than it
  sounds: nothing validates the hanja files' *shape* (a hand-edited `krdict.json` with a
  typo'd key passes today). Their schemas are recorded in SPEC-SCHEMA.md but have no
  validator yet.
- **Schema versioning**: loader rejects newer `schemaVersion` with a clear error (forward
  compat of third-party data).

## Tier C — Cross-platform adapter conformance (the linchpin) 🤖📦

Problem: each platform encodes keys differently (X keysyms vs Android KeyEvents vs Windows
VK codes) and presents text buffers differently. Solution:

1. **`docs/trace-spec.md`**: a normalized interaction format —
   `in: <platform-event>  out: [commit "…", preedit "…" @sel, candidates […], consumed|pass]`.
2. Every frontend builds with `HUNMIN_TRACE=1` → emits this trace for a given input stream.
3. Shared corpus `tests/conformance/*.trace.{in,gold}` — same sequences as Tier A but with
   *platform-native* events. **Gate:** all frontends must diff-clean against the corpus.
   A new platform (e.g. Windows TIP) is "engine-correct" iff it passes C + its own Tier F.
4. **Keycode-map fixture tests**: per platform, a generated table test (headers as source
   of truth: XKB keysyms / `android.view.KeyEvent` / Win `winuser.h`) asserting
   (platform event, mods) → hunmin key id for *every key the layouts use*, both shift and
   alt, plus CapsLock interaction. These tables are where adapters actually break.

⚠️ **Status (2026-10-03): steps 1 and 3 exist for fcitx5/X11; 2 and 4 do not.**

Implemented: `docs/trace-spec.md` (the format, with its invariants); the corpus under
`tests/conformance/<platform>/`, **generated** from tier A goldens by
`tools/gen-conformance.py` so the two tiers cannot drift; `tools/trace-check.py`, which
replays a trace through a real adapter and diffs it, exit 1 on mismatch and 77 to skip;
and `frontends/fcitx5/test/trace-emit.cpp`, which does that for fcitx5/X11.

**87 traces, 201 events**, registered as ctest
`conformance-*` (`-DHUNMIN_TRACE_BUILD=ON`). The corpus was reduced 2026-10-25 for
word buffering (#74): the 31 multi-syllable cases whose per-syllable expectations the
buffered adapter cannot reproduce, and the nine `ref-*` corpora (libhangul parity,
401 events each), were dropped rather than re-emitted. libhangul parity is asserted at
tier A (engine boundary) only until the `ref-*` traces are regenerated against the
buffered adapter. Every remaining trace was re-checked against its tier A case: zero
divergence.

Three properties the generator had to get right, each found by making the gate
adversarial rather than by reading the code:

- A golden's `{"meta": {"case": ...}}` lines are **independent** cases, each replayed
  from a clean engine; concatenating them leaks composition across case boundaries. One
  trace file per case, with a stable ASCII `case_index`/`case_id` in the filename —
  case labels are Hangul and may contain `;`, neither usable in a CMake test name.
- `consumed` **cannot be derived from tier A** (which records only commit and preedit) and
  was guessed wrong twice. It is now derived: a character is consumed iff the *layout's
  key table* maps it after shift normalisation, and backspace iff the previous line's
  preedit was non-empty.
- The runner compares `.in` against `.gold` **before** comparing the adapter's output.
  Without that, a wrong expectation in the `.in` is asserted against a different authority
  than the preedit beside it and passes silently.

Step 2 is `-DHUNMIN_TRACE=ON` (not a `HUNMIN_TRACE=1` *environment* variable): the sink
lives in the shipped addon, `#ifdef`'d out otherwise, writing JSONL to `HUNMIN_TRACE_OUT`
or stderr. It belongs in the shipped library rather than only in the test harness because
D1/D2/Tier C already `dlopen` the installed `libhunmin.so` — the traced binary is the
shipped one. Ctest `trace-optout` asserts the packaged build contains neither the env-var
name nor the write literal, so "compiled out" is checked rather than asserted.

Step 4 is ctest `keymap-fixture`: every key a layout can use (47 across 12 layouts) must
have an encoding on each platform, checked against **the platform's own header**
(`/usr/include/xkbcommon/xkbcommon-keysyms.h`, 2,630 defines parsed) rather than against a
table that would merely restate the code under test. It asserts the printable range really
is ASCII-keysym, that named values match, and that anything unreachable on Android is
declared — `F9` has no `KEYCODE_F9`, because on Android the function row is a modifier
*combination*, which is the same fact that forces F9 into the adapter everywhere.
CapsLock and modifiers are now covered deliberately rather than incidentally.

Between them the tiers leave no gap: the fixture proves every key has a correct native
encoding, and the 87 traces prove the adapter applies it.

**Equivalence extension (2026-10-24, requirement).** The rule in
`docs/FRONTEND-EQUIVALENCE.md` makes this tier the cross-frontend authority:
the same key sequence must produce the same commit, preedit, and consumed
decision in fcitx5, Android, and TSF, and TSF's interim and native delivery
modes must agree. The corpus must be replayed in both buffered and syllable
commit modes; commit boundaries are not part of the expectation. The existing
Windows emitter (`frontends/tsf/test/trace-emit.cpp`, tier F1) already replays
the win32 dialect through the mock; it must run once per delivery mode, and
the expectations must not be duplicated per frontend.

The Android corpus **skips** (rc 77), but not because the Android frontend is missing —
`frontends/android/` exists and is exercised by tiers E0–E2. The gap is narrower and is
specific: `tests/conformance/android/` holds one `.trace.in` and **no `.gold`**, and no
Android `trace-emit` runner is built, so there is nothing for `trace-check.py` to diff.
Generating the goldens needs the runner, and the runner is the missing piece. **With P6
skipped, fcitx5 and Android remain the only two frontends and the shared corpus is what
shows they agree** — and that proof is still missing on the Android side, which is why
PLAN.md gates P5 on this tier.

## Tier D — Desktop integration: real fcitx5 🤖 (Linux CI)

Four layers, cheap→real:

- **D1 in-process addon test** (ctest): link the addon sources into a test binary,
  instantiate `fcitx::Instance` headless + `InputContext` directly, inject `KeyEvent`s,
  assert entry commit/preedit strings. Pattern exists in fcitx5's own `test/` suite
  (`testinstance.cpp`, `eventlooptests.*`, `xvfb_wrapper.sh` in the local 5.1.19 tree).
  Covers: key routing, config keys, layout switching, hanja mode toggles, focus events.
- **D2 fake-UI capture**: preedit + candidate *content* asserted in-process, no pixels.
  ⚠️ Correction (2026-10-02): this tier previously cited
  `fcitx5-5.1.19/test/testuserinterfacemanager.cpp` as the UI fake. It is not one — that
  file only tests `UserInterfaceManager` action registration. fcitx5 5.1.19 ships no
  assert-capable UI fake; `testing/testui/` prints status area and input panel to stdout,
  which cannot be asserted against.
  What is actually available: `InputPanel::candidateList()` / `setCandidateList()`
  (`src/lib/fcitx/inputpanel.h:67-68`) and `setClientPreedit()` / `setPreedit()`. A D2 test
  is a D1-style headless `Instance` that reads those accessors after `updateUI()` and
  asserts texts and ordering. Note `UserInterface::update()` is only ever called with
  `InputPanel` or `StatusArea` (`userinterface.h:21-31`) — there is no candidate-specific
  UI hook, so candidates must be read off the `InputPanel`, not from UI callbacks.
  Implemented 2026-10-02 as ctest `engine-d2-ui`
  (`frontends/fcitx5/test/testhunminui.cpp`), preedit-only as predicted above.
  It asserts: per-keystroke preedit text, clearing on flush and on a rejected
  key, both capability branches (`setPreedit` vs `setClientPreedit`) and that
  only one of them is written, and cursor placement. Expectations are
  `hunmin-sim --trace` for sin3-p2, so tier A remains the authority.
  Mutation-verified: dropping `setPreedit`, dropping `setClientPreedit`,
  dropping `setCursor`, or dropping `inputPanel().reset()` each makes it fail
  while D1 still passes — see `tools/frontend-mutations.py`.
  ✅ Candidate assertions added 2026-10-03 when P3 landed. `testhunminui.cpp:168-200`
  opens the Hanja window with F9 over a candidate preedit, then asserts the list is
  non-null and non-empty, that `candidate(0).comment()` is the `hanja` kind label, and
  that `candidate(0).text()` is U+4B0C — the single candidate for 푸 in `hanja.json`.
  *(This line used to read "Candidate assertions remain impossible until P3 adds
  candidates." It was true when written and is now false; the prediction was correct and
  the prediction was kept as though it were the present tense.)*
- **D3 full-stack X11 e2e**: launch **stock distro `fcitx5`** under Xvfb; start our harness
  app (tiny XIM client window echoing every received commit as `COMMIT:<utf8>` on stdout,
  `tests/e2e/xim-harness.c`); drive a real input path with **XTEST** (libXtst via ctypes,
  script `tests/e2e/xtest-type.py`). Xvfb has no evdev/uinput backend and `/dev/uinput` is
  root-only in CI, so injection enters at the X server instead of at HID level; everything
  *below* X (XKB → fcitx5 XIM → addon → libhunmin → client buffer) is real.
  Test bodies = golden sequences from Tier A, expectations computed at run time by
  `hunmin-sim --trace` so tier A stays the single source of truth → expected COMMIT lines.
  Runner: `testfcitx5d3` wrapping `tests/e2e/run-d3.sh` (ctest `engine-d3-x11`, skips rc 77 without Xvfb/fcitx5).
  ✅ Status: **green** (2026-10-03). All three cases pass and the full `ctest`
  suite passes (ctest is green with `-DHUNMIN_TRACE_BUILD=ON`,
  up from 140 at the P5 merge, which added
  `engine-commit-text` and `engine-android-goldens`; the latter skips rc 77
  without a device). Expectation line for `dubeolsik/dkssuds` is `안|녕`, and the
  client receives exactly that.
  ⚠️ **It was green-looking and measuring nothing until 2026-10-03.** The
  harness called `XOpenIM()`/`XCreateIC()` while linking only `X11::X11`, so the
  client ran libX11's *built-in* XIM against fcitx5's libxcb-imdkit *server*.
  libX11's XIM answers with its own local IM when no server is reachable, so it
  returned non-NULL, decoded every key itself, and D3 compared libX11's output
  against tier A. The old symptom (`expected: 안|녕` / `actual: d|k|s|s|u|d|s`)
  was therefore not the adapter missing keys — those were keys libX11 never sent
  to fcitx5. The harness is now written against dkit's `xcb_xim_*` client API and
  linked against `libxcb-imdkit`.
  Four defects, each found only after the one before it was removed:
  1. *Fixed.* The addon was not loaded at all — `run-d3.sh` staged it under
     `$STAGE/pkgdata/fcitx5/addon` while fcitx5 looks in `$STAGE/pkgdata/addon`,
     because `FCITX_DATA_DIRS` entries are used verbatim.
  2. *Fixed, non-issue.* Focus and nesting were suspected and are ruled out by
     measurement: a standalone probe confirms `XSetInputFocus` takes effect under
     Xvfb and XTEST delivers; Xephyr changes nothing.
  3. *Fixed.* `fcitx5-remote -s hunmin` returns **rc=0** and changes nothing —
     `Instance::setCurrentInputMethod` returns silently when no context is
     focused. It is removed, not retried.
  4. *Fixed.* Two independent ways the engine was misconfigured and the
     expectation still lined up. The profile's **second** item wins the default
     IM regardless of `DefaultIM=` (`InputMethodGroup`,
     inputmethodgroup.cpp:100), and the layout was written to
     `data/fcitx5/conf/hunmin.conf` — a config file the addon never reads —
     instead of the profile's per-item `Layout=`. With the layout empty the
     engine used its `sin3-p2` default, so the two `sin3-p2` cases passed **for
     the wrong reason** while `dkssuds` produced `ㅎ|곈`.
  ⚠️ **It was green-looking and measuring one case in eight until 2026-10-10.**
  A fifth defect, in the same family as the four above: the tier reported
  `Passed` in 5.01s having run `dubeolsik/dkssuds` and nothing else. The cause
  was the case-marker machinery introduced to reuse one harness per layout
  group. `xim-harness.c` polled `STDIN_FILENO` in its event loop and drained it,
  and `run-d3.sh` fed its case list to the main loop on **stdin** — so the
  harness ate `cases.tsv` 256 bytes at a time until the file was at EOF and
  `while read` stopped. Nothing errored; the loop simply had no more input.
  The case that ran is the first of the first group, which is the one case that
  cannot inherit state from another, so the defect also hid the reuse problem
  below. The harness's own comment said "no case-id traffic is expected on
  stdin in this design" and then drained it anyway; the arm is now gone, the
  runner starts the harness with `</dev/null`, and the runner reads its case
  list on **fd 3** so no process it starts can eat it again. With the eight
  cases actually running, `engine-d3-x11` takes ~46s and all eight pass.
  ⚠️ **Harness reuse is not sound for the XIM client, measured 2026-10-10.**
  One XIC is kept for the whole layout group, and an XIC carries the
  server-side composition with it. **Four of the eight cases end with a preedit
  still in flight** (`hunmin-sim --trace`: `dubeolsik/dkssuds` → ㄴ,
  `sin3-p2 kfcQ` → ㅒ, and 세 / 를 for the two long `stream` cases), so the next
  case in the same group starts on top of it: `sin3-p2 jk1` came back
  `COMMIT:ㅒ|COMMIT:ㅇµ` where tier A says `COMMIT:ㅇµ`. An input-context focus
  cycle (`xcb_xim_unset_ic_focus` / `xcb_xim_set_ic_focus`) was tried as the
  reset and did **not** clear the preedit. `engine-d3-x11` therefore starts a
  fresh harness per case again; what that costs is ~1.1s, because this harness
  starts in 0.14s — not the ~5s its own comment claimed, which is where the
  "80% of the tier" figure came from. A per-case input context is
  the untried option, and is a maintainer call, not a settled one.
  harness (stdout is a redirect `run-d3.sh` greps, so `READY` would sit in the
  buffer), the readiness gate waiting for `Loaded addon hunmin` and not just
  `@server=fcitx`, and `FCITX_LOG_RULE` being a **variable fcitx5 does not read** —
  the log rule is `--verbose <rule>`, and `XIM Key Event` is category `xim_key`,
  not `xim`.
  Full account with measurements: `docs/D3-ROOT-CAUSE.md`. The wrong turns are
  kept in `docs/FCITX5-ADDON-NOTES.md` §11b–§11d so they are not repeated.
  ⏳ **Still not covered:** every non-XIM client. A declined key becomes text on
  the GTK path too (D3-GTK below, measured 2026-10-09) but by a different
  mechanism in each module, and the `FORWARD:` line above is a statement about
  fcitx5's XIM server only — it does not transfer to Qt or to Wayland.
  **The candidate window is now covered — by `engine-d3-candidate`, not here.**
  See the tier-D3-candidate section below; the F9 window was drawn for the
  first time in any ctest tier there (2026-10-15, issue #53). What remains
  true here: an F9 *consumption* case does not belong in this XIM run. An F9
  case was added to the ordinary D3 corpus and removed after measurement: two
  mutations of the adapter (dropping `filterAndAccept()`, flushing inside
  `openCandidates()`) both left the ordinary D3 green and both turned D2 red,
  because its distinguishing property is *consumption* and the F9 keycode
  reaches the client either way, so the case would pass regardless. D2's
  `sendKeyEvent` return value remains the only place consumption is asserted.

  **Correction, 2026-10-09 — this paragraph previously said consumption was
  "unobservable from an XIM client at any level", on the grounds that "a
  declined key never becomes text either". That was wrong, and the way it was
  wrong is the reason to record it.** The first half was right and still is:
  `nKeySym` is structurally 0, because fcitx5 sends `XCB_XIM_LOOKUP_CHARS` and
  that XIM message type has no keysym field. But the conclusion drawn from it
  was not. A declined key *does* become text, over a different channel:
  `XIM_FORWARD_EVENT`. fcitx5's XIM server forwards any key the IM declined
  (`src/frontend/xim/xim.cpp:664`, `if (!result) xcb_im_forward_event(...)`),
  and a real toolkit turns that into text by running the key through its
  default input context. D3 could not see it because `tests/e2e/xim-harness.c`
  registered no `forward_event` callback, so dkit discarded every forwarded key
  (`clientprotocolhandler.c:502` tests the pointer for NULL before calling it).
  The absence of a space case from `cases.tsv` was therefore a property of the
  harness, not of the design, and the comment that said so was wrong about the
  reason.

  D3 observes forwarded keys now, as `FORWARD:` lines. What that buys, measured
  on `dkssuds<space>` under dubeolsik:

  ```
  COMMIT:안 | COMMIT:녕 | COMMIT:ㄴ | FORWARD:<space>
  ```

  The space ends the composition *and* is emitted. What it does not buy is the
  other XIM forward path: `instance.cpp:1672` re-forwards an *accepted* key for
  `KeyEventOrderFix`, which would make `FORWARD` ambiguous. It cannot reach an
  XIM client — it is gated on `CapabilityFlag::KeyEventOrderFix` and the XIM
  server only ever sets `ReportKeyRepeat`/`Preedit`/`FormattedPreedit`
  (`xim.cpp:270`) — so every `FORWARD` in D3 is a decline. That is a property
  of the XIM frontend, not of D3, and it is why the ambiguity does not exist
  here rather than why it was never checked.
  Measurements: `docs/D3-ROOT-CAUSE.md`.

- **D3-GTK — the same full stack, a GTK client** ✅ (2026-10-09, ctest
  `engine-d3-gtk3` / `engine-d3-gtk4`): the question the XIM result cannot
  answer, because a GTK application never speaks XIM. `run-d3.sh` with
  `HUNMIN_D3_CLIENT=gtk` reuses everything above the X server — private bus,
  staged addon, `FCITX_*` dirs, the `Loaded addon hunmin` gate, `cases.tsv`,
  XTEST injection — and swaps only the client for `tests/e2e/gtk-harness.py`:
  a real `GtkEntry` in a real `libgtk`, driven by real `GdkEventKey`s, with
  fcitx5's own GTK input method module loaded into the process.
  Measured: the key the engine declines becomes text on both GTK versions.
  `dkssuds<space>` under dubeolsik leaves `TEXT:안녕ㄴ ` — the three commits and
  the space — matching what `hunmin-sim --trace` says the client should hold.
  The expectation is still derived at run time from tier A; for a widget the
  trace is folded into one string (`expected_text`) because a `GtkEntry` has one
  buffer, not an event stream.
  **Two tests, not one, because the two modules are not the same code.** GTK 3
  re-queues the declined key from fcitx5's `ForwardKey` D-Bus signal with
  `IgnoredMask` set, through an xkb compose state, into a `GtkIMContextSimple`
  slave (`gtk3/fcitximcontext.cpp:1299`, `:530`, `:348`). GTK 4's
  `_fcitx_im_context_forward_key_cb` is an **empty function**
  (`gtk4/fcitximcontext.cpp:1228`), `ForwardKey` is never seen on the bus, and
  the character arrives because a declined event is re-queued by pointer
  identity into `pending_events` and handed to the same slave
  (`:537`, `:486`). Confirmed by `dbus-monitor` on the private D3 bus: 3
  `CommitString` + 1 `ForwardKey`(keysym 32) under GTK 3, no `ForwardKey` under
  GTK 4. One test over both would have kept the answer and lost the mechanism.
  ⚠️ Three harness facts, each of which produced a *passing* run while measuring
  something else. (1) `GtkEntry.has_focus()` is `False` indefinitely under a
  bare Xvfb for GTK 3 **and also for GTK 4 — where keys nevertheless arrive**
  and `Gtk.Window.is_active()` is `True`; gating on `has_focus()` reports
  "harness never became ready" for a harness receiving every key. GTK 4 is
  focused with `xdotool windowfocus` because its toplevel is a `GdkSurface` and
  the `GdkX11` 4.0 typelib exposes no xid. (2) The first `python3` on `PATH`
  here is a venv without `gi`, and the failure surfaced as "harness never
  became ready" — a statement about the input method that was really about the
  interpreter; the runner now probes for a `python3` that can `import gi` and
  prints which. (3) **With `GTK_IM_MODULE=xim` every case passes** — `run-d3.sh`
  exports `XMODIFIERS=@im=fcitx` for the XIM server and GTK's XIM module honours
  it, so the buffer is right by the XIM path this test exists to distinguish
  from. The harness prints the `im-fcitx5.so` mapped into its own
  `/proc/self/maps` and the
  runner **refuses** a GTK case without it; the `xim` control now fails.
  Falsified both ways: dropping the space from the expectation fails the case,
  and adding `" "` to dubeolsik's key table so the engine *consumes* the space
  changes the buffer to `TEXT:안녕ㄴ` and the case passes — the result tracks the
  engine's `consumed` flag, so it is evidence about the declined key and not
  about GTK inserting a space regardless.
  ✅ **The harness is reused per layout group (2026-10-10), and the per-case
  reset is a fresh `GtkEntry` rather than a cleared one.** Reuse needs the
  client to start each case from nothing, and two things would otherwise carry
  over: the buffer, which accumulates, and the input context, which carries the
  composition — the same four cases with a trailing preedit that make XIM reuse
  unsound above. A new `GtkEntry` gives a new buffer **and** a new
  `GtkIMContext`, hence a new fcitx5 input context; `entry.set_text("")` would
  clear only the first. The case marker is printed only once the new entry holds
  the X focus, so it is a stronger gate than the `sleep 0.4` it sits in front
  of. Two measured facts behind it: `Gtk.Window.get_focus()` under GTK 4 is
  **never** the `GtkEntry` — it is the `GtkText` the entry is built on, parent
  chain `Text → Entry → Window` — so the check walks up from the focus widget,
  and asking for the entry directly makes every case time out. And the settle
  `sleep 0.4` is still required: the marker proves focus, not that the toolkit
  has finished connecting that entry's `GtkIMContext` to fcitx5, and there is no
  cheap signal for the second (`fcitx5-remote` answers in 3ms and reports the
  same thing every case; `fcitx5-diagnose` does report contexts and costs 3.0s a
  call). Removing it was tried and is not safe: over six GTK 4 runs at zero
  settle, one case returned `TEXT:dㅏㄴ녕ㄴ` where tier A says `TEXT:안녕ㄴ`, the
  leading `d` having reached the widget with no engine behind it.
  Falsified by construction as well as by comparison: with `make_entry()`
  stubbed out to keep the existing entry, every case that is not first in its
  layout group fails on both GTK versions (4 of 8), and with the reset in place
  the reused-harness run's eight case lines are byte-for-byte identical to the
  per-case-harness run's on both.
  ❌ **It does not make the tier fast, and the reason is not the harness.**
  Measured 2026-10-10, the ~50s GTK run is: ~24s inside `xtest-type.py`, whose
  default 40ms-per-event delay over the four long-text cases is 44% of it;
  ~10s in four fcitx5 restarts (~2.6s each); ~8s in fixed sleeps; and **~4s in
  harness startup, 8% of the total** — a GTK harness reaches `READY` in 0.56s,
  not "several seconds". Reuse therefore buys ~2s, and reaching ~15s would mean
  changing the injector's pacing or the fcitx5 restart gate, which is a
  different change with a different risk. See `docs/D3-PERFORMANCE.md`.
  ⚠️ The client is written against GObject introspection, **not** compiled
  against `gtk/gtk.h`: `pkg-config --exists gtk+-3.0 gtk4` fails on this host
  (`libgtk-3-dev`/`libgtk4-dev` absent, `sudo -n` unavailable). The toolkit,
  the `GtkIMContext` multicontext and the fcitx5 module under test are the real
  ones — the module is loaded into the GTK process either way — so the binding
  is the only difference; the tests **skip** (rc 77) where the runtime, the
  module or `xdotool` is missing rather than failing. A real application
  (`gnome-text-editor` is installed) is not driven: input hints, IM-module
  selection and the candidate window are all still unexercised.
  **Still not covered:** Wayland `text-input-v3` — GTK over X11 does **not**
  stand in for it, and D4 stands unchanged — and Qt
  (`fcitx5-frontend-qt5`/`-qt6`, a third fallback implementation).
  Measurements: `wiki/raw/measurements/2026-10-09-gtk-forward-path.md`.

- **D3-CANDIDATE — the F9 candidate window, drawn for real** ✅ (2026-10-15,
  issue #53, ctest `engine-d3-candidate`): the tier no other one could be.
  `run-d3.sh` with `HUNMIN_D3_CLIENT=candidate` reuses the whole XIM stack —
  private bus, staged addon, XTEST injection, `xwininfo` oracle — with **one**
  difference: `classicui` is loaded, so the F9 candidate window is actually
  drawn. The abort in #45 lives exactly here: `classicui` calls `label()` once
  per candidate on the page, and a `CommonCandidateList` that never calls
  `setLabels()` holds 10 — past that, `label()` throws `std::invalid_argument`,
  nothing catches it, and fcitx5 SIGABRTs. `engine-d2-ui` loads no UI addon at
  all and the ordinary D3 lists `DisabledAddons=classicui`, so both were
  structurally incapable of the crash; the tree fact (page size ≤ 10) was
  asserted by D2 while the failure it prevents was not.
  **Per case it asserts three things:** fcitx5 survives the keystrokes
  (`kill -0` — the abort oracle, checked before the window so a dead fcitx5
  reports as a death and not as a missing window); the input window is drawn
  (`xwininfo -root -children` finds `Fcitx5 Input Window` — a one-character
  preedit is ~27px, a candidate column 300–700px, so "is there a window at
  all" is answerable from geometry); and the expected COMMIT text, where the
  case carries one. **Measured red with the cap reverted (2026-10-15):**
  `kMaxCandidatePageSize` set to 1000 makes the deep case fail with
  `CommonCandidateList: invalid label idx` and a core dump — the #45 abort,
  reproduced by a ctest tier for the first time. Green with the cap.
  **Two injector facts, both measured.** `tests/e2e/xtest-type.py` gained
  `{F9}`/`{Return}`/`{Down}` named-keysym tokens (XK_F9 = 0xFFC6, not F5 — an
  earlier run sent F5 and the transcript read "F9 committed the syllable").
  Named keys must be braced: a bare `Return` is six Latin characters, and
  `kfs{F9}Return` committed `간|ᅟᅥᆸ|ㅋ|더` instead of `刊` until it was
  `kfs{F9}{Return}`. And F9 opens the window **asynchronously**, so the
  injector settles 0.8s after it: a `Return` at the default 40ms arrives
  before `candidateActive()` is true, is not handled as a candidate key,
  flushes the source syllable to the client, and the rest lands as Latin
  garbage. The case file `tests/e2e/cases-candidate.tsv` carries a
  hand-written expected-commit column (the sim cannot type `{F9}`, so a
  sim-derived expectation would be a fiction); the expectations were verified
  against `tools/repro-f9-candidates.sh` on the same build. Skips rc 77 like
  every D3 variant and shares the `reserve_display` reservation, so it cannot
  collide with a concurrent `engine-d3-x11` run.
  ⚠️ **What this test costs, measured 2026-10-15, and it is not free.**
  `engine-d3-candidate` is **16.41 s** on a 16-core host, serial (`ctest -j1`).
  The D3 tier goes from 143.53 s (x11 46.43 + gtk3 48.47 + gtk4 48.63) to
  **159.94 s — +16.41 s, +11.4%**. Stated plainly rather than buried: this
  change makes the suite measurably slower, and the reason it was accepted is
  that no other tier can fail on the #45 abort at all, not that the cost is
  small. Where the 16.41 s goes: one fcitx5 start (~2.6 s, same as every D3
  variant), one XIM harness start, and **~9 s of fixed settle** — the
  injector's 0.8 s after F9 plus the runner's 1.5 s, over four cases. The
  settle is the compressible part and it is the next thing to attack if the
  `slow` budget needs room; the fcitx5 start is not.
  **And the budget this lands in was already over.** The `slow` tier is
  **184.84 s** against the 60 s budget AGENTS.md sets — a 3× overrun that
  predates this test, because the three GTK/X11 D3 runs are ~48 s each. This
  test did not create the overrun and did not fit inside it; see
  `docs/CI-TIMING.md` and `tools/ci-time-budget.sh`, which is where the
  overrun is measured rather than argued about. The honest summary: tier D3
  as a whole needs a structural decision (fewer cases, shared fcitx5 across
  the four variants, or a `slow` budget that matches reality), and this test
  is one more reason to have it.

- **D4 Wayland plumbing (optional, later)**: `weston --backend=headless` + minimal
  `zwp_input_method_v2`/text-input-v3 client harness. Not blocking P4; fcitx5's Wayland
  module is not our code, so we treat it as environment risk documented, not tested.

## Tier E — Android integration 🤖 (emulator CI) + 🧑 device matrix

- **E0 engine-on-device** ✅ (2026-10-03): the tier-A goldens replayed through
  libhunmin *on the device*, before any adapter exists.
  `tools/build-android.sh <abi>` cross-compiles the core with the NDK's clang
  (no ndk-build; `cmake/android.toolchain.cmake`), and
  `tools/android-golden-check.sh` pushes the binaries, the shipped `data/` and
  `tests/golden/` and diffs all 21 corpora — ctest `engine-android-goldens`,
  rc 77 without a device or SDK. This is the only tier that runs on bionic, and
  it is here because it earned its place: replaying the goldens on the emulator
  found engine output that depended on the C library (compose dedup after an
  unstable `qsort` — glibc `ᄈᅠᆸ`, bionic `ㅃ`, same source and same data).
  **A host-only tier structurally cannot see that class of bug**, so "the suite
  is green" on Linux says nothing about Android until this passes.
  Mutation-tested: restoring the unstable dedup turns it red on-device while the
  host suite stays green.
- **E1 JVM unit** ✅ (2026-10-03): 19 tests, `ctest engine-android-e1`, via
  `tools/android-unit.sh`. The adapter runs against the REAL engine — the same
  `hunmin-jni.c` the APK ships, compiled for the host and linked to the host
  `build/libhunmin.so` (`tools/build-host-jni.sh`) — and a
  `RecordingInputConnection` that records `commitText`/`setComposingText` and
  routes every surrounding-text call to a `violations` list, so invariant I9 is
  checked rather than assumed.
  Two harness details earn their own note, because both produced a suite that
  was **green while measuring nothing**: Gradle reports `BUILD SUCCESSFUL` when
  every test was SKIPPED, and JUnit 4 records an initialisation error in
  `@Before` as a *skipped* test rather than a failure. `tools/android-unit.sh`
  therefore counts skipped tests out of the XML and fails if any were skipped,
  and passes `--rerun-tasks` because the test task is keyed on test classes
  only, so an env-var change leaves the previous run's report on disk.
  Expectations come from the repo's own tier-A corpora, replayed **case by
  case** (each `{"meta":{"case":...}}` opens a fresh `hunmin_ic`, as
  `hunmin-sim` does); concatenating the cases into one stream interleaves them
  and compares nonsense. `ctest android-mutations` plants nine defects and
  requires all nine to be caught; a tenth candidate is deliberately NOT planted
  because it was measured to be unobservable, and is documented as such in both
  `HunminAdapterTest` and `tools/android-mutations.py`. The tier runs the
  unmutated suite once before planting anything, because a checkout that is
  already red reports all nine as "caught" for the wrong reason; see
  `docs/OPTIONS-MEASURED.md` §AND-4.2 for the measurement and §AND-2/§AND-3 for
  the harness, and what its sandbox strategy costs.
  **The 2026-10-11 character-loss trio** is asserted here, because every one of
  the three bugs was invisible on the build machine and each lost text the user
  had already typed. The 한/영 toggle lost the composing syllable (no flush);
  a bare Alt — its own `ACTION_DOWN` before a Hanja chord (`Alt+X` at the time;
  the fixed key is EISU since 2026-10-11, issue #12) — reached
  `HunminAdapter.onKey`, whose modifier path flushed it; and the picked Hanja was
  never committed at all, because an IME is a Service and receives no activity
  result. The three tests in `HunminAdapterTest` state the shared property —
  *whatever interrupts a composition, the composing characters reach the client:
  committed or still composing, never neither* — and they caught a real defect
  while being written: the Hanja path went through `commitExternalText`, whose
  contract is invariant 5's voice pass-through, so it appended the Hanja after
  the Hangul (`가恨`) instead of replacing it. `HunminAdapter.commitHanja` is the
  replacement path, and the test that found the bug is the one that guards it.
- **align-16k** ✅ (2026-10-11): `ctest align-16k`, `tools/check-align-16k.py`.
  Android 15+ on 16KB-page devices refuses an install whose native libraries are
  not 16KB-aligned, and the failure never appears on the build machine — it
  compiles, links, packages, installs, and only the device complains. The bug it
  was written for was a two-build-system split: `libhunmin.so` gets
  `-Wl,-z,max/common-page-size=16384` from `cmake/android.toolchain.cmake`, but
  `libhunjni.so` is built by AGP's `externalNativeBuild`, a separate CMake
  project that does not inherit it, so the shim shipped at the 4KB default and
  the runtime reported *both* libraries as unaligned. The check reads every
  PT_LOAD `p_align` out of the APK's libraries **and** runs `zipalign -c -P 16`:
  the ELF header align and the stored-offset align are separate mechanisms and
  neither implies the other. Verified to fail by patching a real `libhunjni.so`
  back to `p_align 0x1000`. rc 77 with no APK, so the inner loop stays green —
  build `tools/build-android.sh <abi>` + `./gradlew assembleDebug` first.
- **E2 emulator e2e** ✅ (2026-10-03, `ctest engine-android-e2e`,
  `tools/android-e2e.sh`): the real APK on the emulator, asserted end to end.
  Install → `ime enable` + `ime set` → focus a real `EditText` → inject real key
  events → read the committed text back out of the platform's own view
  hierarchy with `uiautomator`.
  **The assertion is the on-screen text, never a log line.** A trace sink is our
  code judging our own code: a wrong adapter would be wrong in its log too and
  the test would pass. `uiautomator` has no knowledge of hunmin. Measured:
  `dkssud` under `sin3-p2` puts exactly `ㅎ곈디` in the field, byte-identical to
  `hunmin-sim --trace`.
  This tier exists because E1 structurally cannot fail here: key events reach an
  IME through the IME window, and an `InputMethodService` with no input view has
  no such window. Verified on the platform: `mShowInInputMethodPicker=true`, the
  service binds, and one `input keyevent KEYCODE_K` alone produces `ㄱ` in the
  field.
  The editor is the `testeditor` APK (`frontends/android/testeditor`), a
  separate fixture with one `EditText` whose id is
  `org.hunmin.testeditor:id/editor`. It replaced Settings' search field, whose
  widget differs by image and which restores its previous query across a
  force-stop; `pm clear` on the fixture starts every run empty, and clearing it
  cannot destroy a user's data because it holds none.
  Each run force-stops the IME and removes `shared_prefs/hunmin.xml` (the selected
  layout and the Hangul/English mode). The tier's final F9 leaves English
  selected; without the reset the next run typed literal ASCII. The reset wipes
  the IME's own layout/mode prefs, so E2 is an AVD tier.
  Measured platform facts the harness encodes: (1) `input text` injects
  *characters*, never reaching `onKeyDown` -- the text appears whether or not an
  IME is installed, so it proves nothing; `input keyevent KEYCODE_*` is the key
  path. (2) The first key after the tap is warm-up: it lands before the IME
  window is bound, so the run spends one key and removes it with one backspace.
  (3) `uiautomator dump` writes the hierarchy as one line; the harness waits on
  byte size, not line count.
- **E2 Espresso instrumentation** ✅ (2026-10-13, `ctest engine-android-espresso`,
  `tools/android-espresso.sh`): the suite under `app/src/androidTest/`, run through
  `am instrument` on the AVD. It is registered because of issue #13: the suite
  existed, the board's N5 row read "present" — a fact about a *directory* — and no
  tier ever compiled it. `docs-lint` check 6 verifies paths, not builds, so the
  claim was unfalsifiable; `grep -rn connectedAndroidTest` found nothing outside
  the file that names the command, so the tier did not exist even as a command.
  **The harness drives the IME the way the platform does**, which is the part the
  original nine tests got wrong: `setUp` used
  `targetContext.startService(...) as HunminInputMethodService`, and an
  `InputMethodService` is *bound* with `BIND_INPUT_METHOD`, never startable — the
  cast has no path to succeeding, and a service started that way is not *the*
  IME. The suite instead enables and selects the IME from inside the test process
  with `UiAutomation.executeShellCommand()` (the same shell command
  `adb shell ime set` runs), focuses a real `EditText` in `TestActivity`, and
  sends keys with `Instrumentation.sendKeySync()` — the path a hardware keyboard
  takes, which `ViewActions.typeText()` is not.
  **Measured on the AVD, 2026-10-13: 12 of 12 tests execute and pass.**
  The tier found a real adapter defect on its first full run, which is the reason
  it exists: `secure_field_does_not_compose_Hangul` sent `p,a,s,s` into a
  password editor and the field read `퓬ㄴ` with `ime-target=true` — the keys
  reached the bound IME and the IME composed Hangul in an editor where the user
  cannot see it. `HunminInputMethodService.onStartInput` never inspected the
  `EditorInfo`, so a password field was treated exactly like a plain one. Fixed
  by an `isSecureEditor` guard set per-editor in `onStartInput` from
  `isSecureEditorInputType()`, which routes password-editor keys through
  `super.onKeyDown()` — the platform's own insertion path — instead of through
  either engine. Guarded at tier E1 by `SecureEditorTest`, so a regression is a
  JVM failure on every commit rather than only a device one.
  **Two constants were got wrong once and are now verified against android.jar
  (API 35) with `javap -p -v -constants`: `TYPE_MASK_VARIATION` is 0xff0 (4080),
  not 0xfff, and the two password variations are 0x80 and 0x90.** The 0xfff mask
  was caught by SecureEditorTest failing on "0x80 must survive the mask" — the
  guard as first written would have mis-read a password editor as plain in the
  field, i.e. the fix would have shipped broken.
  The runner distinguishes three outcomes and only the first is a failure:
  **0 tests executed → FAIL** (the suite ran nothing — the exact shape issue #13
  records, and the reason the script counts tests rather than trusting
  `am instrument`'s exit code); **tests executed, none failed → PASS**;
  **tests executed, some failed → PASS WITH FAILURES**, printed to stderr,
  because the assertions did their job and the failure is the product's.
  Two measured host facts are encoded in the runner. A physical device attached
  alongside the AVD is *ignored*: on a Samsung Galaxy Z Fold (API 37) every
  `Espresso.onView()` died in `setUp` with
  `NoSuchMethodException: android.hardware.input.InputManager.getInstance []`,
  the reflective resolution Espresso's event injector needs, so a "whatever is
  attached" default would install the APK on the maintainer's own phone and fail
  seven times. And the previous `default_input_method` is snapshotted and
  restored on exit, because `ime set` is a Secure-setting write that survives
  reboot.
  **Not covered, and not claimed anywhere:** the candidate-view, Hanja-F9 and
  `inputView` show/hide assertions from the original nine were never reachable
  and remain unwritten.
- **E4 voice check, with a person in the loop** 🧑🎙 (`tools/android-voice-check.sh`,
  `tests/manual-checklists/tier-e4-voice.md`): the voice path on real hardware, with
  the accuracy judgement left to a human and the *contract* left to the script.
  The split is forced by the platform, not chosen: the transcript comes from
  `SpeechRecognizer` and recorded audio cannot be replayed into it, so there is no
  deterministic accuracy assertion — a threshold would be a test that fails on a bad
  network day. What the script asserts is ours: the key is found, a tap reaches the
  voice path, the mic reports LISTENING, **the mic is painted while it listens**,
  the session returns to idle, the result reaches the editor, and nothing died.
  What the person answers is whether it was heard.
  The paint check is a screencap pixel, not a `uiautomator` dump, and the reason is
  measured: a dump of the IME window takes longer than the ~5 s listening session,
  so it reports the state *after* the session has ended. That timing artefact made
  a real dock defect look fixed for a day — `bind()` was repainting an `ImageView`
  never added to the view tree, so the state was right and the screen was not. No
  other tier can see that class: E1 asserts the state value, E2 samples a moment
  chosen by the harness.
  Not registered in ctest, like E3: a test that only passes when someone is at
  the keyboard is not a regression test. The script does not start, clear, or
  force-stop an editor: it waits for the person to focus a text field and then
  asserts the keyboard is up, so no app's saved state is touched.
- **E2 planned extensions**: GitHub Actions android emulator (x86_64,
  API ≥ 30, `-no-window`):
  `adb shell ime enable/set org.hunmin.ime/.HunminService` →
  inject physical-key events. ⚠️ the source is a **positional** argument that comes
  *before* the command — `adb shell input keyboard keyevent 29`, not
  `input -source keyboard keyevent 29`, which fails with "Unknown command"
  (measured on API 35, see ENVIRONMENT.md "Batch 2") → assertions inside a fixture `EditText` via Espresso:
  committed text (`getText`), **composing span** (`Spannable.getSpans(SPAN_COMPOSING)`) =
  preedit, candidate view via UiAutomator on the IME window, hanja flow via F9-chord.
  Corpus = same golden sequences → identical expectations as D3. Secure-field behavior and
  `inputView` show/hide policy get dedicated negative tests.
- **E3 physical BT-keyboard matrix** 🧑📦: **checklist written, NO SESSION RUN.**
  `tests/manual-checklists/tier-e3-bt-keyboard.md` — typing smoke, shift, CapsLock,
  voice, one-tap IME switch and back, focus change mid-composition. Expectations are
  `hunmin-sim` invocations, not remembered answers.
  ✅ **The trace capture it depends on works, and is not just claimed.** A tier-C
  trace sink in the **shipped** APK (`HunminTraceRecorder`) — a test-only sink would capture a
  different adapter than the one a person holds, the mistake FRONTEND-CONTRACT §3
  already records for fcitx5. It is inert unless a broadcast opens it, and only on a
  debuggable build, so a release IME cannot be made to write files. `tools/e3-trace.sh`
  drives open/close/pull over adb.
  **E2 now proves that round trip on a device**: it enables the sink, types, and
  asserts the trace holds meta plus an event per key — because a sink that silently
  records nothing would make every E3 session collect an empty file that reads as
  "no quirks found", which is the worst possible outcome for a manual tier.
  ⚠️ Enabling the sink is placement-sensitive, and the reason is worth keeping: the
  broadcast is handled by the IME service, so it can only be opened once the service
  is bound to the field being typed into. Enabling it before the warm-up keypress
  leaves the file present with its meta line and every subsequent keystroke
  unrecorded — measured, and it looked like a working sink.
  Non-automatable forever, and that's accepted: coverage comes from E2 + trace capture.
  **PLAN.md's P5 gate stays open**: no device is signed off.
- **E4 monkey/soak** (nightly, cheap): random HW-key streams into the emulator IME +
  LeakSan/JNI reference checks; crash reports auto-filed.

## Tier F — Windows TSF (P6) 🤖~ / 🧑

- **F1 Tier-C conformance** on the TIP (trace mode via a harness app) — required.
  The harness is `frontends/tsf/test/tsf-trace-emit` + the mock TSF layer
  (`frontends/tsf/test/mock-tsf.h`, issue #80): in-memory COM doubles, no registry
  and no real windows, with the adapter source included unmodified. ctest label
  `tier-F1`, corpus `tests/conformance/win32/*.trace.in` (same generator, win32
  dialect), registered only where MSVC found the engine. Milestone C (issue #66)
  added `tsf-config-test` (the layout-pair store and slot resolution),
  `tsf-datadir-test` (staged `<module>\data` loads with the env cleared) and a
  dubeolsik win32 trace — until then every win32 trace selected the hard-coded
  sin3-p2 and no test could see a layout-wiring regression. The harness also pins
  both slots from the trace's own meta, so a host's real TIP settings cannot move
  a conformance run. Registration caveat, measured 2026-10-17: the tier lives in
  a standalone `frontends/tsf` configure, which needs its own `enable_testing()`
  (now present — before it, ctest reported "No tests were found" while the
  harness built) and `-DPython3_EXECUTABLE` passed explicitly works only with
  the executable-keyed guard.
  Issue #107 extended the harness from the key/buffering path to the TSF
  surfaces the adapter had only ever exercised by hand:
  `tsf-registration-test.cpp` drives `DllRegisterServer`/`DllUnregisterServer`
  over the registration seam (`frontends/tsf/src/registrationseam.h`, whose
  DEFAULT table is the platform call itself) and asserts the
  register-to-unregister mirror, the parameters (CLSID, 0x0412, the profile
  GUID, the category GUIDs) and both activation branches;
  `tsf-session-test.cpp` covers the conversion-compartment writes, the
  key-event-sink advise/unadvise pairing, and the language-bar
  AddItem/RemoveItem path including the measured `0x80004005` refusal.
  Neither asserts what the real msctf servers do to the registry: that stays
  the elevated cycle in `tests/manual-checklists/tsf-unregister-cleanup.md`,
  and a green run here does not close it.

  Milestone F (issue #69) widened the win32 corpus from 3 traces to the
  families the x11 corpus replays: `p2-2-1-basic` and its `sin3-p2` alias run
  (`dkssud-combo`), `p2-2-3-chosung`, `p2-2-4-1-yetrules`, `p2-2-4-2-eung`,
  `p2-3-swapped`, `manual-sin3-p2` and `manual-sin3-p2-yet`. All eight are
  generated from the same tier-A goldens with
  `tools/gen-conformance.py --platform win32 --batch` and golded with
  `tools/trace-check.py --emit`. Measured 2026-10-31: 11 traces / 106 cases /
  688 events over four layouts (dubeolsik, sin3-p2, sin3-p2-basic,
  sin3-p2-yet), 12/12 green on `ctest --test-dir build-msvc -C Release -L
  tier-F1`. Each emitted `.gold` was diffed against the `.in` expectations the
  generator copies verbatim from tier A — identical on all 688 events, so
  nothing was accepted blind.

  One case is **excluded, not replayed**: `manual-sin3-p2-yet`'s caps-on `k/`.
  Win32 has no caps encoding (docs/trace-spec.md "`in` — the native event"), and
  emitting it at `mods: 0` asserts caps-off keys against a caps-on expectation —
  measured: the adapter composes 고 where tier A records commit ㄱ + preedit ㅋ.
  `tools/gen-conformance.py` therefore refuses a caps case for win32 and writes
  the refusal into the trace's `meta.excluded`, so
  `tests/conformance/win32/manual-sin3-p2-yet.trace.in` covers 14 of the
  golden's 15 cases and says so in the artefact.

  Not yet covered: caps-on key sequences on win32 (excluded, above), a backspace
  boundary (no sin3-p2 tier-A golden carries one) and overlay-window
  presentation (needs real HWNDs, stubbed via `ITfContextView::GetTextExt`).
- **F2 TAT** (Microsoft TSF Accommodation Tool, the official TIP stress-tester) driven on
  a Windows runner; plus UIAutomation-based text assertions in Notepad/test edit control
  with injected keys (`SendKeys`/`keybd_event`) — best-effort automation, flaky-allowed,
  results logged.
- **F3 x86+x64 dual-arch install/activate matrix** 🧑 scripted checklist.

## Tier G — Release gates & process

| Gate | Requirement |
|---|---|
| Any PR | Tier A+B+C green (host CI, minutes) |
| Frontend PR | + that platform's Tier D/E/F automated subset |
| Desktop release | D3 e2e suite green on the Ubuntu LTS + current dev target |
| Android release | E2 emulator suite green; E3 checklist signed off for listed devices; signed release build |
| Data-only PR (new layout, external contributor) | A + B(`hunmin-doctor`) + C green; docs/SPEC diff review |
| Weekly | fuzz 8h + differential-vs-libhangul 1h + E4 soak |

- `tools/regen-goldens.sh` regenerates goldens — output always human-reviewed; trace corpora are
  append-only.
- Known untestable-by-design corners (documented, not hidden): ASR results (voice),
  OEM IMMS deviations (One UI), fcitx5 Wayland module internals, SmartScreen reputation.
- Tooling inventory to validate at P0: clang libFuzzer, `python3-evdev`+uinput, Xvfb,
  Espresso/UiAutomator + emulator-runner action, TAT + pywinauto, `nm`, ctest.
