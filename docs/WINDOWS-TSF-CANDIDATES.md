# Windows TSF candidate (Hanja) window

Issue #67 (milestone D — candidate & edit-mode UX). The F9 Hanja candidate
window over the buffered word: what the engine provides, what the adapter
supplies on Windows, and the measured behaviour.

## What the engine provides

The candidate lookup is engine-owned and already existed before this
milestone (P3): `hunmin_suggest_count/text/detail/kind` over the three tables
in `libhunmin/data/` — `hanja/` (single syllable → ordered candidates),
`krdict/` (words, 국립국어원 한국어기초사전-derived) and `idioms/`. Lookup is
by the whole Hangul word, never per character (`libhunmin/src/suggest.c`,
`hunmin.h` "Hanja candidates (P3)").

Measured against the shipped tables (2026-10-17, scratch probe over
`hunmin_ctx_new` with the repo data dir):

| Lookup | Count | First candidates (kind) |
|---|---|---|
| `안녕` | 1 | 安寧 (word) — unchanged after the (ko,hanja) krdict dedupe (tools/krdict-dedupe.py, 2026-10-18); the row count moved 2→1 |
| `가` | 65 | 可 (word), 可 (hanja), 家 (hanja) |
| `간` | 67 | 刊 (word), 肝 (word), 間 (word) |
| `중국` | 1 | 中國 (word) |
| `가가호호` | 3 | 家家戶戶 (idiom), 家家戶戶 (word) ×2 |

This is what makes the "type one hangul word, press F9, pick the term"
flow work with no engine change — the user's request behind issue #67's
candidate half ("잘 알려진 한자 용어들을 사전에서 찾기. 한 자씩 입력하는 건
귀찮" — well-known hanja terms looked up in the dictionary instead of
one character at a time).

## What the adapter supplies (`frontends/tsf/src/tsf.cpp`)

- **Anchor — the whole buffered word, not the preedit.** The lookup text is
  the accumulated finalised syllables (the adapter's `wordbuf_`) plus the
  in-flight preedit. fcitx5 anchors to its preedit only
  (`frontends/fcitx5/src/engine.cpp` `openCandidates`), because it commits
  each syllable immediately; the buffered adapter holds the word, so the
  lookup must cover it. This divergence is deliberate and recorded: the
  dictionary tables are keyed by words (`krdict`, idioms), so a preedit-only
  anchor would miss every multi-syllable entry the word-buffered frontends
  can reach.
- **Opening may not flush anything.** `hunmin_ic_flush()` would commit the
  anchor out from under the list — the fcitx5 rule, kept.
- **The open/close key is the configured Hanja-key binding** (`v1:<vk>:<mods>`
  grammar, one entry). The DEFAULT is the bare physical **Hanja key**
  (VK_HANJA 0x19) — the maintainer's instruction of 2026-10-18; F9 is NOT in
  the default set any more, but the settings dialog (§below) or the env seam
  (`HUNMIN_TSF_HANJA_KEY`) can bind it. An unparseable stored binding falls
  back to the default rather than matching nothing — the window must stay
  reachable. One shared predicate makes OK and test phases one decision.
- **Keyboard flow** (= fcitx5's, engine.cpp `keyEvent` candidate block):

| Key | While the window is open | Window closed |
|---|---|---|
| Hanja key (the configured binding; default the bare physical Hanja key, VK 0x19) | close | open (nothing: passes through, both test and down) |
| `1`..`9` / `0` | commit that page-relative candidate | — |
| Up/Down/Left/Right | move the cursor (clamped, no rotation — recorded divergence from fcitx5, which rotates) | — |
| PageUp/PageDown | move one page of 10; the cursor follows | — |
| Return | commit the cursor entry (starts on 0, the highest-frequency candidate) | passes through (normal boundary) |
| Esc | close, composition untouched | passes through |
| any other key | close and fall through — typing continues instead of being swallowed | normal path |

- **Page cap = 10** (`kMaxCandidatePageSize`), the value the fcitx5 label
  ceiling forced and the maintainer ruled for issue #45 (engine.cpp
  471-486: 10 candidates survive, 11 abort; 67-row lists measured ~2000px).
  The TSF popup draws its own rows, not fcitx5's label table, so it *could*
  show more — but the digits can only reach one page, so the same ruled
  number is kept and PageUp/PageDown are what the rest of the list sits
  behind.
- **Commit replaces the whole buffered word.** `hunmin_ic_reset()` drops the
  composition the list was anchored to, the adapter's `wordbuf_` is
  replaced by the chosen text, and the normal boundary path inserts it
  exactly once. The Hangul is never typed into the document, so there is
  nothing to delete and no surrounding-text surgery anywhere — the same
  invariant I9 the fcitx5 frontend holds (FRONTEND-CONTRACT.md §5).
- **Boundaries close the window.** A mode toggle (or a settings-Apply
  rebuild, issue #71) is a composition boundary: the candidate list belongs
  to the composition being flushed, so it closes with it and the buffered
  word inserts once.
- **Consumption mirrors the down path.** `OnTestKeyDown` eats the window's
  keys when it is open, and mirrors the F9-open decision exactly (suggest
  lookup in the test phase is a read of tables already in memory) — a
  zero-candidate F9 belongs to the client in both phases. One shared
  predicate (`hunmin_tsf_candkey_eats_test`) makes test and down one
  decision, the same rule the toggle binding uses.

## Presentation (`frontends/tsf/src/candwindow.{h,cpp}`)

`HunminCandWindow` is a pure-presentation popup: one topmost, no-activate
STATIC window whose text is one row per candidate, each row
`>> 1. 中國` (`>>` on the cursor), drawn under the caret rect the adapter
obtains from the shared caret-rect fetch (the same `GetTextExt` edit session
+ `GUITHREADINFO` caret fallback the overlay uses — extracted into
`AcquireCaretRect` for both windows to share). Font follows the host window
(richedit selection face → `WM_GETFONT` → default GUI font), DPI-aware
creation like the overlay. Mouse selection, rotate-on-overflow, in-window
scrolling are NOT in this cut — polish for a later pass, named here so the
absence reads as deliberate rather than forgotten.

Out of the mock harness's scope deliberately: `mock-tsf.h` offers the
harness no screen geometry (`GetTextExt` fails; a console process has no
caret), so the popup takes the hide path and window code is never exercised
in tier F1 — the same seam that keeps the overlay window out of the trace
runs. Everything state-machine-shaped IS exercised: see `tsf-candidate-test`.

## The edit-mode half

The buffered word is editable by backspace through the engine, and the
contract is asserted, not just claimed (`tsf-candidate-test` case 11):
dubeolsik `dkssud` composes `안`(wordbuf) + `녕`(preedit); Backspace walks
the engine's in-flight preedit down `녕 → 녀 → ㄴ → ""` (`hunmin-sim` trace,
2026-10-17), each step consumed, no insert, the key never reaching the app;
one more Backspace hits the empty preedit, the engine declines, and the
boundary inserts the buffered remainder once before the key passes. Nothing
adapter-side deletes or clobbers buffered text — the delete-into-document
happy path is the app's.

## Verification

```sh
cmake --build <tsf-build-dir> --target tsf-candidate-test tsf-trace-emit
ctest --test-dir frontends/tsf/build-msvc -L tier-F1 --output-on-failure
# device-bound (needs a live TIP; see tests/manual-checklists/tsf-hanja-candidates.md):
# select Hunmin, type 안녕 (dkssud), press the Hanja key, pick a candidate.
```

`tsf-candidate-test` asserts (engine-measured anchors): open state and
count, test-phase consumption, Return/digit commit inserting the exact
candidate text once, cursor motion + clamps, paging, zero-candidate
pass-through in both phases, dead-digit fall-through with the one boundary
insert, Esc close, toggle close, and the backspace edit walk.

## Provenance and scope

- Board rows in `docs/REMAINING-WORK.md` are deliberately untouched: that
  file is the shared status board every active session patches, and this
  PR was built while the parallel `tsf-config-dialog` session (issue #71)
  was still open. Rows can be added at merge time; the "no second home"
  rule is satisfied by naming the exclusion here.
- Candidate counts above are data-dependent by nature: a krdict/hanja
  regeneration changes them. The TEST anchors on them are the lookup keys
  (`dkssud` → 2, `rk` → ≥11, `rkrkrk` → 0) — the counts themselves were
  still measured when this file was written, and the test's `>= 11` guard
  is what keeps a silently changed table from passing a wrong expectation.
