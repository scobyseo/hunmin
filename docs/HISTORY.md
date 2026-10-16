# HISTORY

What shipped, and what we learned while shipping it. This file exists because
the git history it would otherwise draw on is being squashed into one commit,
and because a change history that is not written down gets re-derived wrongly.

## What belongs here

**A phase or a day's work, not a commit.** One entry per thing that changed the
project's shape, written as: what shipped, and the measurement or the defect
that makes it worth remembering. Detail belongs in the document named in the
entry, not here.

Three things deliberately do not go in:

- **Commit hashes.** They would not survive a squash, and a hash that does not
  resolve is worse than prose because a reader will try to look it up.
- **Every commit.** About one in six commits is a real change; the rest are
  fixes to the change before it, and listing them says nothing the entry above
  does not.
- **Anything that reads as a boast.** If the entry cannot say what was measured
  or what was wrong, it does not go in.

**Keeping it true is a per-phase job, not a one-off.** When a phase lands, add
what shipped. A history that stops growing is indistinguishable from one that
never got written.

## 2026-09-29 → 09-30 — the engine reaches parity

**Shipped.** Symbol tiers lifted from pat.im's `symbols.xml`; 옛한글 conversion
from `yethangul.xml` with converters that fail fast rather than emit partial
data; CapsLock wired through to the goldens; romaja ported verbatim from
libhangul and byte-exact against it.

**Learned.** The disambiguation model was settled by comparison against kime and
pat.im together, and the answer is written down rather than assumed — variant
absorption via J/jvar, and a pair-aware G-gate. See `docs/SPEC-P2.md`.

## 2026-10-01 — a layout becomes data, provably

**Shipped.** The P2 gate: nine golden corpora, one per pat.im/1136 feature
section. The first fcitx5 addon with an in-process test (tier D1). Debian
packaging.

**Why the gate matters.** Without it "the layout is data" was a claim about the
code's shape. Nine corpora make it a claim about behaviour, and a layout that
regresses now has a named failure.

## 2026-10-02 → 10-03 — the first tier that could see the whole stack

**Shipped.** Tier D3: a real distro fcitx5 under Xvfb, a real XIM client, keys
injected through XTEST. The Android IME, which types.

**What it cost.** D3 had to be made to terminate under ctest before it could be
trusted at all — a hang is a test that measures nothing. And the layout picker
turned out to need an fcitx5 restart, which is a real cost rather than a detail.

## 2026-10-04 → 10-05 — 한영 automatic detection, and its ceiling

**Shipped.** The MODES requirement set R1–R13 and a measured ceiling on the
detection data. `tools/modes-cap.py` reports what each candidate source covers.

**The finding that shaped everything after.** The English side saturates — a
50,000-token cap recovers 86.35%, so the cap is justified. The Korean side does
**not**: 200,000 entries yields 2.90% of the forms, and cutting to that size
drops `hangul_valid` from 73% to false, which under R8's "when ambiguous, keep
the current mode" makes the Korean mode vanish quietly. Dictionary lookup is
also not the answer — filtering by an English dictionary takes recovery from
86.35% to 43.87%, and `kbs`, `fbc`, `fc`, `sbs` are absent from it.

**The boundary.** `krdict` was moved out of scope for this research: it exists
only for hanja conversion. Every measurement that used it is marked as outside
the boundary and was re-derived inside it.

**Also.** The R8 adapter shipped with a real bug that verification caught, and
a drift audit was run because documents had begun claiming things the tree
disagreed with.

## 2026-10-07 — the Android path stops being one-way

**Shipped.** Release signing configuration, and the tier-C Android corpus: 14
`.in`/`.gold` pairs with a runner, so the Android adapter is checked against the
same expectations as every other frontend.

## 2026-10-09 → 10-10 — three tiers that were green and measured nothing

This is the part of the history worth most, and none of it was found by reading
code.

**Tier D3 ran 1 of its 8 cases and reported green.** `run-d3.sh` redirects the
script's stdin from the case file, and the XIM harness had that same descriptor
in its poll set and drained it — so the harness and the shell's read loop
competed for one file, and whichever won consumed the file. Verified by count:
eight cases in the corpus, one `ok:` line in the log, 100% passed in 5 seconds.
With all eight running, the harness reuse turned out to leak state between
cases, and the tier went back to one harness per case. The "regression" from 5
seconds to 46 was the truth arriving. See
`wiki/raw/measurements/2026-10-10-d3-case-coverage.md`.

**This was the second time D3 looked green while measuring nothing.** The first
is `docs/D3-ROOT-CAUSE.md`: the harness used libX11's built-in XIM against
fcitx5's server, so it decoded every key itself and compared its own output
against tier A.

**Both mutation self-tests reported catches they had not made.** The Android one
read its runner's skip exit as "caught" — nine of nine, exit 0, in 0.31 s, with
nothing built. The fcitx5 one held the whole suite hostage because it rewrote
the working tree. Both now plant their mutations in a sandbox and change
nothing outside it. See `docs/MUTATION-TIER-OPTIONS.md`.

**And a status board went stale.** `docs/REMAINING-WORK.md` §7 is the record:
eleven of its open items had already been closed by commits, and a mechanical
guard now parses the document's own claims and verifies each against the tree.

**Cost of the suite itself.** 185 s serial → 54 s at `-j16`, with the individual
fixes recorded in `docs/D3-PERFORMANCE.md` and the label vocabulary in
`docs/TESTING.md`. The largest single win was not the tests but the fact that
fourteen of them could not safely run at the same time.

## 2026-10-10 — preparing to publish

The licence file that was missing, the `debian/copyright` stanzas that claimed
the maintainer's copyright over a ShareAlike derivative of a government
dictionary, and the contributor file that was 462 lines because it was built by
pasting private sessions into it.

**What is still open is in `docs/REMAINING-WORK.md`**, including the items that
are blocked on the maintainer rather than on code.

## 2026-10-14 — CI wall time, measured rather than argued

Issue #59 asked what the 8 m 24 s linux job costs and where. The answer is now
a measurement record, `docs/CI-TIMING.md`, with the per-job segment tables
parsed from the run's own job logs rather than re-estimated.

**Measured.** Three tests are 252 of the linux job's 269 test-seconds and 386
of the deb job's 402 — `engine-android-e1`, `android-mutations` and
`frontend-mutations` in all three jobs that run them. The deb job is
13 m 30 s, longer than the linux job the issue names, and it builds fcitx5
5.1.13 from source a second time (326 s) with nothing shared. The Android tiers
land in the linux job at all because the runner image ships an Android SDK the
workflow never installs — `ANDROID_HOME=/usr/local/lib/android/sdk` and the
exact NDK this repo pins are in the image, so the `CMakeLists.txt` guard fires
on every run and the android job re-runs the same two tests at `ci.yml:166`.
Cold Gradle is most of tier E1: 126 s against a 23 s warm, measured by
emptying `GRADLE_USER_HOME`, and there is no `actions/cache` in the workflow.
`ctest -j` is real and cheap — 190 → 48 s on the host — but cannot reach the
1–2 minute target alone, because `engine-android-e1` is `RUN_SERIAL` and
`android-mutations` is nine sequential Gradle runs.

**Not done, deliberately.** No lever was applied. The issue says "the choice
between them is not made here" and AGENTS.md reserves the ruling for the
maintainer; `docs/CI-TIMING.md` §"Options" compares all five with their
coverage cost and what evidence would change the order. One duplication is
recorded as an open tree fact with a `REMAINING-WORK.md` assertion row rather
than deleted: `ctest -N` registers 16 Android conformance tests against 15
corpus files, and removing the stale block at `CMakeLists.txt:592-603` is a
maintainer call, not a cleanup.

**Cost of one existing decision, now costed.** "Labels are not a gate" —
`.github/workflows/ci.yml` runs the unfiltered suite — is what makes 386 of
the deb job's 402 test-seconds the price of every .deb build. The property
stands, and `docs/TESTING.md` and `CMakeLists.txt` now both say what it costs
rather than leaving it unexamined.

**And the levers were applied, in the order the measurement named.** The
`docs/CI-TIMING.md` record ended at "the choice is the maintainer's"; the
follow-up commit implements four of its five levers and the fifth for the deb
job only. `actions/cache` on `~/.gradle` in linux, android and deb (the one
lever with no coverage trade — E1 is 126 s cold against 23 s warm, measured by
emptying `GRADLE_USER_HOME`); `ctest -j4` in linux, deb and the android job's
E0/E1 step, `-j4` rather than `-j16` because the runner has 4 cores and this
repo has been bitten by oversubscription before; the fcitx5 5.1.13 install tree
uploaded by linux and restored by deb, which makes `deb` declare
`needs: linux` and turns 523 s of duplicated source build into one build plus
one restore; and the duplicate conformance registration removed, so `ctest -N`
reports 15 against the 15 corpus files instead of 16 against 15.

**The one lever NOT applied to linux, and why.** `ctest -LE 'tier-E1'` in the
linux job would take it to ~130 s and past the 1–2 minute target. It is applied
to `debian/rules` only, because the .deb does not ship the Android adapter. It
is not applied to the linux job because the android job — the only other place
tier E1 runs — is the one that failed at the SDK step in the run measured, and
excluding E1 from linux in that state makes `linux` green while the only job
that measures E1 is broken. That is a maintainer call about what `linux`
asserts, and it is recorded in `docs/CI-TIMING.md` §"Options" rather than
settled here.

## 2026-10-16 — 0.1.0, and the tier that was asserting nothing

**Shipped.** The release checklist issue #10 asked for (`tests/manual-checklists/release-0.1.md`),
the release gate as a script rather than a sentence (`tools/release-gate.sh`, from
the maintainer's ruling that "on release, all TCs in this repo should pass, except
data-selfcheck which cannot pass"), and the publish path as a script
(`tools/release-0.1.sh`, issue #7). The `.deb` builds again — `tools/build-release.sh
deb` exits 0 with the suite green inside the package build — and the release APK
builds, signs, installs and composes `ㅎ곈디` on an emulator.

**The defect worth remembering is that tier E2 had stopped asserting.** It reported
`100% tests passed` over zero assertions, for two independent reasons in
`tools/android-e2e.sh`: `dump_ui` was defined ~420 lines *below* its first call
site, and its validity check was `wc -l > 5` on a hierarchy that `uiautomator
dump` writes as **one line**. Every tap by resource id and every screen read failed,
each failure ended in `skip`, and a skip is exit 77 — which ctest counts as
not-failed. The same class had already cost a 2026-10-10 afternoon when
`engine-d3-x11` reported green having run one of its eight cases. **A tier whose
failure mode is `skip` cannot report its own blindness**, which is why the release
gate now fails on an undeclared skip and on a declared exception that stopped
skipping, rather than reading `100% tests passed` as done.

**The second blocker was one renamed accessor.** `1521fad` renamed
`config().layout` to `config().hangulLayout` and updated the invariant checker in
the same commit but not the `I5-layout-override-ignored` anchor in
`tools/frontend-mutations.py`. The mutation could no longer be applied, the
mutation self-test failed closed — and because `debian/rules` runs the whole suite
inside `dpkg-buildpackage`, **no `.deb` could be built at all**. The self-test
caught it correctly and far too late. `docs-lint` check 7 now verifies every
mutation anchor against its source in milliseconds, and `shellcheck -S error`
covers the shell, whose first catch was a script with no shebang.

**What is still open is in `docs/REMAINING-WORK.md`.** Two things are named there
rather than left to be discovered: `engine-android-e2e` remains intermittent
(passes at 108 s with full assertions, and fails with the field receiving literal
ASCII while `dumpsys` reports the IME current, connected, bound and shown — five
hypotheses refuted in `wiki/raw/measurements/2026-10-16-android-e2e-keys-bypass-the-ime.md`),
and **no CI job has executed since 2026-10-13** — an account/billing limit, not a
defect in this tree — so the release gate for 0.1 was `tools/release-gate.sh`, run
locally.
