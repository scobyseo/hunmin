# TSF interim-input spike — the legacy input path for classic (default-sink) hosts

> Measurements only / no option is the shipped design / the choice is the
> maintainer's (issue #119; the options remain open below).
>
> Produced on 2026-10-20, branch
> `scobyseo/windows-legacy-input-path-for-classic-default-si`, base
> `0e0b84c`, host: the Windows 11 dev machine. The spike work lives ONLY in
> `frontends/tsf/build-msvc/spike/` and an **uncommitted** edit of
> `frontends/tsf/src/tsf.cpp`; the SPIKE CODE IS NOT PART OF ANY SHIPPED
> DIFF — this document is, and the tree was restored to base before the
> verification row below was measured. `docs/REMAINING-WORK.md` untouched
> (parallel sessions may patch it; board rows follow the merge).

## 1. The premise, checked first

The ruling implemented (issue #119 body, verbatim): *"file issue to
implement legacy support."* The session's shape pick (clarify, this session,
2026-10-20): *"TSF interim-input spike first — measure whether
interim-char/selection display behaves differently on EDIT/RICHEDIT50W
before committing to the overlay restore."*

Checked against the tree, numbered:

1. *The #98 native composition path is the only path; the overlay window
   preedit code is gone.* **Verified** — `git grep` for `overlay|UpdateOverlay`
   in `frontends/tsf/src/tsf.cpp` returns nothing; the pre-#98 shape is
   intact in git history at commit `e2ad652` (`UpdateOverlay`, old lines
   1419–1520; the direct-insert `FlushSession`, 1667–1672).
2. *Classic hosts terminate the composition after every key*, and the
   termination handler drains the buffers and resets the engine
   (`tsf.cpp:1575`). **Verified and now reproduced on the dev host** (no
   sandbox needed — see the baseline row below): per key,
   `OnTestKeyDown w=… eat=1` → `OnKeyDown` → `composition start hr=0x0` →
   `OnCompositionTerminated … wordbuf=0`, 6 start lines + 6 terminator
   lines for the 7-key `dkssud` on the RICHEDIT50W probe.
3. *Termination drops the composed text*, the premise the #119 body relies
   on for "every key evaluates its fresh/bare-jamo branch". **CORRECTED.**
   The composed text is **fused** into the document by termination, not
   dropped: after `dkssud`, the probe control reads ㅇㅏㄴㄴㅕㅇ — the
   concatenation of each key's terminated composition text. The garbage
   output is caused by the terminator resetting the engine *after* each
   key (so each key composes from zero) PLUS that fused residue — the
   never-deliver-twice rule is right, but "her fusion leaves one char per
   key in the document" is a fact the legacy path has to understand:
   a legacy path that inserts per key collides with the host's own fuse.
4. *`fInterimChar` (TF_SELECTIONSTYLE) is host-displayed* — docs-measured:
   "an interim character selection … visually represented as a solid
   rectangle that is usually flashing … a standard UI element of Korean
   … compositions … fInterimChar can only be nonzero for a single
   selection. In this case, there will be no caret" (msctf.h TF_SELECTIONSTYLE).
5. *The TSF-immediate-sink route has prior art with the SAME measured
   artifacts.* jamotong_ime's `winapi-c-ime-manual.md` (rubidus-api,
   read-only — an independent Korean-IME TSF manual): "Transitory contexts
   terminate compositions per key by design → commit-only";
   "in-place replacement (`ShiftStart`+`SetText`) … some tested restricted
   text stores did not honor the backward range edit, so text accumulated
   (`ㄱ가간...`)"; "product fix — route only validated capabilities. For
   matching, field-tested EDIT-family controls, drive the control directly
   with `EM_REPLACESEL`". Terminal's TSF client (microsoft/terminal#20481)
   documents `TF_TMAE_CONSOLE` as disabling "single character composition"
   for Korean IMEs — the IMM-style host mode the probes sit in.

A premise the issue did NOT state and the tree forces on every option:
on a default-sink host, NOTHING in-place rewrites — compositions fuse per
key — so every legacy-path variant's per-key writes must be
anchor-replacement-clean (see the accumulation artifact below) — and the
candidate anchor has to carry the doc-resident text (EXP-C).

## 2. The spike protocol

Every probe exe in the session's spike dir,
`frontends/tsf/build-msvc/spike/`:

- `ime-probe-spike.c` = the checked-in in-tree probe
  (`frontends/tsf/test/ime-probe.c`) + a first-thing entry log — the
  DEFAULT mftedit host class (title `RICHEDIT50W`);
- `ime-probe-plain.c` = the same with `FORCE_EDIT_HOST` — the plain EDIT
  bisect (round 13's class);
- the TIP build (this worktree's hunmin-tsf.dll) deployed **by file swap**
  into the registered path per the measured deploy recipe;
- driving: the scratch driver `b119-round3.py` (ctypes SendInput with
  `VK'-classified keys, `~` = VK_HANJA, digits/case handled), launched via
  the terminal; the control text read by `WM_GETTEXT`.

The windows-subsystem wedge: freshly built `/subsystem:windows` exes never
entered `wWinMain` on this machine this session (three for three:
ThreadState=Initialized, 1 thread, ~2 MB RL, no logs) while
`/subsystem:console` builds of the same sources ran; every probe variant
was built `console` for the rounds and nothing about it affects the TIP
(the DLL is a static-linked engine inside the probe either way).
Unresolved host fact, recorded for the round.

## 3. EXP-A — the interim direct path, trained and landed

Shape (env-gated `HUNMIN_TSF_INTERIM=1`, spike only; NOT the shipped
design): no `ITfComposition` is created at all on a consumed key. The
in-flight syllable lives in the document as text at adapter-bookkept
`preStart_..preStart_+preLen_`; each consumed key rewrites EXACTLY that
span (select + `EM_REPLACESEL` — class-aware: `EM_EXSETSEL`+CHARRANGE for
the msftedit richedit, `EM_SETSEL` for plain EDIT; the TIP resolves the
control via `GetFocus()` because `QueryInterface(IID_ITfContextOwner)`
fails on both probes — measured) and the engine's finalised syllables
append before the in-flight span per key. On a boundary (declined key),
the engine's flushed in-flight joins the finalised buffer, the final span
text lands once, and the bookkeeping resets.

The FIRST rewrite shape — remembered ITfRange + `ITfRange::SetText` per
key — **accumulated text exactly like the prior art** (doc: 1+1+1+2+2+2 =
9 chars for the 7-key `dkssud`; span read-back `want == fetched` after
every SetText, so the span itself always covered the just-written text
while the DOCUMENT collected the history — the same ` ㄱ가간`-shape
artifact, in the same place). The docs' anchor semantics (Ranges: anchors
tracked through edits) did NOT give replace-in-place through a kept range
on these hosts. Rewritten to select+`EM_REPLACESEL` per key.

### EXP-A measured rows

| Row | RICHEDIT50W probe | plain-EDIT probe |
|---|---|---|
| `dkssud ` → control text | `안녕` (2 chars) | `안녕` (2 chars) |
| composition lines for the round | **0** | **0** |
| `OnCompositionTerminated` | **0** | **0** |
| per-key caret bookkeeping (`caret==expect`) | exact, all keys | exact, all keys |
| Hanja key over the word (see EXP-C) | `src=안녕 count=1` | `src=안녕 count=1` |
| interim-selection mark (see EXP-B) | `E_INVALIDARG` per key | same class measured |
| env OFF (shipped shape unchanged) | 6 compositions + 6 terminations for `dkssud` (the #119 finding state) | not re-run (same DLL, same gate) |

The composition count row is the issue's first verification checkbox,
measured here on this host: **the trace shows no per-key engine reset and
no composition lifecycle at all** — the word buffer concentrates
doc-side, the caret bookkeeping exact per key.

### §3-D Additional round facts (measured at implementation, 2026-10-20 — corrections to the rows above)

The implementation rounds (the same dev host, both probes) re-measure the
{space} row and add three host facts the spike's rounds did not capture;
per the repo's correction convention these are dated, not rewritten into
the table:

1. **The `` row's `안녕 (2 chars)` was an artifact of the eaten
   space, not the interim path's verdict.** The shipped predicate ate
   `VK_SPACE` at the TEST phase (`eat=1`, unconditional in hangul mode), so
   the spike's round never saw the space at all. With the boundary drain
   running at the test phase, the space lands (`dkssud ` →
   `안녕 `, 3 chars, measured both probes).
2. **msftedit does NOT call `OnKeyDown` for an uneaten printable key.**
   With `eat=0` at the test phase the down phase never fires — the test
   phase is the TIP's only sight of the key. The mock's `Key()` always
   calling `OnKeyDown` is why the mock stayed green while live broke.
3. **The drain (any write from the timer) runs AFTER the app's own
   boundary insert; a span select there parked the app's caret inside the
   word, ahead of the space — following keys landed before the space and
   the backspace deleted the wrong char.** The app's caret is not ours to
   move: saved and given back after every drain.
4. **The one-char Hanja ruling** (maintainer, 2026-10-20: "in interim
   mode, only one char conversion should be supported") is implemented on
   the same branch: the candidate anchor is the IN-FLIGHT preedit
   (`src=녕 count=35` measured live, superseding the EXP-C whole-word
   anchor whose multi-char candidates cannot commit without corrupting
   the word — `안녕` + `安寧` → `안安寧`, measured); a candidate commits
   only when exactly one char (`dkssud~1` → `안寧`), multi-char
   candidates refused whole.

The implementation carries these in the code commit on this branch
(this commit); the spike itself stays a measurements-only record.

## 4. EXP-B — does the host render an `fInterimChar` selection?

The picked measurement. Implemented per the docs' own wording (one char,
`ase = TF_AE_NONE`, the caret back over the in-flight span via
`ITfRange::ShiftStart` in an own edit session; `SetSelection` with
`fInterimChar = TRUE`).

**Measured: not reachable from the TIP side.** `SetSelection` with
`fInterimChar = TRUE` returns **E_INVALIDARG on every key**, both host
classes; each shift's first attempt reports `moved=0` from the caret it
read; a full-screen capture JUDGED (the maintainer's eyes are owed on the
packaging row) shows the composed text rendered as plain committed-style
text — cursor, no highlight rectangle, no underline. So the
interim-char display IS NOT a TIP-side instrument on these hosts: whatever
renders it must be the app, and our TIP's `SetSelection` route with the
interim flag is rejected outright on both.

What the interim path STILL renders inline: the in-flight text itself,
plain. That is a USER-visible difference from the MS-IME-style composition
(the ruled target: transparent background, underline only — a target that
has never been met by any encoding tried on the NATIVE path either, per
the measured negatives in the working skill notes). Neither EXP-A nor any
sub-state restores that ruled rendering via the interim route.

## 5. EXP-C — the candidate row (the issue's checklist row on both classes)

Opening (VK_HANJA at the interim path's mid-word moment) measured the
anchor collapse FIRST: `src=녕 count=35` (the whole-word lookup fell to
its last-syllable fallback because the interim path had already delivered
the finalised prefix into the doc, leaving `wordbuf_` empty). The fix —
the interim path keeps the doc-resident finalised text in
`interimPrefix_` and the anchor (BufferedHangul) reads it BEFORE +
wordbuf_ + preedit — measured **`src=안녕 count=1` on BOTH host
classes**: the row the issue asks for (a non-empty source) opens with the
WHOLE buffered word on the interim path.

## 6. The options after the spike (comparable, undecided)

| | A. interim direct path (EXP-A as measured) | B. overlay restore (pre-#98 shape) | C. hybrid per-context |
|---|---|---|---|
| Mechanism | no composition; the in-flight syllable is doc text, rewritten per key by `EM_REPLACESEL`; finalised syllables append in-doc | the engine's in-flight lives in a floating topmost `STATIC` window over the caret; the doc only receives ONE insert per word at the boundary (`FlushSession`) | modern hosts keep #98 native; a context that terminates a composition flips to A or B for its life |
| Composition lifecycle | none on the legacy path (nothing to terminate) | none | native where possible |
| Bytes in doc during composition | the word-resident FULL accumulation (inline visible, plain) | only the committed prefix (only in-flight visibly renders in the OVERLAY) | — |
| Candidate anchor | carrying in `interimPrefix_` (measured-green) | wordbuf_ + preedit (already the #73/#100 shape) | either |
| Underline/composition styling | **none** (plain text; EXP-B measured unreachable) | the overlay can STYLE the preedit freely (its own window) | mixed |
| Hosts in scope | EDIT + RICHEDIT50W measured | both measured working pre-#98 | both, plus any later host |
| Non-EDIT hosts (XAML, Scintilla) | `EM_REPLACESEL` is meaningless — `GetFocus` resolves nothing TSF-shaped | the direct insert part (`FlushSession`) is still TSF-side and works; the overlay is host-agnostic | per-context fallback by measured behaviour |
| What would change the verdict | a host class seen NOT terminating compositions and NOT an EDIT family | if the overlay's caret-rect anchoring measures badly in the sandbox rounds | if exactly one of A/B measures badly on a real host class |

The #119 issue's own phrasing prefers A-then-B — "the overlay
restoration measures badly" as the condition for a B spike. The spike's
measured artifact for A: no composition styling at all (EXP-B) and the
`GetFocus` resolution being thread-local (focus-driven, not
TSF-owner-driven). B's cost is the restoration of ~100 lines from git
history (the overlay window + the caret-rect machinery the candidate
popup already carries).

## 7. Provenance

Environment: Win11 dev machine (build 26200), VS2022 BuildTools 14.44,
Windows SDK 10.0.26100; every probe exe built per-build with `cl` and the
TIP by `cmake --build frontends/tsf/build-msvc --config Release
--target hunmin-tsf`; the DLL swapped into
`windows-milestone-d-candidate-and-edit-mode-ux/…/Release/hunmin-tsf.dll`
(the registered path — the swap itself does not need elevation; the
`/u /s` cycle was NOT re-run this session — the UAC prompt was not
accepted; the registration metadata stayed valid because the registered
path and GUIDs did not change).

Commands-as-measurements (every figure above is anchored):

- the per-key/per-host rows: `grep -E 'interim:|hanja key|composition
  start|Terminated' <spike>/hunmin-tsf-debug.log` while the round's own
  probe log lives beside it (`ime-probe.log` carries
  `ActivateLanguageProfile … hr=0x…`);
- the control-text rows: `WM_GETTEXT` of the probe's edit control as the
  driver prints it (the `CONTROL TEXT:` lines, `\uXXXX`-escaped by the
  driver);
- the visual rows: full-screen captures saved as `screen*.png` in the
  spike dir, judged with the analysis description quoted in §4;
- the premise checks: `git grep`/`git log --oneline … -- frontends/tsf/…`
  against this worktree's base.

READ-BUT-NOT-EXECUTED facts, labelled: jamotong_ime's
`winapi-c-ime-manual.md` §8.1/§8.2 claims (transitory termination is
by-design; the restricted-store accumulation artifact) in EXP-A were
measured HERE as well (the accumulation reproduced exactly), but the
manual's OTHER rows (InsertTextAtSelection insert-first, per-regional
re-writes, EditCtl_ReplaceSelection's selection postconditions) are
NOTED, unexecuted. Sandboxed fresh-install rows and the interactive
Edge/Notepad rows of the checklist stay owed at their own dev stage —
this spike measures the dev host, and BOTH probe host classes.

Changed files: `frontends/tsf/src/tsf.cpp` (uncommitted; returned to base
`0e0b84c` before the tier run below), the spike dir under the gitignored
`frontends/tsf/build-msvc/` (probe sources, build bat, driver, screen
captures — out of every diff by design), and this document.
