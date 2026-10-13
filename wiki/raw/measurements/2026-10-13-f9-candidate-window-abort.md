---
title: "F9 with 11 or more candidates aborts fcitx5 (CommonCandidateList: invalid label idx)"
created: 2026-10-13
updated: 2026-10-13
type: measurement
status: active
tags: [measurement, fcitx5, e2e, testing, hanja]
ingested: 2026-10-13
confidence: high
---

# F9 with 11 or more candidates aborts fcitx5

Issue [#45](https://github.com/scobyseo/hunmin/issues/45). The symptom is real
and it is not local: fcitx5 dies with `SIGABRT` about 70 ms after F9, on any
syllable whose candidate list has **11 or more** entries on one page. The
mechanism is a page size, not a candidate count and not the vertical layout
hint. Everything below is measured on fcitx5 5.1.19 as installed on this host;
the commands are in `tools/repro-f9-candidates.sh`, so the figures are
reproducible rather than quoted from this session.

## The abort

```
D ... xim.cpp:647] XIM Key Event: 2 F9 time:43747023 detail: 75 state: 0 sequence:96
F ... event_sdevent.cpp:275] CommonCandidateList: invalid label idx
=========================
Fcitx 5.1.19 -- Get Signal No.: 6
```

`Get Signal No.: 6` is `SIGABRT`, exit status 134 through the shell. It is an
abort, not a segfault, and there is no core dump line in the handler's own
backtrace because fcitx5's `CommonCandidateList::label()` throws
`std::invalid_argument` and nothing catches it.

## Where the throw comes from

fcitx5-5.1.19 `src/lib/fcitx/candidatelist.cpp`:

- `regularLabelSize = 10` (line 28).
- `fillLabels()` (lines 31–41) pads the label vector to `regularLabelSize`
  and no further, so a list that never calls `setLabels()` has exactly 10
  labels.
- `CommonCandidateList::label(int idx)` (lines 556–565) throws when
  `idx >= labels_.size()`.

classicui walks the whole page in `src/ui/classic/inputwindow.cpp:392-429`:

```cpp
for (int i = 0, e = candidateList->size(); i < e; i++) {
    const auto &candidate = candidateList->candidate(i);
    ...
    Text labelText = candidate.hasCustomLabel()
                         ? candidate.customLabel()
                         : candidateList->label(i);
```

`size()` is the **page** size (`candidatelist.cpp:357-364`), so with hunmin's
page size of 67 the loop asks for labels 0..66 and label 10 throws.

## The threshold, measured

Isolated from classicui, replaying only its loop over
`CommonCandidateList` (`size()`, `candidate(i)`, `label(i)` for
`i in [0, size())`):

| page size | Vertical hint | Horizontal hint |
|---|---|---|
| 1, 5, 9, 10 | no throw | no throw |
| 11, 12, 19, 67, 73 | throws at `i=10` | throws at `i=10` |

Then end to end through the real stack, with the candidate count read out of
the shipped data:

| syllable | keys (sin3-p2) | candidates | result |
|---|---|---|---|
| 녹 U+B179 | `hvc` | 10 | survives, input window 336x334 |
| 낙 U+B099 | `hfc` | 11 | `SIGABRT` |
| 간 U+AC04 | `kfs` | 67 | `SIGABRT` |

The window geometry is the oracle that the window opened at all: 녹's page
renders as a 336x334 column, against 27x34 for the one-character preedit with
no window. Reading "no crash" without that check would have measured nothing,
which is the failure mode recorded in [[2026-10-10-d3-case-coverage]].

## What the issue's hypothesis was, and what measured

The issue named two suspects and marked them as unestablished. Both were
measured, and neither is the cause.

**The vertical layout hint is a bystander.** classicui calls `label(i)` for
every candidate on the page *before* it looks at `layoutHint_`
(`inputwindow.cpp:431` is where the hint is read, after the loop). The table
above shows Horizontal throwing at exactly the same index.

**The page size is the trigger, not the candidate count.** A list of 67
candidates with the default page size 5 was built and walked: no throw,
`totalSize=67 pageSize=5 size()=5`. What crashes is
`setPageSize(n)` with `n > 10`, which is
`frontends/fcitx5/src/engine.cpp:509`. The comment above that line states the
intent — "The page size is the candidate count, not the default 5, so the whole
list is drawn at once" — and nothing in the same function supplies labels to
match.

**`hunmin_suggest_text(i)` is bounded, and does not re-traverse.** The issue
read `suggest.c:322-328` as a rebuild that happens on every call. Lines 322-328
are `hunmin_suggest_count`'s own prologue (`list_reset()` is line 323);
`hunmin_suggest_text` (lines 358-361) is a pure bounded read that returns `""`
for `i >= g_list.n` and for `i < 0`. Measured, against both the repo data and
`/usr/share/hunmin`:

```
A[67] (==count) = ''      A[77] = ''      A[-1] = ''
```

So the loop in `updateCandidateList()` cannot read past what was collected. It
is not the crash.

## A separate defect on the same line of code

The process-wide candidate list *is* shared, and a second
`hunmin_suggest_count()` from another context does clobber the first one's
indices. Two contexts, repo data and installed data alike:

```
A count=67 ; A[0] = '刊'
B count=1  (푸)
A[0] = '䬌'          <- A's first candidate is now B's
```

Bounded reads mean this is wrong text, never an out-of-bounds access. It is a
different defect from #45 and it is recorded here rather than in the issue
because it was measured here. Two input contexts with an open F9 window is
what it takes; one context alone never hits it.

## Not local

`HUNMIN_F9_SYSADDON=1 tools/repro-f9-candidates.sh hfc` runs the **installed**
addon (`/usr/lib/x86_64-linux-gnu/fcitx5/libhunmin.so`, Debian `hunmin` 0.1.0,
dated 2026-10-01) against the installed data (`/usr/share/hunmin`). Same abort,
same line. The data trees are also byte-identical in size and date, and the
candidate counts match, so a data-directory mismatch is not the local
difference either.

## Why no tier caught it

Both tiers that cover this path are structurally unable to see it, and each is
green:

- `engine-d2-ui` drives `HunminState` through fcitx5's testfrontend and asserts
  the InputPanel. It does open the F9 window on 간 — 67 candidates, page size 67,
  asserted at `frontends/fcitx5/test/testhunminui.cpp:391-401`. testfrontend
  loads no UI addon, so nothing ever calls `label()`.
- `engine-d3-*` runs the whole real stack, but `tests/e2e/run-d3.sh:391` lists
  `DisabledAddons=classicui`. No D3 run has ever drawn the window.

This is the same shape as the issue's own complaint that the tier "runs and
measures nothing about the case that matters", one tier further out: D2 has the
case and not the UI, D3 has the UI switched off.

One correction to the issue as written: the multi-candidate F9 case it asks for
exists. It was added on 2026-10-11 (`testhunminui.cpp:364-439`, "Keyboard
selection in the open candidate window (2026-10-11)") and covers 간, Down,
Return, Escape. It is a good tier-D2 test and it cannot fail on this.

## The ruling, and what it did

**Ruled by the maintainer, 2026-10-13: cap the page size at 10**, rather than
keep the whole list on one page and supply labels with `setLabels()`.

`frontends/fcitx5/src/engine.cpp` now carries `kMaxCandidatePageSize = 10`,
named rather than a bare literal so a future fcitx5 that moves the ceiling has
one place to look and so the number keeps its provenance — it is
`regularLabelSize`, not a preference. Verified after the change, through the
real stack with classicui enabled:

| syllable | candidates | result | input window |
|---|---|---|---|
| 푸 | 1 | survives | 198x64 |
| 녹 | 10 | survives | 336x334 |
| 낙 | 11 | **survives** | 300x334 |
| 간 | 67 | **survives** | 706x334 |

**What the cap cost: nothing that was reachable.** Measured end to end, not
inferred, because the pre-fix process could not be asked:

- Return straight after F9 commits 刊, candidate 0 (cursor starts there).
- Down once commits 肝, candidate 1.
- Page_Down once puts the cursor on page 2's first candidate — 肝 again, which
  is index **10**, not index 1; Page_Down twice commits 竿, index **20**. The
  two are distinguished only by dumping the list with indices, which is the
  reason the table above carries `dumpcands` rather than the characters.
- Up from the first candidate wraps to the last, 諫 at index 66 of 67.
- Escape closes the window.

So paging reaches everything the one-page layout reached, and the old
comment's claim that cursor movement rotates over `totalSize()` holds with the
cap in place.

**What it did not close.** No ctest tier draws the F9 window, so the abort
still cannot regress through `ctest` alone; `tools/repro-f9-candidates.sh` covers
it and nothing runs it in CI. `engine-d2-ui` now asserts
`pageSize == min(totalSize, 10)`, which is the tree fact — measured red with
the cap reverted ("page size is 67, above fcitx5's label ceiling of 10") and
green with it — but that tier loads no UI addon, so it goes green even with the
bug present. The comment in the test says so, because a green tier that looks
like coverage is exactly what this page is about.

## Two things that cost time here, recorded so they are not repeated

**`XK_F9` is 0xFFC6. 0xFFC2 is `XK_F5`.** An early run of the repro injected
0xFFC2; the addon correctly declined an unmapped keysym and flushed the
composition, and the client transcript read `COMMIT:간` — indistinguishable
from "F9 committed the syllable" unless you know what to look for. What settled
it was fcitx5's own `xim_key` debug category printing `XIM Key Event: 2 F5`.

**`FCITX_LOG_RULE` does nothing.** There is no such `getenv` anywhere in
fcitx5 5.1.19; the log rule is the `--verbose` option, and `-d` means
daemonise. `tests/e2e/run-d3.sh:431` exports `FCITX_LOG_RULE` and it has never
had any effect — that script's logs come out at `default=5` regardless, which is
why `hunmin=5` and `xim=5` in it were decorative.

## Where the fix is not decided here

The measurements above locate the defect to one line and separate the three
candidate causes. The page size was then **ruled on by the maintainer**
(cap at 10) and the change is in the tree; see
[[decisions-belong-to-the-maintainer]] for why that split is the rule.

Related: [[2026-10-10-d3-case-coverage]] on a tier reporting green for a case
it did not run, and [[2026-10-10-mutation-tiers-measuring-nothing]] on the same
failure one level down.