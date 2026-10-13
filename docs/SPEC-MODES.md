# SPEC-MODES — Hangul/latin automatic determination (hangul/latin mode guessing)

Status: **Requirements settled. Not implemented. Corpus not settled.**

**2026-10-04:** What R8's `english_valid`/`hangul_valid` must look up has been
measured (`docs/MODES-DATA.md` §4-7). **Neither is a dictionary lookup.** English is
a **corpus list with a cap of 50,000**, Hangul has **no cap** (the lookup has to
come back from the dictionary, or be a composition decision). Defining them as
dictionaries drops English reproduction rate to half.

This document contains **requirements only**. Measurement results are in
`docs/MODES-METRICS.md`, data and licences in `docs/MODES-DATA.md`, engine rules
and the state machine in `docs/MODES-ENGINE-RULES.md`, and the failures of the
investigation in `docs/MODES-POSTMORTEM.md`.

## How to read this document — what is settled and what can change

The body is split into two parts. This split is not decoration, it is a **document
editing rule**.

- **§1 Settled requirements (DECIDED)** — what the user explicitly settled. Items
  here cannot be changed by measurement results. To change one you have to edit
  this document by hand.
- **§2 Changeable after evaluation (EVALUABLE)** — the requirement is settled but
  **the implementation parameter or the display method can change depending on
  evaluation.** Each item states "what is a measurement", and when the result
  comes in you rewrite the status.

The status markers at the end of each item are as follows. The intended use of
this document is that the user changes these values by hand.

    [D] = settled (DECIDED)         — kept regardless of evaluation
    [E] = evaluable (EVALUABLE)     — can be replaced by a result from §2.Metrics

## Reading order

**The R number is the reading order.** This document assumes a person reads it
from the front, and reading in order makes each item the result of the ones before
it. The first edition numbered in discovery order, which was a reversed order in
that R3 (when) came before R4 (how) — it was renumbered on 2026-10-04, and the
cross references in the other four documents were fixed at the same time.

| Stage | Question | Items |
|---|---|---|
| **1. What it solves** | why this feature exists | R1 |
| **2. What it converts** | what a mode is, how many IMs there are, where it is decided | R2, R3 |
| **3. When it is judged** | what the composition buffer (조합 버퍼) is and when it ends | R4, R5, R6 |
| **4. How it is judged** | when to switch and when not to | R7, R8, R9, R10 |
| **5. When it is wrong, and evaluation** | who fixes it, how it is measured, how to turn it on and off | R11, R12, R13 |

**Why this order holds:**

- Stage 2 comes before 3 because you have to know **what is being converted** for
  there to be anything for "when to judge" to apply to. You cannot talk about
  "when" while the mode is undefined.
- Stage 3 comes before 4 because the rules of stage 4 take "the moment the
  composition buffer completes" as a condition. **The buffer has to be defined
  first before you can talk about its completion moment.**
- Stage 5 is last because repair-when-wrong and the scoring criteria are not
  defined without the preceding rules.
- R7-정정 does not reverse the rule of R7, it only changes **the direction of the
  error**. R13-정정 adds one more surface to R13. Both are placed immediately
  after the original item.

**Items with the `-정정` marker** (R7-정정, R13-정정) are ones the user explicitly
stated later and which amend existing rules. Instead of deleting the original
rule, why it changed is left in the same document.

## At a glance — the whole behaviour

```
[Hangul mode]  the user types in dubeolsik/sebeolsik
                    │
      accumulates in the composition buffer (not per syllable, R4)
      digit runs accumulate together too (R6)
                    │
      space or symbol = delimiter
                    │
              ┌─────┴─────┐  judged only at this moment (R4)
              │  judge      │
              └─────┬─────┘
                    │
    ┌───────────────┼───────────────┐
    │               │               │
 commit as English  commit as Hangul  ambiguous → current mode
 + English mode     (stay Hangul)   (R7, R8)
    │               │               │
    └───────────────┴───────────────┘
                    │
        the new mode applies from the next key onward (no retroactive effect, R10)
                    │
        if wrong, a person hits backspace (R11)
                    │
        scoring: keystroke count + number of manual on/off toggles (R12, R7-정정)
```

---

# 1. Settled requirements (DECIDED)

## R1. What this solves [D]
> "한글로 치던 중, 영어로 바꿔야 하는 불편함이 현재 어떤 모드인지 몰라서
> 뭔가를 쳤다가 지우고 다시 치는 비용. 이런 것을 줄일 수 있는 방법으로
> 이 연구를 하고 있는 거야."

The problem being solved is **not typing.** It is not typing speed. It is *not
knowing which mode you are currently in* and therefore the cost of type → see
something odd → select → delete → type again. This feature does not reduce
typing, it **removes not knowing the mode.**

> "나는 개발자라서 코드를 손으로 치면서, 중간에 한글을 작성해야 하는 경우도 꽤 있어. 이 때 잘못된 변환이 발생하면 곤란해."

So what this feature has to be careful about is **not misfires themselves, but
the number of times a person has to intervene and press on/off** (R7-정정).

## R2. One IM [D]
> us layout을 쓸 필요는 없어. us layout을 삭제하고 hunmin만 쓴다고 생각하자구.
> hunmin 내에 한글 모드, 영어 모드가 있고, 한글 모드에서는 두벌식, 세벌식 등을
> 선택해서 쓸 수 있고, 영어 모드에서는 qwerty, dvorak 등을 선택할 수 있게
> 하는 거지.

- **One IM. No separate `us` registration.** Dubeolsik·sebeolsik are *modes*
  inside hunmin.
- Hangul mode picks a Hangul layout, English mode picks a Latin layout.
- The per-item `Layout=` in the profile is kept as the only per-IM override
  (`docs/FCITX5-ADDON-NOTES.md` §11c-ter).

## R3. Mode guessing is an engine rule [D]

> 모드 추측은 엔진 규칙. 안드로이드에서도 윈도우에서도 사용할 수 있어야 함.
> 마찬가지로 자동변환 기능을 켜고 끄는 기능도 모든 환경에서 동일한
> 방법으로 지원해야 한다.

This is the *requirement*: a single judgement rule, owned by **one component**
and delivered identically in fcitx5, Android, and (planned) Windows. Two
frontends must never answer differently on the same word.

**The owner and the mechanism are not requirements — they are a design
decision.** The current implementation is an *adaptor*: `frontends/fcitx5/src/modeswitch.*`
keeps a shadow `hunmin_ctx` on the Latin layout and replays the buffered word
at the judgement boundary. An engine-owned variant is the alternative. Both
satisfy the requirement above. Which one wins is recorded in the R8
architecture decision, not in this document.

The three design facts that came out of the earlier discussion are moved to
`docs/architecture.md` **§3**: no per-frontend settings store, one state
(`hunmin_ic_set_option(autoswitch)`), and no new data schema needed for
mode/guessing.

## R4. Judgement moment — the end of the composition buffer [D]
> "조합이 끝나면 해도 돼. 스페이스바, 기호 등을 사용할 때."

**"Composition" is not a syllable.** This distinction is the core of this
requirement, and misunderstanding it makes the implementation completely
different.

> 조합이 완료되는 지점은 **기호나 공백문자가 나타나기 전일 수 있는데**,
> 이 경우에는 불편함이 더 커지기 때문에, 더 이상 확장해도 개선되지 않는
> 선을 찾아야 해.

Precise definition (settled by the user):

- **Composition buffer** = the whole span **from after** the immediately preceding
  space character or symbol **until** the next space character or symbol appears.
- The point at which the composition buffer is complete is **the moment the next
  delimiter (space or symbol) appears**. Judge and commit at that moment.
- **Judgement is not done per syllable.** All syllables within a word accumulate
  in the buffer.
- **A symbol is also a termination condition.** But that is not because the user
  said so.

### Why a symbol is a termination condition — the user's explanation is wrong, the conclusion is right [E]

The user said this.

> "기호가 나타나도 더 이상 한글, 영어 단어 조합은 만들어지지 않으니까"

**This proposition is not true.** We measured it.

    $ hunmin-sim --layout dubeolsik --keys 'rk,rmf'
    [r] commit=''    preedit='ㄱ'
    [k] commit=''    preedit='가'
    [,] commit='가'  preedit=''
    [r] commit=''    preedit='ㄱ'      ← recomposes after the symbol
    [m] commit=''    preedit='그'
    [f] commit=''    preedit='글'

    $ hunmin-sim --layout sebeolsik-3-2 --keys 'well-known'
    [w][e][l][l] preedit='ㅈ','ㄷ','디','ㅣ'
    [-] commit='ㅣ-'                     ← a hyphen does not break it either
    [k][n][o][w][n] preedit='ㅏ','ㅜ','ㅐ','ㅈ','주'

**A symbol does not end composition.** Hangul (whatever the layout) keeps
composing after a symbol.

**So why is a symbol the right delimiter — the actual basis is this:**

> **It is not that a symbol ends composition; it is that a symbol makes
> composition unnecessary.**

It is true that composition continues after a symbol, but that composition is
**not an extension of the preceding eojeol (어절, space-delimited word) but the
start of a new eojeol.** The judgement target is "the text up to before the
symbol", and after the symbol it is already a different eojeol. So a symbol is in
fact the correct delimiter — the conclusion is right and only the basis differs.

**Why this item is [E] — two things are not yet verified:**

1. ~~**Hangul after a symbol with no space**~~ — **MEASURED 2026-10-04. Rare.**
   Measured below.
2. **Symbols inside digit runs** — `1,000` (thousands separator), `3.14`
   (decimal point), `12:30` (time). R6 says a digit run should be held in the
   buffer when ON, but if a comma breaks the buffer then `1,000` is not a digit
   run but three tokens. **R6 and R4 conflict.**

### Measurement — Hangul after a symbol with no space is rare [E → measured]

Tool: `~/work/wikicorpus/measure-symbol-gap.py`, 4,000 pages of ko.wikipedia.
**Important: this measurement is only valid if you read the page body whole** — a
reader that only reads line by line measures the header instead of the body
(`docs/MODES-DATA.md` §4-1).

The raw data `hangul<symbol>hangul` (no space) has 26,387 cases, but most of them
are **wiki furniture** — `분류:` chains (all 14,722 of the colons), `15px` flags,
infobox spans. Removing the furniture leaves 13,436.

You have to go further from there — among those 13,436, some are not real usage:

| Distinction | Cases | Basis |
|---|---|---|
| Remaining after wiki furniture removal | 13,436 | `분류:` chain removal |
| Of those, **the ones with adjacent whitespace** | **1,984** | something a person actually typed |
| `,` `.` with adjacent whitespace | **132** | used as sentence punctuation |

**Most of the 1,984 are not `·` but a Sino-Korean compound marker (한자어 결합
부호):**
`노·정 두 후보`(노무현·정몽준), `동·서 독일`, `한·미·일 정상`, `전·현직`,
`정·부 Relations`. This is **not two eojeols without spacing but a notation that
joins two Sino-Korean words inside one eojeol with `·`,** and it reads as one
eojeol to a person too.

Those used as sentence punctuation number **132 cases (`,` 62 + `.` 70)**, and
these sit right in the middle of real sentences, so as a delimiter they are
**exactly right** — `했다. 그러나`, `발표한 바 있는데,`.

**Conclusion: using a symbol as a delimiter is right.** The misreading risk of a
form like `가나,나나` does not exist, or is at the level to be ignored (all 132 are
valid delimiters). However, if the Sino-Korean compound marker (`·`) becomes a
delimiter then `노무현·정몽준` is split into two eojeols, so it is split off as
item **E8 (a Sino-Korean compound marker is not a delimiter)**. This is
conversely **evidence that symbols may be used as delimiters**, so it strengthens
R4's conclusion.

### Cross-validation — Wikipedia and the web corpus give the same answer [E → measured]

**Re-measured with AIHub 624 (web text).** Wikipedia is encyclopedic prose, so it
may under-represent ordinary sentence punctuation usage, which is why **if this
corpus gave a different answer you would have to doubt the Wikipedia answer.**

The two corpora differ in size by 12x, so the comparison is **by ratio, not by
count**:

| Corpus | hangul runs | all attached symbols | of those `·` | real sentence punctuation | ratio (overall) | ratio (sentence punctuation) |
|---|---|---|---|---|---|---|
| ko.wikipedia 4,000 pages | 4,132,658 | 23,861 | 7,869 | 16,005 | **0.577%** | **0.387%** |
| AIHub 624 2,000 records | 50,119,004 | 261,600 | 125,974 | 135,694 | **0.522%** | **0.271%** |

**The two corpora show nearly the same ratio** (0.577% vs 0.522%, sentence
punctuation only 0.387% vs 0.271%). In other words **"Hangul attached to a symbol
with no space" is not a phenomenon produced by encyclopedic prose, it appears at
the same magnitude on the web** — the Wikipedia sample did not distort ordinary
writing.

And the top forms in 624 show the same composition as well:

```
광주·전남  연구·개발  초·중  산·학  한·중  국내·외  민·관    ← `·` = Sino-Korean compound
말했다.이어  밝혔다.이번  말했다.한편  않았다.. .            ← `.` = sentence end
```

**624's sentence-punctuation ratio is lower than Wikipedia's** (0.271% vs 0.387%) —
news articles type a full stop separately more often, while wiki sentences attach
it because it is syntactically optional. Neither side reaches 1%.

**This is the basis for raising R4's [E] to [D].** A corpus of a different nature
gave the same answer.

**What 624 additionally gives:** a notation compound like `신·구 정권` occurs
125,974 times in 2,000 records. Wikipedia's `·` is 7,869/4,132,658 = 0.19% while
624 is 125,974/50,119,004 = 0.25%, so **Sino-Korean compounding is more common in
news.** So if the E8 (`·` exception) had been measured only on Wikipedia it would
have been underestimated.

### Measurement method

`tools/modes-glued.py` — counts the two corpora with the same statistic and
separates out `·`. 624 **streams the original zip** (3.9 GB, no decompression).

```sh
tools/modes-glued.py --both 2000      # 2,000 records against 4,000 pages
```

Because the two corpora differ in size, **a ratio against hangul runs is used
rather than a count.** The tool judges the ratio difference itself as
`agree`/`DISAGREE` (outside 0.5x–2x it is DISAGREE).

**The schema was verified (not assumed — POSTMORTEM 10):**

```
{"SJML": {"header": {...}, "text": [ 99 SEGMENT DICTS ]}}
  each element is not a string but a {"title","subtitle","content"} dict
```

Iterating `text` as a list of strings gives **0 characters, rc=0** — a silent zero.
Three things are wrong at once: key-name guessing (`obj.get("data", [])`),
discarding short records (`len(obj) > 40`), and **assuming a dict inside a list
is a string.** The extractor always prints the skip count and the reason.

**Only the remaining item (2) has to be measured:** symbols in digit notation
(`1,000` `3.14` `12:30`) — are they buffer boundaries or interior characters?
Part of it can be decided without a corpus — a Latin layout has no composition in
the first place (Latin key → Latin letter), so **digit notation in English mode
is not a judgement target of this feature.** Only typing digits in Hangul mode
(R13) remains.

**Provisional rule:** use a symbol as a delimiter. But if (1) comes out as
"exists but is rare" — that is, if the misread rate stays low when it is left as
a delimiter — keep it as it is. If it comes out as "exists and is common" then
**even among symbols you have to distinguish sentence punctuation (`. , ? ! :`)
from interior-word symbols (`-`, `/`, thousands separator `,`).** That is a
change to a judgement rule and not to data, so it is decided here rather than in
R4.

**This requires changing an engine invariant.** The current engine commits a
completed syllable immediately (`libhunmin/src/ic.c:171,184,192,200` — calls to
`save_commit()`). Measurement evidence:

    $ hunmin-sim --layout dubeolsik --keys 'world' --trace
    {"in":"w"} commit=''    preedit='ㅈ'
    {"in":"o"} commit=''    preedit='재'
    {"in":"r"} commit=''    preedit='잭'
    {"in":"l"} commit='재'  preedit='기'     ← already committed at the 4th key
    {"in":"d"} commit=''    preedit='깅'

The typo `재` in `world` is committed **before** the space arrives. As the
requirement says, the whole of `world` should accumulate in the buffer and be
judged once at the end. `preedit` likewise has to hold the whole word — the
current `save_preedit()` (`ic.c:127-128`) renders **only one**
`jaso_to_string(cho, jung, jong)`, so it is a single-syllable preedit, and this
has to change too.

The load and how to measure it are put in `docs/MODES-ENGINE-RULES.md` §4.

## R5. The composition buffer in the OFF state [D]
> 자동변환 기능이 꺼져있을 때, 영어 모드에서는 글자 단위로 commit 되어야 해.
> 여러 소프트웨어에서 공백문자 없이 영숫자 입력을 처리하는 사례가 있어서,
> 이런 사례에 사용할 방법이 필요해.

**This requirement applies only when automatic conversion is off (OFF).** The
requirement for the on case is specified separately in R4.

- **OFF + English mode:** Latin letters and digits are **committed per
  character.** There is no composition buffer. It is needed because of software
  that handles alphanumeric input without spaces (`abc123`).
- **OFF + Hangul mode:** preserves current behaviour 100% (§4).

Per the correction the user gave immediately before ("the case I mentioned earlier
was only talking about the behaviour when automatic conversion is off"), this
item is not promoted to an ON-side constraint. **Digit runs in ON are a separate
requirement** and are in R6 below.

## R6. In the ON state a digit run is part of the composition buffer [D]
> 자동변환 기능이 있을 때는 버퍼가 있어야 해. 세벌식에서는 숫자열까지
> 사용하기 때문에 숫자 입력이라고 곧장 commit 하면 안돼.

- **ON:** the composition buffer does not only hold Hangul eojeols, it holds
  **digit runs too.** Committing digits directly breaks digit-run composition in
  sebeolsik.
- This requirement is backed by measurement — the digit keys of `sin3-p2` are
  synthesised as Hangul jaso and resolve to digits/symbols at the terminal tiers
  101/102/103 (`1` becomes `Ⅰ` at tier 101, `₁` at 102, `¹` at 103). In other
  words a digit run **is already part of composition today.**

**The combination of the two requirements is the exact definition of on/off.**

```
autoconversion ON  → keep the composition buffer (Hangul eojeol + digit run), judge once at the delimiter (R4, R6)
autoconversion OFF → no composition buffer. Hangul mode is current behaviour, English mode is per character (R5)
```

For this combination to hold, **English mode has to be a Latin layout rather than
a Hangul layout** — since a Latin key produces a Latin letter there is nothing to
compose. This is the same statement as R2's "English mode picks qwerty and the
like". Therefore there is no need to build a composition buffer in English mode,
and no dictionary lookup for one either.

---

## R7. When judgement is ambiguous, depend on the current mode [D]
**Core rule: if it can be judged as English or as Hangul, depend on the current
mode. Only switch when it is clearly the other language.**

While stating this requirement **the user gave an example in only one
direction.** Generalizing that example into an asymmetric rule was over-
interpretation, and it is replaced by R7-정정 below.

> "문제는 내가 명확히 한글로 쳤는데, 그게 영어로 바뀌어 버리는 경우이지.
> 물론 오타를 내서 영어모드로 바뀌는 것도 조금은 괴로워."

The direction the user gave as an example is this one, but **this does not define
the asymmetry of the rule.** The rule is the first paragraph above, and the rest
is commentary on it.

## R7-정정. Errors occur in both directions [D]
| Situation | Judgement |
|---|---|
| Can be judged as English or as Hangul | **Keep the current mode.** Do not convert to either side. |
| Clearly the other language | **Convert.** |
| Misjudged in only one direction (including typos) | Can occur and is treated identically regardless of direction. |

**Errors in both directions carry the same weight.** There is no rule under which
one side is more permitted than the other.

Why it changed, and what it requires:

- The original R7 wrote that only "Hangul → English" was dangerous, which
  **contradicted the symmetric rule in R8.** There was a reason for the second
  line (the direction from English mode over to Hangul) to disappear. **The
  symmetric rule is alive.**
- The user's original words contain that direction too: "영어 모드에서도 저건
  제대로 된 한글인지 모르겠다면 영어로 유지하더라도, 그 정도는 사용자가 감수할 수
  있는 동작이야." That is **describing the permitted direction, not defining an
  asymmetry.**
- **Rather than fixing the direction of the error, the correct direction of
  improvement is reducing the number of manual on/off toggles:**

> 사용자가 의도적으로 어색한 단어를 입력할 수 있으니, 이 기능을 수동으로
> 끄고 켜는 일을 줄일 수 있다면, 그게 더 좋은 방향이야.

**So the evaluation criterion of this feature is not the error rate but the
decrease in the number of times a person intervenes and presses on/off.** In this
direction E3 (short-word threshold) is redefined as well: the threshold is set on
the basis of **"reduces manual intervention"**, not "permits errors". It is not
decided by which direction of error is more tolerable.

## R8. Judgement rule — only when clear [D]
> "명백히 영어 단어이면 영어로 바꾸고, 명백히 한글 단어이면 한글로
> 바꾸는 거지. 모호하면 현재 모드로 출력."

> "한글도 영어도 모두 나오거나, 모두 되지 않는 경우에는 현재 모드에 따라
> 출력을 할거야."

> "한글 모드에서 영어 단어가 명확히 구성되는 경우가 아니면 한글 모드를
> 유지할 거기 때문에, 네가 이야기한 안녕ㅎ 같은 어절은 정상적으로
> 유지될 거야."

**If both are valid, or if both are invalid, keep the current mode.** Only switch
when they disagree. `안녕ㅎ` is neither a Hangul word nor an English word, so
keeping Hangul mode is **the correct answer, not a failure.**

```
moment the composition buffer completes (space or symbol):

    english_valid AND NOT hangul_valid   ->  commit as English and switch to English mode
    hangul_valid AND NOT english_valid   ->  commit as Hangul and stay in Hangul mode
    otherwise (both valid / both invalid)  ->  commit in the current mode
```

Whether the second line, the symmetric rule, actually holds is left to E1.

### `valid` is not a dictionary lookup (measured 2026-10-04)

**What `english_valid` / `hangul_valid` in the judgement formula above must look
up has been measured** (`docs/MODES-DATA.md` §4-7). The requirement (R8) is
unchanged, but its definition is not a dictionary:

- **english_valid** = a lookup against a **corpus list** with a cap of
  **50,000**. Reproduction rate 86.35% (on 624). Even raising the cap to 815,822
  (all of it) only gives 91.96% — it saturates.
- **hangul_valid** = **no cap.** A cap must not be applied (§4-7b).

**"Clearly an English word" must not be defined as an English dictionary.**
Measured: at a cap of 50,000 a dictionary filter drops the reproduction rate from
**86.35% → 43.87%**, and raising the cap does not recover it (flat at ~44%). The
reason is that `kbs` `mbc` `fc` `sbs` `fifa` `uefa` `sk` `kt` `nhk` `cj` are
**absent from English dictionaries** while actually being among the most
frequently typed tokens. **This list is not a "list of English words" but a
"list of Latin strings that Koreans type in Hangul."**

Therefore "word" in the judgement formula above means **"something in this
list",** and the material of that list is not a dictionary but a **corpus
harvested from text typed in Hangul.** Stopword removal is not done because it
only costs reproduction rate (§4-7a).

## R9. Short words are not switched [D]
> "짧은 단어에서 이런 변환이 일어나는 건 위험성이 크고, 단순 오타에 대한
> 방어로도 좋지 않은 것 같아."

Prohibiting automatic switching of words — this is a requirement. The numeric
threshold is §2.E3.

## R10. Switching is not retroactive [D]
Settled by the user. After judgement the new mode applies **from the next key
onward.** Already committed text is not reassembled.

## R11. Accept that a person fixes it [D]
> 그 후에는 backspace로 사람이 지우고 고치는 방법을 받아들일 수 있어.

Do not aim for 100% agreement. When a switch is wrong, the cost of a person
fixing it with backspace is accepted. **So the success metric is not accuracy
but a reduction in keystrokes** (R12).

## R12. Success is measured in keystrokes [D]
> 가장 의미있게 2/3번 지표를 측정하는 방법은, 주어진 문장을 한영 전환없이
> 타이핑 했을 때 동일한 문장이 나오는가? 이고, 이게 100% 성공할 필요는
> 없어. 중간에 한영 전환이 잘못된 것을 발견하고, 사람이 고친다고 했을 때,
> 키 입력을 이 기능이 없을 때보다 많이 줄일 수 있으면 충분해. 완벽한 것은
> 없고, 계속 개선될 수 있으면 충분하다는 말이야.

The metric is **not accuracy. It is keystroke count.** The pass-line number is
§2.E4.

## R13. on/off is a key combination by default, and there must also be a UI [D]
What the user wanted was **key combination first.**

> 자동변환 기능 on/off 방식으로 가장 선호하는 건 UI로 설정하는 것이 아니라,
> 키조합으로 하는 것.

And they stated explicitly that it is **in addition, not exclusively.**

> UI로도 설정할 수 있어야 해. 특히 UI에서는 키조합으로 바꿀 수 있다는
> 정보를 제공해서, 처음 이 IM을 접하는 사람이 기능을 배울 수 있도록
> 도와줘야 해.

## R13-정정. Two surfaces, one state [D]
**The on/off state is one and the engine owns it** (R3). "Do not build a settings
store per frontend" is unchanged. What is added here is a **surface** — one more
path to reach the same state.

```
state: hunmin_ic_set_option(ic, "autoswitch", 0|1)   ← owned by the engine, the single source of truth

surface A (default)  key combination — a top-level key hook that behaves identically in any layout
surface B (added)    settings UI — reads and shows the value, and tells you it can also be changed by key combination
```

What surface B requires is **not value change but visibility + discoverability of
the value:**

- **It has to be learnable** — someone meeting this IM for the first time has to
  be able to find out that an on/off key combination exists. A feature that
  cannot be discovered is a feature that does not exist.
- **It has to be explainable** — the UI has to know which key it is (for example
  "Alt+한/영") so the frontend does not hardcode it. **This information is
  provided by the engine.** The assignment is decided in E5, but if each frontend
  knew it on its own, the three frontends would each advertise a different key.
- **It has to be changeable** — allow the assignment to be changed in the UI
  (surface A; the engine reads it again at judgement time).
- **Whether ON or OFF is the default — DECIDED 2026-10-11: OFF, until release
  0.3.** Maintainer: "B3 는 0.3 전까지 off를 기본으로 하자. 자동변환에 필요한
  사전이든 뭐든 추가 데이터는 별도 배포를 가정하고 그 데이터가 있을때만 기능을 켤
  수 있게 하면 transition이 용이할 거 같아." The feature's additional data ships
  separately and the feature is only enable-able where that data is present.
  Recorded with its reasoning in
  `wiki/decisions/2026-10-11-auto-switch-off-by-default.md`; the data-gate
  mechanism is design work for 0.3, not settled here. (The assignment part of E5
  remains undecided; see below.)

## Does this decision erase the earlier ones

**It does not.** This decision has already settled two things from the earlier
conversation:

1. **No per-frontend settings store is needed** — still true. The state is in the
   engine and the UI is a thin layer that reads and writes it.
2. **`hunmin_ic_set_option()` is the precedent for the new option** — still
   used. The preprocessing-expression override already passes through there
   (`ic.c:610`).

**The undecided parts were moved to E5** (assignment, default value). Below only
records the current factual relationships (not a measurement):

- **Android has no settings screen yet.** There is no `android:settingsActivity`
  in `AndroidManifest.xml`. There is only the input view (layout picker + mic).
  That is, surface B is **new construction** and there is no existing reusable
  resource.
- **Android has no soft keyboard yet either** (AGENTS.md P5). Surface B is an
  IME settings screen, which is unrelated to the soft keyboard, so **it can be
  implemented ahead.** The only thing that cannot be verified end-to-end is
  whether that key is actually pressed via a real key combination — only JVM
  testing is possible, via the path of injecting keys directly through JNI.
- **Windows does not exist yet** (P6). The design of surface B has to be fixed
  in advance so that both frontends use the same structure.
- fcitx5 already has an addon settings (conf), so surface B is not extra UI but
  one existing settings item.

---

# 2. Changeable after evaluation (EVALUABLE)

Each item is **an implementation parameter, not a requirement.** Rewrite the
status marker as `[E]` → `[D]` or "deleted" from the results in
`docs/MODES-METRICS.md`.

## E1. Whether the symmetric rule holds [E]
The second line of R8 (the direction from English mode over to Hangul) **should
be implemented**, but in which word list it holds is unmeasured. If there was a
reason to drop that direction (R7's asymmetric rule), **that reason was corrected
on 2026-10-04, so E1 is not "should we throw it away" but "what will we make it
hold with."**

In the first investigation this symmetric rule did not hold on krdict (it was
replaced by a word list), and at present it is unmeasured which list it holds on.
The record is in `docs/MODES-POSTMORTEM.md` — The National Institute of Korean Language lists `다` and `은` in **both**
the josa (조사, particle) list and the eomi (어미, ending) list, so without an
arbitrary precedence rule the lists alone cannot divide them.

**Why this direction is needed:** because R7-정정 made errors in both directions
carry the same weight, dropping one side leaves **the manual on/off arising on
that side intact.** And the decrease in manual interventions is the evaluation
criterion of this feature (R12, R7-정정). In other words the symmetric rule is
not a convenience but **half of what is being scored.**

**What is a measurement:** when a Hangul eojeol (어절, space-delimited word;
including josa and eomi, and a meaningful unspaced span) is typed in English mode,
does the judgement actually flip over? Record 3 in the current
`docs/MODES-POSTMORTEM.md` is "list-only split is not possible without an
arbitrary precedence rule" (the josa `다`/`은` are in both the josa list and the
eomi list). If the symmetric rule does not hold, **R8 is narrowed to one side
(Hangul→English).**

## E2. Buffer extension cap — how far is extending the buffer worthwhile [E]
> 더 이상 확장해도 개선되지 않는 선을 찾아야 해. 아마 내가 앞에 이야기 한
> 것이 그 지점일거야.

The point the user pointed at is **the preedit length limit.**

> preedit이 너무 길어지면, 대부분의 app이 문제를 일으키기 때문에 주의해야 해.

**As R5/R6 were settled, the character of this item changed.** The ON composition
buffer is no longer a choice but a **requirement** — it has to hold digit runs
too (R6). So E2 no longer asks "should we use a buffer" but only **"where must
the buffer end."**

R4 extends the buffer up to the delimiter, but the cost is that the preedit grows
to the length of a word. So a cap exists:

- The buffer of `hunmin_ic` is `STR_CAP` 64 ucschar (libhangul parity,
  `ic.c:11`). Exceeding it causes **truncation** — it is cut silently. This is
  the effective ceiling on buffer extension.
- Even below that, editors react to the preedit length (varies per app,
  unmeasured).

**What is a measurement:** with the buffer cap set to N, measure separately (a)
the degradation in judgement accuracy, (b) the incidence rate of preedit length
problems, (c) the buffering delay perceived by a person. Find the N at which the
improvement becomes 0. **R4's "extend to the end" can be amended depending on the
result of this evaluation** — once the cap is fixed, "in groups of at most N
characters" is added to R4.

## E3. Short-word threshold [E]
R9 requires "do not switch when it is short", but there is no N. The requirement
does not even settle whether the measure is "in syllables" or "in keystrokes".

**R7-정정 changes the scoring criterion of this item.** The threshold N is set on
the basis of **"the number of times a person intervenes manually and presses
on/off",** not "permits errors". Therefore:

- The error direction (Hangul→English vs English→Hangul) is **not** used as a
  basis for deciding N. The two directions carry the same weight (R7-정정).
- The larger N is, the more automatic switching there is and the fewer manual
  interventions, but misfires increase. Which N to pick is decided by **a
  comparison of the two values**, and since that comparison is the same kind of
  judgement as the pass line (E4), it is not decided without a number.

**What is a measurement:** set N to 2, 3, 4 and measure separately (a) the number
of misfires in each direction, (b) the number of manual on/off toggles. Find the
N at which (b) is minimal.

## E4. Metric pass line [E]
R12 said success is "enough if the number of keystrokes goes down", but **how
much** is enough is not specified. Without a scoring line, scoring is impossible
even when the implementation is finished.

**Metrics ①②③ are the only scoring targets of this spec** (`docs/MODES-METRICS.md`
§1), and each needs a pass-line number. Because of R7-정정, the **number of
manual interventions** (②b) is also a scoring target, so the two numbers needed
now are two. Both are undecided.

## E5. Assignment of the Hangul/English key [E]
> 지금은 한영키를 단순히 한/영 전환을 위해서만 쓰는데, 단일 입력이면 전환,
> 복합 입력이면 특수 키로 만들어도 괜찮지 않을까? 이 기능을 끄고 켜는 데
> 쓸 수 있을 것 같은데.

R13 settled that on/off is a key combination. **What a single input and a
compound input each do is undecided.** Candidates: single = manual Hangul/English
switch, compound = automatic-switch on/off. The fact that F9 is already occupied
by the preprocessing expression, and that Android has no AOSP keycode, is what
constrains this assignment.

**R13-정정 added one more requirement to this item.** The assignment is now shown
verbatim as instructions in the UI (surface B), so **there is not a single
answer.** If the value written on screen as "press this" and the value that
actually works diverge, that is not allowed:

- The assignment must be **a value the engine holds**, and the UI reads it. If
  each frontend knew the assignment on its own, the three frontends would each
  advertise a different key (R13-정정).
- Whether the assignment can be **changed** is undecided. If it can be changed, a
  key remap path is needed, and that is a feature hunmin does not have now.
- **The ON/OFF default is DECIDED 2026-10-11** — OFF by default until release
  0.3, with the feature's additional data shipped separately and the feature
  enable-able only where that data is present (maintainer's ruling, quoted in
  `wiki/decisions/2026-10-11-auto-switch-off-by-default.md`; see R13 above).
  What remains undecided in E5 is the **assignment**, not the default.
- **The 0.1 shipping state is DECIDED 2026-10-12** — the feature is **disabled
  by default in 0.1, by disabling the default key composition** (maintainer's
  ruling, issue #2, quoted in
  `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`): the
  Android `AUTO_TOGGLE` default chord (Right Alt + Enter) is removed, the
  dispatch skips the action, and the settings row shows 미정 (0.3). This rules
  the *shipping state*, not the assignment: **the assignment (single vs
  compound) remains undecided** and stays a 0.3-lane question (`docs/MILESTONES.md`).

**What is a measurement:** the assignment itself is not a measurement target but
a design decision. After confirming the Android/F9 constraint (documented), raise
the status to `[D]`. ~~The default is decided from the result of a usability
evaluation.~~ The default was ruled on 2026-10-11 (OFF until 0.3, data-gated; see
E5's default bullet above and the wiki decision page) — no usability evaluation
was needed for it, and none is owed for it.

## E6. Display method — the flicker (도깨비) [E]
A risk the user pointed at directly.

> 그 와중에 한글 모드이면 일단 한글로 preedit을 제공하다가, 확실히 영어인게
> 확인되면 영어로 바꿔서 보여줘도 되지만, 이건 도깨비 현상을 만들 수 있고,
> 영어라고 생각했지만 다시 한글로 바뀌는 상황도 있을 거야.

Because of R4, if judgement happens only at the end of the buffer (the
delimiter), this flicker **does not occur** — because judgement happens only
once. The flicker only arises when judgement is moved earlier, into the middle of
the buffer (every key).

So this item is not "whether to display in the middle" but the corollary of
**"whether to move judgement earlier"**, and it in turn hits E2 (the cap).

**What is a measurement:** the felt experience in real use (how often it flips
back) when judging exactly once at the end of the buffer. If that value is judged
bad, move on to the option of moving judgement earlier, but then E2's preedit
problem comes back in real time. **The two options are mutually exclusive.**

## E7. How to prove identical behaviour on Android/Windows [E]
R3 was a decision and the implementation direction is fixed too (an engine rule).
What remains is **how to prove it.** With an engine rule a single golden file
makes every frontend follow automatically, but the parts where the frontend holds
state separately (display, key combination handling) each need their own
verification.

**What is a measurement:** (a) prove the rules with a core golden, (b) compare
"same key sequence → same result" at each frontend's tier. Android has no soft
keyboard yet, so stage (b) of E2 cannot currently be verified — it is left in the
document as it is.


## E8. A Sino-Korean compound marker is not a delimiter [E]

An item split off by measurement (SPEC-MODES R4).

```
노무현·정몽준      a notation joining two Sino-Korean words inside one eojeol with ·
동·서 독일
한·미·일 정상
전·현직 대통령
신·구 정권
```

**This is not two eojeols without spacing but one eojeol.** It reads as one to
a person too, so if `·` is taken as a delimiter then `노무현` and `정몽준` are
each split into different eojeols, and if neither is in the dictionary then
**both fail judgement.** It is not two eojeols merely because there is no
whitespace.

**What is a measurement:** compare the number of misreadings when `·` is a
delimiter and when it is not. In 4,000 pages, `·` appearing between Hangul without
a space occurs 10,185 times (raw) / 10,183 times (after furniture removal), and
of those 819 have adjacent whitespace — that is, 819 were typed by a person and
the rest are wiki furniture. On the basis of 4,000 pages × an average of 3,000
eojeols, 819 is under 0.03%, so **it is too rare for the metric to ever show up
as a misreading.** So E8 could be raised to [D], and has been.

**Settled: `·` is treated as a Sino-Korean compound marker and excluded from
delimiters.** The remaining sentence punctuation (`. , ? ! :`) is used as
delimiters.
---

# 3. What is explicitly not done [D]

- **Not implemented:** conversion of QWERTY/DVORAK/COLEMAK. Latin keys are
  already translated by the OS before they reach hunmin. kime makes the same
  judgement (`latin.preferred_direct: true`, measured).
- **Not debated:** the fact that no open-source implementation has this feature
  does not mean the feature is unnecessary. Hancom Office/MS Office ship this
  feature. The absence is a **capability gap**, and the judgement is made only on
  the basis of efficiency.

---

# 4. What this feature must not produce [D]

**The basis of §4 is the user's original words** ("어떤 단어나 어떤 표현을 아예
입력할 수 없다는 건 불량이다"):

> 자동변환 기능을 껐을 때는 입력가능하겠지만, 입력 불가가 0%인 것이
> 이상적이다. 아직은 기능 결함으로 판단하지 않을 생각이며, 입력 불가
> 단어는 실제 사용될 확률이 매우 작아야 함.

That is:

- **When it is off it must return to 100% of the previous behaviour.** This is an
  invariant. It is verifiable and must be verified.
- **When it is on, the incidence of untypable input must be low, and there is no
  pass line yet (E4).** For now this is not judged to be a defect, but it must
  **be measured.**

---

# 5. Other files this document references

| File | Content |
|---|---|
| `docs/MODES-ENGINE-RULES.md` | the engine state machine of R4/R3, the key hook of R13, the load and its measurement |
| `docs/MODES-DATA.md` | word list, corpus, licences (the "valid" criterion of R8) |
| `docs/MODES-METRICS.md` | definitions of metrics ①②③, pass lines, measurement results and invalidation declarations |
| `docs/MODES-POSTMORTEM.md` | the failures committed while investigating this requirement |
| `docs/SPEC-SCHEMA.md` | the data contract of the word list (shares a problem with the P2 typesetting) |
| `docs/FCITX5-ADDON-NOTES.md` §11c-ter | why the per-IM layout selector is wrong |

What is not in this document, and where it is:

| Not here | Where it is |
|---|---|
| EFFECT/COST numbers, filter table, key-inversion rules | MODES-METRICS.md §3 (but the numbers in §3 are declared invalid) |
| Corpus scale numbers, whether distributable | MODES-DATA.md §2 |
| AIHub corpus schema, cross-validation method | MODES-DATA.md §2 (AIHub section) |
| Candidates and constraints for the key combination assignment | MODES-ENGINE-RULES.md §3 |
| The 22 failures committed during the investigation | MODES-POSTMORTEM.md |
