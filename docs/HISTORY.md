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
