---
title: Tier D3 ran one case in eight and reported Passed
created: 2026-10-10
updated: 2026-10-10
type: measurement
status: active
tags: [measurement, e2e, testing, postmortem]
ingested: 2026-10-10
confidence: high
---

# Tier D3 ran one case in eight and reported Passed

Found while implementing a different change (reuse the GTK harness per layout
group, on branch `task/D3-gtk-reuse`). It is recorded here because it is the
second time this tier has been green while measuring something other than it
claims — the first is in `docs/D3-ROOT-CAUSE.md` — and because the two have the
same shape: **a harness or runner detail that is silently wrong, in a tier whose
failure mode is a diff of two text streams.**

The evidence is this session's own runs against `tests/e2e/run-d3.sh` and
`tests/e2e/xim-harness.c`; the commands are reproduced below rather than cited,
because they were run once against a working tree and are not a reusable
artefact. The figures live in `docs/TESTING.md` §D3 and
`docs/D3-PERFORMANCE.md`; this page records how they were found, so the
evidence survives the fix. `docs/HISTORY.md` carries the entry.

## The symptom

`ctest -R engine-d3-x11` printed:

```
1/1 Test #35: engine-d3-x11 ....................   Passed    5.01 sec
```

and the run printed exactly one `ok:` line, for `dubeolsik/dkssuds`. The case
file holds eight.

## How it was pinned down

The obvious first guess — "the harness died" — was wrong, and the wrong guess
is the useful part:

* `HUNMIN_D3_KEEP=1` left the stage directory; the harness log was clean and
  the process had exited normally at teardown.
* The GTK tier ran **all eight** cases at the same revision, with the same
  `cases.tsv`. So the case file was readable and the loop was fine; something
  ate it only on the XIM path.
* An instrumented copy of the runner that, after the loop ended, tried to read
  one more line from fd 0 got `rc=1` (EOF) while `stat` on the *file* still
  reported 2849 bytes. **The runner's file offset was at the end.** Not
  truncated — consumed.

## The cause

Two facts that were each individually defensible:

* `run-d3.sh` fed its case list to the main loop on stdin: `done <"$CASES"`.
* `xim-harness.c`, after an earlier change added a `STDIN_FILENO` arm to its
  poll loop, drained it. The body of that arm said, in the source, "No case-id
  traffic is expected on stdin in this design (the id travels via
  `HUNMIN_CASE_ID_FILE`)" — and drained it anyway.

The harness is started by the loop, so it inherited the descriptor. 256 bytes
per poll iteration, until the file was at EOF and `while read` stopped.

The diagnosis was confirmed by trying the opposite fix: launching the harness
with `</dev/null`. That made **all eight** cases run, and every one of them
failed "harness never became ready" — because the arm's own guard is
`if (got <= 0 && errno != EAGAIN && errno != EINTR) break;`, and a read from
`/dev/null` returns 0. The arm was consuming a stream it did not own in one
direction and dying on it in the other.

## Why it was invisible for so long

The one case that ran is the first of the first layout group, and that is
exactly the case which cannot inherit anything from a previous one. So the
defect simultaneously hid the second one: with all eight running, **XIM harness
reuse is unsound** — three cases fail, because an XIC carries the server-side
composition and four of the eight cases end with a preedit in flight. A tier
that only ever ran case one could never have found it.

## What I got wrong, recorded so it is not repeated

* I read the brief's "per-case startup is the majority of the 50 seconds" as
  established and went looking for the harness reuse without measuring the
  runner first. The premise was false — harness startup is 8% of the GTK run
  (`docs/D3-PERFORMANCE.md`). Measuring the breakdown first would have cost two
  minutes and would have told me that the deliverable's target was unreachable
  by the requested means, which is something the maintainer should have been
  told at the start rather than at the end.
* I then found the case-count defect through an unrelated diff, on the way to
  being asked "did you regress X11?". That is luck, not method. **A tier that
  prints one `ok:` line per case in its output should have its case count
  asserted**, the same way the Android tiers assert their test counts rather
  than trusting registration. That check does not exist and this page is the
  argument for it; it is not implemented here because it is a change to the
  runner's contract, not to this task.

## Related

- [[the-rules-keep-breaking]] — this is one of five instances of the project's
  most repeated failure, collected there with the two defences.
- [[2026-10-09-gtk-forward-path]] — the three ways the GTK harness was green
  while measuring something else, including one that is still load-bearing.
- [[2026-10-09-where-space-is-emitted]] — the tier that should have caught the
  symptom could not see the key at all. Same genre, different key.