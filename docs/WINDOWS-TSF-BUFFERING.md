# Windows TSF buffering model for Hunmin

**Status: design record.** What must hold is `docs/SPEC-TSF.md` (TSF1, TSF4) and
`docs/SPEC-FRONTEND.md` (FE4, FE5); why the buffer lives in the library rather
than in this adapter is the comment above `hunmin_buffered_preedit_new`
(`libhunmin/src/buffered_preedit.c`). This page keeps the design that got the
frontend there, and the two corrections measured along the way.

## Purpose

Avoid leftover and duplicated preedit text. The frontend treats TSF as a
commit-only surface: finalized syllables belong to the document, and what is
still being composed stays the engine's until a boundary. The overlay that the
first design used for preedit is retired (`docs/WINDOWS-TSF-API-NOTES.md` records
why inline composition replaced it).

## The rule that survived, and the two that did not

- **Insert exactly once, at a boundary.** A key the engine consumes inserts
  nothing; a declined key, Enter, an edit key, a toggle or a chord drains the
  buffered word once. This is now `SPEC-FRONTEND` FE4 and is asserted by the
  win32 corpus.
- **Rule 3, "never keep a TSF composition that owns the same text you later
  insert", was superseded 2026-10-20 (issue #98).** The adapter now opens an
  inline composition for preedit; the rule's intent survives — the same text is
  never delivered twice — because `EndComposition` fuses the range into the
  document at the first boundary.
- **The insert path is flattened commit text through a synchronous edit session**
  (`RequestEditSession`, `TF_ES_SYNC | TF_ES_READWRITE`, `ITfRange::SetText`).
  The draft's `ITfInsertAtSelection` wording was never implemented; measured
  2026-10-17 and corrected here.

## Measured dates

- 2026-10-17 — the adapter state confirmed end to end in Notepad.
- 2026-10-20 — inline composition for preedit ruled in (issue #98); the overlay
  kept only the candidate popup.
- 2026-11-02 — the automatic switch's delivery measured live in a Chromium
  textarea (issue #216): caret, undo unit and selection with the switch ON and
  OFF, in the section below.

## Measured state, and what moved since

Measured 2026-10-17 (issue #73): finalised syllables accumulated in the ADAPTER
(`wordbuf_`), the overlay showed the word plus the in-flight preedit, and the
first boundary key produced exactly one insert of the whole word. Issue #80 gave
that contract an automated check (mock-TSF, tier F1: one insert per boundary).

**The adapter-side buffer is gone** (issue #128, 2026-10-25): the word lives in
`hunmin_buffered_preedit`, and the adapter hands every key to the layer. What the
2026-10-17 measurement asserted is now `SPEC-FRONTEND` FE4 with the same
assertion channel — the win32 corpus — and the reason the buffer moved is the
comment above `hunmin_buffered_preedit_new`.

## The host's own termination already delivered the word (fixed 2026-11-02, issue #175)

A host-driven `OnCompositionTerminated` (`endingSelf_ == 0`) FUSES what the
composition showed into the document — that is what the host's termination means.
The layer is deliberately not told (the `ic` keeps composing its in-flight
syllable, issue #144), so the adapter has to remember that the text is now client
text. It did remember it (`hostFusedCopy_` for the native path,
`CaptureDeadCopy` for the interim one) but only a *later composition* consumed the
note. A boundary arriving first — the person leaves the window mid-word, comes
back and presses space — delivered the layer's copy on top of the fused one, which
is the reported `적어두자.적어두자.` (issue #175 comment, 2026-11-01).

The rule now: a pending fused copy means the document holds the text, so a
boundary DROPS the layer's copy instead of inserting it (`InsertBufferedWord`;
the reset also clears any judgement that would have rewritten text the client
already holds, ReqMode10). Pinned in both deliveries by the same invariant — *the
boundary adds nothing*:

- `TsfConfigTest.HostTerminationThenBoundaryKeepsTheWordOnce` (native; measured
  before the fix: the 3-character word twice, document length 6),
- `LegacyTest.HostTerminationThenBoundaryKeepsTheWordOnce` (the classic host,
  which flips to the interim path on its first mid-word termination).

Removing the `hostFusedCopy_` branch turns the native case red with exactly the
reported shape (`ㅎ곈디ㅎ곈디`), which is what makes it a guard rather than a
description.

What this does NOT decide: a host that terminates the composition AND removes the
fused text itself would now lose the word at that boundary. No such host has been
observed (the termination IS the host keeping the text), and the re-adopt path
still refuses to take a copy the document does not carry, so the residue case is
unchanged — but the boundary rule assumes the fused copy is still there.

## The automatic switch, measured live (2026-11-02, issue #216)

ReqMode8's E1 line is judged in the mock; what the client does with the keys was not.
This section is the live round: one host, one control, the same keystrokes with
the switch ON and OFF, and the caret, the undo unit and the selection compared as
numbers instead of described.

`Autoswitch` ON means English-mode keys go **through the engine** instead of
being passed to the host (`docs/FRONTEND-EQUIVALENCE.md` §2.2), so the delivery
is no longer the host's insertion. That is the surface measured here; the text is
the same, the delivery is not.

**Host and control.** A Chromium textarea (Edge, one fresh `--user-data-dir` per
round): a TSF-native host, so the adapter keeps the native delivery, and a
control whose `value`, `selectionStart` and `selectionEnd` can be read after
every key without a keystroke. The two alternatives were measured for this
question and do not serve it: the RICHEDIT50W host
(`frontends/tsf/test/ime-probe.c`) logs `legacy detection: mid-word host
termination` on its first key and flips to the interim delivery, where §2.1 turns
the switch off by design — measured with `--script '%ksms '`, the document is
`sms` with one composition and no judgement; and a Notepad readback needs a key
of passage (Ctrl+A), which `docs/SPEC-TSF.md` §TSF8 makes flush the word being
measured — the read would destroy the state it read.

**Tool.** `tools/tsf-autoswitch-round.py`. The page mirrors its state into the
window title, the driver polls it after each key, and `round.json` in the run dir
keeps every row:

```sh
python3 tools/tsf-autoswitch-round.py --switch on  --layout dubeolsik --keys '{L}English ' --out build-tsf-round/on
python3 tools/tsf-autoswitch-round.py --switch off --layout dubeolsik --keys '{L}English ' --out build-tsf-round/off
```

`{L}` is the 한/영 key (`docs/SPEC-MODES.md` §E5); everything else is typed.

### What the engine's delivery costs, and what it does not

`{L}English ` — Latin mode, seven characters, then a boundary. Run 5 times per
mode, identical every time except the row named in §"Findings".

| | switch OFF (host inserts) | switch ON (engine delivers) |
|---|---|---|
| adapter log | 0 `composition start` | 1 `composition start`, 1 `composition end text=English` |
| key path | `OnTestKeyDown eat=0` per key — the host's | `consumed, display len=n preedit=…` per key |
| document after each key | `E` `En` `Eng` `Engl` `Engli` `Englis` `English` `English ` | same (see the one exception below) |
| text | `English ` | `English ` |

So the premise "every English character becomes an edit through the adapter" is
half right and half wrong, and the wrong half is the load-bearing one: the keys
are **consumed** one at a time, but they are delivered into ONE inline
composition range that the adapter grows per key and closes once at the boundary.
Not one edit per character.

### The sentence (ReqMode8's release gate, live)

Keys `English sms whgdms Language dlqslek `, typed from Hangul mode with no
manual toggle, on `dubeolsik` — the keystrokes of
`tests/golden/judgement-r8.jsonl` `r8-mixed-sentence-gate`:

| | switch ON | switch OFF |
|---|---|---|
| document | `English 는 좋은 Language 입니다 ` | `뚜히ㅑ노 는 좋은 ㅣ무혐ㅎㄷ 입니다 ` |
| `autoswitch -> mode=` rows | 2 `english`, 2 `hangul` | 0 |
| compositions (start/end) | 5/5 | 5/5 |
| `legacy detection` | 0 | 0 |

The feature works end to end on this host: the judgement flips the mode four
times inside one word stream and the intended sentence lands. With the switch OFF
the same keys deliver the Hangul readings throughout — the switch is the whole
difference, not a cosmetic one.

### Caret

Selection start/end after every key, as offsets into the control's value
(`sel == len` means the caret is at the end of what the host holds):

| key | OFF `sel` | ON `sel` |
|---|---|---|
| `E` `n` `g` | 1,1 / 2,2 / 3,3 | 1,1 / 2,2 / 3,3 |
| `l` | 4,4 | **2,2** (the document holds `뚜히` — finding below) |
| `i` `s` `h` | 5,5 / 6,6 / 7,7 | 5,5 / 6,6 / 7,7 |
| space | 8,8 | 8,8 |

Apart from that row the caret is at the end of the delivered text at every step
in both modes, and the boundary does not move it into the word — the defect class
`docs/TSF-INTERIM-INPUT-SPIKE.md` measured for the interim path does not appear
on the native one.

### Undo unit

One Ctrl+Z per press, four presses after `{L}English `; the number is the
document length after each press. Two rounds per mode, identical — a third ON
round is the run in issue #222 and is excluded here rather than averaged in.

| press | OFF | ON |
|---|---|---|
| 0 (typed) | 8 `English ` | 8 `English ` |
| 1 | 7 `English` | 7 `English` (selection 7,7) |
| 2 | 6 `Englis` | 7, no text change, **selection becomes 0,7** |
| 3 | 5 `Engli` | 6 `Englis` (selection 0,0) |
| 4 | 4 `Engl` | 5 `Engli` |

Both modes undo one character at a time — the adapter's per-key growth of the
composition is one undo entry each, exactly like the host's own insertion. The
difference is one extra entry in the engine path that changes no text and
selects the whole document. Four presses remove four characters with the switch
OFF and three with it ON.

### Selection

`{L}English{SELALL}x ` — select the whole document, then type one letter and a
space:

| | switch OFF | switch ON |
|---|---|---|
| after `{SELALL}` | `English`, sel 0,7 | `English`, sel 0,7 |
| after `x` | `x`, sel 1,1 | `x`, sel 1,1 |
| after space | `x `, sel 2,2 | `x `, sel 2,2 |

Identical. Typing over a selection still replaces it when the engine is the one
delivering, so the host's insertion semantics survive the detour.

### Findings

Two surfaces regressed in this round, each with its own issue rather than folded
in here:

- **issue #221** — after a 한/영 press, the first Latin word shows its Hangul
  reading for one key (`preedit=뚜히` where the word is `Engli`), 5 of 5 ON rounds
  and 0 of 5 OFF. The document held it too, so it is a delivery defect, not a
  paint.

  Cause, attributed 2026-11-07 (issue #221): the layer, not the adapter.
  The adapter's log line prints `ComposeDisplay()`, which is the layer's
  `build_preedit` — and in Latin mode its `judged_display` branch (E1 +
  E6's per-key default) answers the Hangul reading whenever the prefix is not
  a dictionary word and the reading is well-formed syllables. At `l` that is
  `Engl` (not a word) over `뚜히` (two syllables); at `i` the reading gains a
  dangling 초성 and stops being well-formed, so the display falls back to
  Latin one key later. That is why the flash is exactly one key, why the
  mixed sentence typed from Hangul mode shows no such step (Hangul is the
  current mode there, so its display is Hangul throughout), and why the
  document held it: the native delivery writes `ComposeDisplay()` into the
  composition range, faithfully. The engine case
  `WordSetTest.PerKeyTimingInLatinModeShowsHangulForEnglishThenReturns`
  pins the per-key sequence (`E En Eng 뚜히 Engli Englis English`) and that
  the boundary timing shows `Engl` at that key with the same delivered
  text. No TSF code change follows from this attribution: the ruled options
  are the timing default itself (`judge_timing`, E6) and the E1 per-key
  rule, and changing either is a ruling, not a fix — the priced Hangul
  direction of the same flicker is §BND-4, and its Latin direction is the
  unpriced half this round measured. What would change the recommendation
  is a human typing measurement of the Latin direction, which this round
  did not take (see below).
- **issue #222** — in 1 of 8 ON rounds the host accumulated the composition text
  (26 characters for a 7-key word, 10 `composition start` for 0 `composition
  end`) and the adapter consumed Ctrl+Z in that run. A race, measured once; its
  trigger is not claimed.

  Fixed 2026-11-07 (issue #222): two adapter-side defects, both pinned in tier
  F1 by `NonEditHostTest.HostEndingCompositionInsideTheSessionComposesTheWord`
  and `NonEditHostTest.CtrlChordPassesInLatinModeWithTheSwitchOn`, and both
  fail against the pre-fix sources with exactly the reported shapes (the
  26-character accumulation, `OnTestKeyDown w=90 eat=1`).

  1. *Ctrl+Z.* The test-phase chord rule was gated to Hangul mode, while the
     down phase classified the chord mode-free: in Latin mode with the switch
     on, the test claimed the letter (`eat=1`) and the down phase passed it. The
     gate is now mode-free (the English-passthrough branch above it still
     returns when the engine owns no keys), so test and down agree.
  2. *The accumulation.* The composition edit session published the shown
     text (`compText_`) only AFTER the host-visible `SetText`/`SetSelection`,
     so a host termination delivered re-entrantly from inside the session
     (the mock delivers the sink from its `SetSelection`) recorded the
     PREVIOUS key's text. The next composition then re-adopted the wrong copy,
     the document-text verify refused it, and the key appended: each key's
     display landed after the last, `1+2+3+2+5+6+7 = 26`. The session now
     publishes before the writes, collapses the caret at the range end when the
     sink ended the composition mid-session (the caret the next re-adoption
     measures), guards its own end against a composition the host already
     ended, clears the stale fused note at a boundary end, and re-adoption
     measures what the selection already covers instead of assuming a caret
     after the copy.

  What this does NOT claim: the trigger, still. The mock models the timing the
  issue names - the host closing inside the session - it does not measure
  Chromium. Whether the in-session window is where Chromium closes, at what
  rate, and whether the fixed shape holds against the host's own closing
  policy, is the live round's to measure (`tools/tsf-autoswitch-round.py`,
  repeated `--switch on --keys '{L}English {UNDO}...'` runs). The 1-of-8 rate
  above stands until that round re-measures it.

### What this round does not measure

- A human typing at speed: the rounds are scripted at 0.45 s per key, and #221's
  flash was never watched by a person.
- The Win11 Notepad class and any host that withholds `selectionStart` — the
  measurements above are Chromium's, and the mftedit class cannot carry the
  switch at all (§2.1).
- The IME-indicator half of the issue: the langbar was not read during these
  rounds.
- Undo after a mode judgement inside one word stream: the undo table is the
  English stretch only, where ON and OFF deliver the same text.

## Open questions

- Whether UWP and console edit controls need an overlay for readable preedit.
- The caret-rect source for any overlay movement on such hosts.

## Key routing notes (field test 2026-10-18, issue #67)

The chord and edit-key rules measured here are requirements now: a CTRL/ALT chord
flushes and passes (`docs/SPEC-TSF.md#§TSF7`), and the edit/navigation cluster is
a key of passage (`docs/SPEC-TSF.md#§TSF8`). The verbatim user note those came
from is quoted in `docs/SPEC-TSF.md`.
