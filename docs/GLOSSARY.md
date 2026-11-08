# Glossary

Terms whose meaning is not guessable and that this project uses across several
documents. A term belongs here when **the wrong reading would produce a wrong
measurement**, not merely when it is jargon.

Related: `SPEC-SCHEMA.md` (data contracts), `SPEC-P2.md` (the layout
specification), `MODES-MEASUREMENT.md` (corpus sources and licences).

**Conventions.** The Korean form is authoritative; the English is a gloss, not a
substitute. Where a term has no settled English rendering, say so rather than
inventing one — that is how this list came to exist.

## Hangul structure

| Korean | English | note |
|---|---|---|
| 초성 | choseong | initial consonant |
| 중성 | jungseong | vowel |
| 종성 | jongseong | final consonant (**받침** when attached) |
| 겹모음 | — | compound vowel; no settled English one-liner |
| 겹받침 | — | compound final consonant, e.g. ㄳ ㄵ ㄶ |
| 한자 | hanja | Sino-Korean character; **the feature's own subject matter**, so it may appear literally where the data is discussed |
| 낫개 | — | final cluster written as two separate jamo |

## Morphology

| Korean | English | note |
|---|---|---|
| 어절 | eojeol | **space-delimited word.** The unit the hangul/latin judgement operates on. Not "word" — a Korean sentence's word boundaries are not recoverable from the text without a morphology step. |
| 조사 | josa | postposition particle. Attaches to a noun: 이·가, 은·는, 을·를, 에·에서. **Phonologically identical to the copula 이**, which is why a surface-form test cannot separate them. |
| 어미 | eomi | verbal/adnominal ending: 다, 는, 었, 며, ㅂ니다. Attaches to a verb stem. |
| 어간 | eomgan | verb stem — the form an ending attaches to. Distinct from the dictionary form (`가다` → `가`). |
| 활용 | — | inflection. **The source of the ~85% gap between a dictionary and a corpus.** |
| 동일본존사 | — | subject-position copula: 이다/이에요/예요. Not a josa, despite being spelled like one. |
| 오모르프 | allomorph | one morpheme, several surface shapes. Relevant here because NIKE writes endings in the 받침-led form (`ㅂ니다` = ㅂ+니다), so a naive suffix stripper silently drops 121 of the 722 entries. |

## Romanisation and layout

| term | note |
|---|---|
| romaja | the romanised layout (`gksrmfd` → 한글), derived from libhangul's tables |
| 신세벌식 | the flagship layout, from [pat.im/1136](https://pat.im/1136)'s 날개셋 `ist.xml` |
| 옛한글 | `yethangul`; archaic forms, converted from `yethangul.xml` |
| 날개셋 | the source compilation behind the P2 layout; its predicates D/E/F were lifted verbatim into `h3` expressions |
| hanja conversion | 표기 → 한자 lookup; `hunmin_suggest_*` |

## External sources and tools

Full names, per the maintainer's instruction to use no invented abbreviations
(2026-10-04). The repository has been inconsistent on this; these are the forms
to use.

| write | not | what it is |
|---|---|---|
| **국립국어원** (National Institute of Korean Language) | ~~NIKL~~, ~~NIKE~~ | the government language authority. **It has no standard short form.** Both `NIKE` and `NIKL` circulate in this repo and neither is correct; the upstream dump path happens to be `spellcheck-ko/korean-dict-nikl`, which is where `NIKL` came from. |
| NIKE Korea | — | the retail label used on commercial dictionaries. **A different thing**, and not the institute. |
| libhangul | — | the upstream Korean input library (BSD-3), the engine's parity oracle. **Frozen** (issue #31): never patch the system copy. |
| kime | — | the Rust IME, read for semantics only (GPL-3). |
| AIHub | — | the Korean AI data portal. Datasets **624** (웹데이터) and **119** (국가기록물) are measurement instruments: use-and-training permitted, redistribution not. |

## Terms this project uses that are NOT Korean grammar

| term | meaning |
|---|---|
| **eojeol buffer** | does not exist yet. The engine has no word-level buffer: `hunmin_ic_preedit` returns the composition buffer, which is syllable-level. Do not write as if it does. |
| **tier** | a verification level, A–G. See `docs/TESTING.md`. Not a priority. |
| **register** | a text variety: encyclopedia prose, web text, official documents. The three corpora are three registers and a figure measured on one does not transfer to another. |
| **accept** (an ABI symbol) | to widen `libhunmin/tests/abi/symbols.txt`. Deliberate, never automatic. |

## Words this project deliberately does not use

| avoid | use | why |
|---|---|---|
| "word" for 어절 | **eojeol** | the distinction IS the problem (see above) |
| "끝이다"/"끝" as a standalone noun | **eomi (어미)** | 끝이다 is a verb; the noun needs the Hangul |
| "suffix" / "ending" interchangeably | **josa** / **eomi** | different categories; a josa attaches to a noun, an eomi to a verb stem |
| ~~NIKL~~ / ~~NIKE~~ for 국립국어원 | **국립국어원** | no such abbreviation |
| "stakeholder" / "synergy" | — | general, but this repo has been bitten by jargon once already (the glossary itself) |