# SPEC-ANDROID-KEYPAD — Android soft-keypad requirements

Requirements for the Android virtual keypad (0.4 Track 3, issue #285 / #298).
These bind the Android frontend's keypad; the common frontend requirements are
in `docs/SPEC-FRONTEND.md` and are cited, not restated. The composition engine
and layout-data contract are unchanged by this SPEC — it is about how the keypad
presents keys, not how the engine composes.

**Status:** the criteria below were stated by the maintainer on 2026-11-22 and
are recorded verbatim as requirements. The **mechanism is open** (issue #298):
no layout, geometry or engine change is ruled here, and none is implemented yet.
The design analysis that prices the options is
`docs/ANDROID-SYMBOL-INTERACTION.md`; the prior-art survey is
`docs/ANDROID-KOREAN-IME-SURVEY.md`.

# Requirements

### KP1. 104-key symbols at minimal touch and physical positions; number-row symbols paired with digits

The maintainer's words (2026-11-22, verbatim):

> "중요한 기준을 세워보자면, 표준 104키 키보드로 입력할 수 있는 기호들은 최소한의
> 터치로 입력할 수 있고, 물리 키보드의 위치와 유사하게 제공한다. 그 외의 자주
> 사용되는 특수 문자들은 조금 더 입력이 힘들어도 상관 없다. 특히 숫자열에 있는
> 기호들은 숫자 키와 연결해서 제공하는 게 좋다. 이건 암호 입력 시에 숫자와
> 기호를 연결해서 사용하는 경우가 많아서 그래."

Observable outcome, in three parts:

- **Tier 1 — the symbols a standard 104-key keyboard can type** are reachable in
  the minimum number of touches, at positions similar to the physical keyboard.
- **Tier 2 — other frequently-used special characters** (those a 104-key cannot
  type) may cost more touches; that is acceptable.
- **The number-row symbols** (the shifted digits) are provided **linked to the
  number keys**, because password entry commonly interleaves digits and their
  symbols.

The mechanism that satisfies this — a QWERTY-positioned symbol page, a third
per-key row, long-press pairing on the number row, or another arrangement — is
the open choice of issue #298. *Implementation:* none. *Refuted by:* none yet.

### KP2. Functional keys separated, compact and non-physical; one shift

The maintainer's words (2026-11-22, verbatim):

> "스페이스, 엔터, 백스페이스는 실수로 입력하는 것을 피하기 위해 서로 가까이
> 붙어있어서는 안되긴 하나, 물리 키보드처럼 큰 면적을 차지할 필요도 없고, 물리
> 키보드의 위치에 있을 필요도 없음. 시프트도 마찬가지로 2벌 있을 필요 없음."

Observable outcome:

- **Space, Enter and Backspace are not placed close together**, so a mis-tap
  does not turn a space into a deletion or a premature commit (user error
  protection).
- They **need not occupy the large area** a physical keyboard gives them, and
  **need not sit at physical-keyboard positions** — the physical-position rule
  of KP1 applies to character/symbol keys, not to these functional keys.
- **Shift needs only one instance**, not the two a physical keyboard carries.

The placement that satisfies this is a geometry/presentation decision (Track 3),
not an engine change: the engine already treats space/enter/backspace as named
keys and shift as a per-event modifier (`docs/SPEC-FRONTEND.md#§FE6`).
*Implementation:* none. *Refuted by:* none yet.

### KP3. Frequency-based placement and visual emphasis — symbols and the main keypad (endorsed directions)

The maintainer's words (2026-11-22, verbatim):

> "기호들의 사용 빈도를 기반으로 배치하는 건 좋은 방향이라고 생각해. 다른
> 방법으로는 사용 빈도가 높은 기호를 강조해서 보여주는 - 색상이나, bold 등의
> 방법으로 - 것도 좋은 방향일 것이라고 생각해."

Two endorsed directions for cutting the visual-search time of KP1, stated as
directions rather than a fixed mechanism:

- **Frequency-based placement** — arrange symbols by how often they are used, so
  the frequent ones are reached and found first.
- **Visual emphasis** — highlight the frequent symbols (colour, bold, …) so they
  are found faster *without* moving them.

The maintainer extended the emphasis direction to the whole keypad (2026-11-22,
verbatim):

> "다른 면으로는 한글/영어 입력 시에도 숫자, 편집키 등은 연하게, 문자는 진하게
> 표시하는 것도 같은 이유에서 하는 일이라고 생각하면 돼."

So the principle is a **visual hierarchy across the keypad**, not only symbols:
in Hangul/English text mode the character keys (jamo/letters) are the primary
targets and are shown prominently (진하게), while numbers and editing keys are
secondary and shown faintly (연하게). The goal is the same — guide the eye to the
current mode's primary targets and cut visual-search time. It is a
presentation/theme decision, with no engine change.

These are complementary, and they interact with KP1: frequency *repositioning*
can pull away from the physical-position familiarity KP1 asks for, whereas
*emphasis* keeps the positions and adds salience, so it improves discoverability
without spending screen space or shrinking targets. Both need a symbol-frequency
source (unmeasured here; the bulk corpora live outside the repository). Emphasis
must not rely on colour alone (accessibility, `docs/SPEC-FRONTEND.md` usability;
ISO 25010 user error protection / appropriateness recognizability). The choice and
combination are open. *Implementation:* none. *Refuted by:* none yet.
