---
title: "Milestone F: the win32 corpus replays the eight x11 tier-A families (issue #69)"
created: 2026-10-31
updated: 2026-10-31
type: session
status: current
tags: [session, windows, tsf, conformance, tier-c, tier-f1, corpus, issue-69, issue-184, issue-185, issue-186, issue-187]
confidence: high
---

# Milestone F: the win32 corpus replays the eight x11 tier-A families (issue #69)

One line: the Windows conformance corpus grew from 3 traces (bulk prose, words,
dubeolsik seosi) to 11 — the eight tier-A families the x11 corpus replays, batched
per family — and the work exposed a generator that emitted a caps-on case at
`mods: 0` and asserted caps-off text.

## What the gap was

`docs/FRONTEND-EQUIVALENCE.md` §6 makes the shared corpus the cross-frontend
authority, and Windows replayed none of the tier-A families fcitx5 replays: its
three traces were one long dubeolsik text and two sin3-p2 word lists. The win32
dialect, the tier-F1 registration glob and `tools/trace-check.py` all existed, so
this was generation plus reviewed golds rather than new tooling.

## What shipped

`p2-2-1-basic`, `dkssud-combo` (the sin3-p2 alias run of the same goldens),
`p2-2-3-chosung`, `p2-2-4-1-yetrules`, `p2-2-4-2-eung`, `p2-3-swapped`,
`manual-sin3-p2`, `manual-sin3-p2-yet` — generated with
`tools/gen-conformance.py --platform win32 --batch`, golded with
`tools/trace-check.py --emit`.

Measured: 11 traces / 106 cases / 688 events over four layouts (dubeolsik,
sin3-p2, sin3-p2-basic, sin3-p2-yet); tier-F1 12/12 green on `build-msvc`
Release. Every emitted `.gold` was diffed against the `.in` expectations the
generator copies verbatim from tier A — identical on all 688 events, so nothing
was accepted blind. That diff is the review: `--emit` writes whatever the runner
produced, so "wrote the gold" proves only that it ran.

The new traces also carry 13 case instances (11 labels) the x11 corpus does not
replay at all. That is not a gain to celebrate: it is the same drift in the other
direction, filed as issue #187.

## The defect the work found

`tools/gen-conformance.py` refused a caps case for win32 and then un-refused it —
`unsupported -= {"caps"}` ran unconditionally — so `manual-sin3-p2-yet`'s caps-on
`k/` was emitted at `mods: 0` with the caps-**on** expectation. Replayed, it fails
at its second key: the adapter composes 고 where tier A records commit ㄱ +
preedit ㅋ. Win32 has no caps encoding at all (`docs/trace-spec.md`, "`in` — the
native event": the TIP models `VK_CAPITAL` as a Hangul/English toggle), so the
honest fix is a per-case exclusion written into the trace's `meta.excluded` —
provenance in the artefact, not only in a terminal. The trace covers 14 of the
golden's 15 cases and says so.

This is the pattern [[the-rules-keep-breaking]] names: a rule that a tool was
written to enforce, silently washed by one line in the same function. It was
found because a generated case was replayed, not because the generator was read.

## What stayed out, and where it went

Nothing was absorbed silently. Four issues carry what this round did not measure:

- #184 — caps-on sequences have no win32 encoding; excluded, needs a ruling on
  whether the trace dialect gains a caps channel.
- #185 — both TSF delivery modes (`docs/FRONTEND-EQUIVALENCE.md` §2/§6): the mock
  has no `EM_*` edit-control channel, so interim mode is unmeasured over it.
- #186 — 11 of 15 shipped layouts appear in no conformance trace on any platform.
- #187 — x11 omits 13 case instances that win32 replays from the same goldens.

The commit-unit axis was already out (`docs/FRONTEND-EQUIVALENCE.md` §6.1, ruled
2026-10-25), and `docs/REMAINING-WORK.md` carries the rows for all of the above.

## Links

- [[2026-10-26-tsf-mock-unmodelled-surfaces]] — the mock harness this corpus is
  replayed through, and the session that showed its own blind spots.
- [[the-rules-keep-breaking]] — the failure class the generator defect belongs to.
