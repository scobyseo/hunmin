---
title: 2026-11-04 one coverage golden per layout, and four instrument defects
created: 2026-11-04
updated: 2026-11-04
type: session
status: active
tags: [session, golden, coverage, instrument, layouts, issue-227, sebeolsik, romaja, ahnmatmae]
sources:
  - ../../../docs/TESTING.md
  - ../../../tools/type-hunminjeong.py
  - ../../../tools/gen-coverage-golden.py
confidence: high
---

# 2026-11-04 one coverage golden per layout, and four instrument defects

One line: the corpus had a word per layout and no letter per slot, so a layout
could lose 초성 ㅉ or 종성 ㄳ without any test noticing; building the slot corpus
found four defects, all of them in the instrument, and two of them had already
been reported to me as layout facts.

## The goal was legibility, not percentage

The requirement that started this was "make test cases a person can read in a
diff". A golden whose case is `key/r` and whose expectation is a jamo string
teaches nothing; the same case as `cho/ㄲ` → `까` reads as the letter a user
types. So the unit became **(jamo, slot)**: ㄲ must be typeable as an 초성 *and*
ㄳ as a 종성, because a layout can admit one and refuse the other, and the
세벌식 family does exactly that — 초성 and 종성 sit on opposite physical sides.

`tools/gen-coverage-golden.py` writes `coverage-<layout>.jsonl` and **refuses to
publish** below `--require-modern`. A ten-of-sixty-seven file would be a
coverage claim nobody could refute from the filename, and the refusal is what
turned this into measurement rather than editing: every number that would not
move was a bug somewhere.

## The four defects, and the number that exposed each

1. **A batched answer read against the wrong probe.** 67 probes went through one
   `--batch` call and the answers came back misaligned; sebeolsik-3-90 reported
   25 종성 failures that `--trace` showed composing (`jfs` renders 안, alone and
   in a two-line batch). Fixed by tracing each probe — ~2 s.
2. **A two-key 초성 truncated to its first key.** ㅉ is the ㅈ key twice; keeping
   `keys[0]` typed 자 and reported `cho/ㅉ` unreachable on 세벌식 3-2.
3. **The literal `\` key dropped from the alphabet.** hunmin-sim reads a bare `\`
   as the start of an escape, so the probe excluded the character — and 세벌식
   no-shift puts ㅋ there (`가` + `\` → 앜). keep the alphabet as KEYSTROKE
   STRINGS and carry the escaped form.
4. **A hand-written compose pair preferred over the measured one.** `COMPOSE` held
   ᄍ = ᄌ+ㅅ, written for one layout; 3-2 composes ᄍ from ᄌ+ᄌ, so the order is now
   `C` → measured `CH` → `COMPOSE`.

## Two "layout facts" that were not

Both were reported to me as behaviour of the layout, and both were the probe:

- **겹받침 ㄵ.** The rule is ㄴ then ㅈ, and `안` + the ㅈ key renders 앉 on
  sebeolsik-3-90. The probe missed it because the engine keeps such a 겹받침 as
  TWO jamo in the preedit (`안ᆽ`), which a single-jamo pass cannot see; the pair
  is named by the layout's own compose table (`{a:ᆫ, b:ᆽ, r:ᆬ}`), and the stale
  single-key entry had to be *removed* so it could not win.
- **The 세벌식 3-90 "fewer 겹받침".** I had reached for a Korean Wikipedia
  sentence about 3-90 reducing its 겹받침 set, and it was about to become a golden
  that quietly omitted five letters. The maintainer's rule settled it: a layout
  that cannot compose ㄵ is a layout to fix, not a coverage table to trim. The
  article's claim may still be true about *dedicated* keys; it is not true about
  composition, and the two must not be confused again.

Recording both, because the shape of the mistake is reusable: **a number below the
gate is a claim about the instrument until the instrument is shown to be right.**
That is the same lesson as [[2026-11-03-x11-corpus-replays-every-tier-a-case]] —
a corpus that looks complete because nothing checks the missing case.

## Where the gaps now live

What remains `not_reached` is in each golden's own `meta`, not in prose:
sebeolsik-yet, dubeolsik-yet and sin3-p2-yet (옛한글), plus romaja and ahnmatmae,
whose jamo live on SHIFTED keys and whose unshifted keys commit literal ASCII
(`Fj` renders 가, `kf` renders ㅣㄱ) — both read as 1 of 19 초성 until the probe
was allowed to discover its own 가 context instead of scoring five literals.

Every layout that uses modern jamo now composes every modern slot. No conclusion
is drawn here about whether the 옛한글 or roman-letter gaps are gaps in the
layouts; the metas state what was measured, and the ruling is not mine
([[decisions-belong-to-the-maintainer]]).

## What changed in the tree

- `tools/type-hunminjeong.py` — `probe_slots()` (the instrument, four fixes)
- `tools/gen-coverage-golden.py` — the generator and its gate
- `tests/golden/coverage-<layout>.jsonl` — 12 files, one per layout
- `docs/TESTING.md` — the family, its gate, and that the counts live in `meta`