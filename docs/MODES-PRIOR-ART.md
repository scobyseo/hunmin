# MODES-PRIOR-ART — how other IMEs convert, suggest and mix scripts

Status: **prior-art survey for Track 2's design question, not a ruling.** Track 2's
purpose (`docs/MILESTONES.md` §0.4 Track 2) is to find the design the automatic
switch should have, by proposing, building and measuring alternatives. This file
records what other input methods already do, so those alternatives are argued
against shipped practice rather than against intuition.

Method: reading upstream **sources and keymap data**, not blogs. Three of the six
cases are on this machine (`~/work/ibus-hangul-1.5.5`, `~/work/fcitx5-hangul-5.1.9`,
`~/work/kime`), two are read through the GitHub API (`google/mozc`
`src/data/keymap/ms-ime.tsv`, `rime/rime-prelude` `default.yaml` and
`key_bindings.yaml`). **No code is copied**: Mozc is BSD-3-Clause (its `COPYING`;
GitHub reports NOASSERTION), `rime-prelude` LGPL-3.0, `ibus-hangul` GPL-2.0-or-later,
`fcitx5-hangul` LGPL-2.1-or-later — read for facts only, per `AGENTS.md`.

The three proposals being compared are recorded verbatim in
`wiki/raw/sessions/2026-11-27-t2-direction-and-the-three-proposals.md` §1:
**(1)** show the alternative instead of replacing the input, **(2)** convert only
when a key asks, **(3)** allow an English word + Korean particle.

## 1. The cases

| # | system | what it does | which proposal it is prior art for | how it was checked |
|---|---|---|---|---|
| 1 | **Mozc / Google 日本語入力**, `ms-ime` keymap | While **composing**, only these convert: `Space`, `Henkan`, `Shift Space` → `Convert`; `Tab` → `PredictAndConvert`. Everything typed before that is the person's own kana; predictions are **candidates**, never a rewrite of the composing run | (1) suggest-only, (2) explicit convert | keymap source read: `Composition` rows |
| 2 | same, `F6`–`F10` | The **composing run** can be re-rendered in another script by one key: `F6` hiragana, `F7` full-width katakana, `F8` half-width, **`F9` full-width alphanumeric**, **`F10` half-width alphanumeric**; `Ctrl i/o/p/t/u` are the same family, `F2` = `ConvertWithoutHistory` | (2) one key converts what is being composed; also (3)'s Latin half (F10 turns the composing run into ASCII) | same keymap, `Composition` rows |
| 3 | same, `Reconvert` | `Henkan` in `DirectInput`/`Precomposition` re-converts **already committed** text. Known cost, recorded in this tree already: it needs the host's surrounding text, which many applications break (`docs/FCITX5-ADDON-NOTES.md` §"Surrounding text: use for information, do not modify") | (2) as a repair of committed text — the *expensive* variant of it | keymap `DirectInput`/`Precomposition` rows + the notes file named to the left |
| 4 | **Rime** (`rime-prelude` `default.yaml`) | `recognizer/patterns` declares a Latin shape (`url: ^(www[.]|https?:|ftp[.:]|mailto:|file:).*$\|^[a-z]+[.].+$`) that is recognised **inside** the Chinese input and offered as a raw candidate. Mixed Latin/CJK text with no mode switch, decided by a *pattern*, not by a dictionary lookup | (3) — the closest shipped mechanism: the mixed token is a candidate, and the rule is data | config source read (`recognizer/patterns`) |
| 5 | **ibus-hangul** (on this machine) | `hanja_keys` (Hanja, F9 by default) looks the **composing word** up in the hanja table and opens a candidate list; picking one replaces that word (`hanja_list` → `apply_hanja_list`, `hanja_mode` is temporary and cancelled by Backspace) | (2)'s mechanism, for a different question: **one key, composing word, candidate list, in-place replacement** — no surrounding text, no retyping | source read: `src/engine.c` (`update_hanja_list`, `apply_hanja_list`, `hanja_keys`) |
| 6 | **fcitx5-hangul** (on this machine) | Same hanja path through libhangul, with `wordCommit` and `hanjaMode` as config; `HunminCandidateWord::commitCandidate` in our own addon is the same shape (`docs/DESIGN-FRONTEND.md` §5) | (2) again; also the composing-unit setting that (2) would need | source read: `src/engine.cpp` (hanja list, `wordCommit`) |

Two negative results matter as much as the positives:

- **None of the three Korean IMEs on this machine (`ibus-hangul`, `fcitx5-hangul`,
  `kime`) has an automatic script switch.** A grep for `autoswitch` / `auto?switch`
  / language detection in their sources returns nothing. The "suggest instead of
  convert" pattern is a **Japanese/Chinese** convention, and this feature has no
  Korean precedent to inherit — so its shape is a design decision, not a
  compatibility question.
- **`Tab` has a job of its own in each system surveyed.** Mozc binds it to
  `PredictAndConvert`, Rime to `move_by_word_with_tab` (default `Shift+Right`
  while composing), and our own Android keypad ships it as a navigation key
  (`docs/ANDROID-KEYPAD-MOCKUP.md` §KP6). **What our Linux hosts do with `Tab`
  while a candidate list is open was not measured here** — the tree has no capture
  of it, and the repo's rule about stated premises is why this says so instead of
  assuming. What would settle it: a D2 UI capture per host with a list open
  (`docs/DESIGN-FRONTEND.md`'s tier). Until then the safe shape for proposal (2) is
  one of our own E5 special-function chords, which no host claims and every
  frontend resolves through one table.

## 2. What the survey settles (facts, not rulings)

- **Shipped practice separates "what I show" from "what I commit".** Mozc's
  predictions and Rime's recognized Latin are *candidates*; the composing run is
  only re-rendered when the person asks (`F6`–`F10`, `Space`, `Tab`). The one
  place practice *does* rewrite what a person typed is the composing run under an
  explicit transform key.
- **The mechanism proposal (2) needs already ships for a different question.**
  One key, the composing word, a candidate list, an in-place replacement: that is
  the hanja path on all three of our frontends (`hunmin_suggest_*` →
  `commitCandidate`). Pointing it at the language judgement is a *re-wiring*, not
  a new surface.
- **Converting committed text is the expensive variant.** Reconvert needs the
  surrounding text, and this tree's own notes record that many applications break
  it. Converting while the word is still composing needs nothing beyond the buffer
  the engine already holds (`ReqMode11`'s cheap repair).
- **Mixed-script tokens are handled by a pattern, not by segmentation, in the one
  system that ships them** (Rime). That is a cheaper shape for proposal (3) than
  the stem/josa split: match the Latin run, offer the literal as a candidate, and
  let the particle come from the ordinary Hangul reading. What it does *not*
  answer is which side wins when both parse — a ruling, not a fact.

## 3. What it does not settle

- **Felt experience.** None of this measures whether a person notices a hint, or
  how a wrong offer costs them. Mozc's and Rime's shapes have years of users; that
  is an argument from survival, not a measurement, and it is labelled as such.
- **Windows / macOS Korean IME behaviour** was **not** verified here — no such
  host was touched, so nothing in this file describes them. Claims about them
  (e.g. what 한/영 does while composing, or whether Hanja conversion replaces the
  composing word) would need a probe on those platforms; the repo's rule about
  trusting stated premises is exactly why they are absent rather than guessed.
- **No licence permits reuse.** Facts and shapes only; the implementations are
  GPL/LGPL/BSD and none of their code belongs here (`AGENTS.md` hard rule).

## 4. What each proposal gets from this

| proposal | prior art says | the cheapest thing to try, from this survey |
|---|---|---|
| (1) show the alternative | predictions-as-candidates is the shipped convention; nothing rewrites the composing run until the person asks | reuse the **hanja candidate list** path: publish the other-language reading as candidate 0; picking it commits it (already implemented end-to-end on fcitx5/IBus/Android) |
| (2) convert on a key | Mozc's `F9`/`F10` and the Korean Hanja key are the same gesture; `Tab` carries a different job in each system surveyed (our hosts unmeasured) | add one E5 special function that opens the same list, or converts directly when only one reading is offered |
| (3) stem + particle | Rime matches a *pattern* and offers the literal; nobody segments by dictionary | try a pattern (Latin run + a particle from a five-entry list, `docs/MODES-MEASUREMENT.md` §1.5) before trying a segmentation |

The one thing this survey changes in the plan is the order: **the surfaces and the
replacement mechanism already exist for the Hanja question**, so (1) and (2) are
cheaper than they looked in
`wiki/raw/sessions/2026-11-27-t2-direction-and-the-three-proposals.md` §6, while
(3) is cheaper as a pattern than as a split. None of that is a ruling; the choice,
and whether the track's first experiment changes, is the maintainer's.