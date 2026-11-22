# DESIGN-ANDROID-SYMBOL-VARIANTS — swipe/long-press variants on symbol keys

Status: **design analysis and a provisional data artefact for issue #312, not a
ruling.** The variant mapping and the per-key gesture set are the maintainer's
choice; this document presents them as comparable and implements one candidate
so the data contract can be checked. Requirement pointers:
`docs/SPEC-ANDROID-KEYPAD.md#§KP10` (the direction, stated 2026-11-22),
`docs/SPEC-ANDROID-KEYPAD.md#§KP11` (the data home, DECIDED),
`docs/SPEC-ANDROID-KEYPAD.md#§KP7` (swipe-up as the alternate),
`docs/SPEC-ANDROID-KEYPAD.md#§KP8` (the learnability cap),
`docs/SPEC-ANDROID-KEYPAD.md#§KP5` (accidental-swipe defences); parents
`docs/SPEC-OVERVIEW.md#§A1`, `docs/SPEC-OVERVIEW.md#§A2`.

Already settled, cited not reopened:

- **Data home** — the variant mapping lives in the same app-read file as the
  symbol screen, `frontends/android/app/src/main/assets/keypad/symbols.json`;
  the engine is uninvolved (KP11, #298 ruling; implementation #297). No engine,
  ABI or composition-schema change is made or proposed here.
- **Direction** — gesture variants on Tier 1 symbol keys are preferred over a
  Tier 2 button/page (KP10). The Tier 2 page records in KP6/KP9 stand as the
  history of the considered alternative.

## 1. Scope and frequency grounding

The issue names the Tier 2 set this extension must make reachable:
`· 《 》 〈 〉 「 」 『 』 ※ ₩ …` — twelve characters a 104-key cannot type.
The larger measured-plus-curated candidate set
(`docs/ANDROID-SYMBOL-INTERACTION.md` §Korean special symbols (Tier 2)) is out
of scope here; §7 says what that costs.

The frequency grounding is the measured kowiki reading in
`docs/ANDROID-SYMBOL-INTERACTION.md` §Korean-frequent special symbols — measured
(`tools/symbol-frequency.py`, 3000 ns0 pages, dump sha256 `de25cba4…`). It was
re-run for this issue on 2026-11-23 and reproduced the recorded counts exactly
(artefact + `meta.json` written beside the corpus, outside the repository):

```sh
python3 tools/symbol-frequency.py --wiki-dump <kowiki.xml> --pages 3000 --out <dir>
```

(needs `mwparserfromhell`, e.g. the wikicorpus venv). Two findings from the
re-run that the mapping below uses:

- **《》 outranks 〈〉 by ~7.6×** in the measured register, and 「」 outranks
  『』 modestly; `·` outranks `…` by ~69×. Within every shape family the
  measured order is the same *kind* of choice (which member gets the
  one-gesture slot), so one ordering rule can cover all keys.
- **※ and ₩ are present but tiny in encyclopedia prose** (single-digit and
  sub-unit per-million respectively). They are named by the issue, so they are
  in scope; their placement is justified by the **use case** (notes, chat,
  prices), not by this register. Register caveat, stated: this corpus is
  encyclopedia prose, not chat/notes/passwords; a typed-message corpus would
  re-rank ※, ₩ and the curly quotes, and is the measurement that would change
  the ordering below. The first-3000-pages sample and the residual-markup
  caveat are recorded with the original reading and apply unchanged.

## 2. Mapping-rule candidates

Which Tier 1 key carries which variant? Three rules, comparable:

| rule | mechanism | what it preserves | what it costs |
|---|---|---|---|
| **M1 shape family** | the variant sits on the Tier 1 key it resembles: `·` on `.`, `《〈` on `<`, `「『` on `[`, `※` on `*`, `₩` on `$` | recognition over recall (KP8): the host key *looks like* the variant, so the mapping is read, not memorised; orientation preserved (openers on opener keys, closers on closer keys) | families with two members need a second gesture or direction (§3, §4); `※→*` and `₩→$` are family-by-convention (reference mark, currency), weaker than the bracket/dot resemblances |
| **M2 frequency across keys** | highest-frequency variants on the most reachable keys (bottom row, near the thumbs), independent of shape | reach for the frequent few (KP3's frequency direction) | breaks recognition: `·` on `,` or `《` on `;` is a per-key fact to memorise — the KP8 failure mode; also fights KP9's positional consistency |
| **M3 physical position** | place variants where a physical keyboard would | touch-typist muscle memory (KP1) | **degenerate for Tier 2**: these characters have no physical key on a 104-key board — that is what makes them Tier 2. The rule has nothing to bind to |

M3 is recorded to show why KP1's physical-position clause does not reach this
problem: KP1 governs Tier 1, and `docs/ANDROID-SYMBOL-INTERACTION.md` already
notes repositioning is free within Tier 2. The live comparison is M1 vs M2,
and the deciding axis is KP8: M2 optimises reach at the price of a memorised
table; M1 makes the mapping visible in the key's own shape.

**Recommendation (analysis, maintainer chooses): M1**, because a shape-family
host is recognisable without memorisation, which is the binding KP8 constraint.
*What would change it:* a measured visual-search or entry-time study showing
the frequency gain of M2 outweighs the learning cost, or a maintainer ruling
that reach dominates.

## 3. Ordering within a family: shape fidelity vs frequency

Two families have more than one member competing for the single-gesture
(swipe-up) slot:

| host | members | shape-fidelity order | frequency order (measured) |
|---|---|---|---|
| `<` / `>` | `〈〉` (single angle) vs `《》` (double angle) | `〈〉` up — the single-line form matches the ASCII single angle bracket | `《》` up — ~7.6× the measured rate |
| `[` / `]` | `「」` (single corner) vs `『』` (double corner) | `「」` up — single matches single | `「」` up — same answer, modest margin |
| `.` | `·` (middle dot) vs `…` (ellipsis) | `·` up — one dot | `·` up — same answer, ~69× margin |

Only the angle family actually forks. Both orders are defensible: shape
fidelity makes the up-gesture rule "the Korean form of this key's shape";
frequency makes it "the most-used member of this key's family". The latter is
one rule that covers every key uniformly (including `.` and `[`), which is the
KP8-consistent choice; the former needs a per-family tie-break story.

**Recommendation: frequency order within the shape family** — swipe-up delivers
the most frequent member, the popup lists the family in the same order.
*What would change it:* a maintainer preference for shape fidelity, or a
chat-register measurement that re-ranks the angle brackets.

## 4. Gesture-set candidates, bounded by KP8

The constraint is `docs/SPEC-ANDROID-KEYPAD.md#§KP8`: recognition over recall,
consistent cross-key mapping, a small direction cap. Four sets, comparable —
all of them reuse gestures already in the KP5/KP7 vocabulary (tap, swipe-up,
long-press); none invents a new one:

| set | gestures per key | covers the 12? | hints needed | KP8 reading |
|---|---|---|---|---|
| **G1 swipe-up only** | 1 direction, 1 variant | **no** — `.` needs `·` and `…`, `<` needs `《` and `〈`; the second members must scatter to shape-weak hosts or drop out of scope | one corner hint per host | simplest possible, but cannot hold a two-member family on its shape host |
| **G2 up + long-press popup** | 1 direction (up = first member), long-press = the family list | yes | one corner hint (the up member); the popup itself is the hint for the rest | up = alternate stays the *same rule as everywhere else* (KP7); the popup is read, not recalled; family size capped (§6) |
| **G3 up/down** | 2 directions (up = first member, down = second) | yes | two corner hints on a ~30–33 dp key | adds a direction whose meaning ("second member") is consistent but abstract; two hints on the densest rows fight the target-size finding in `docs/ANDROID-SYMBOL-INTERACTION.md` §Target size on a narrow screen |
| **G4 long-press only** | popup, no swipe | yes | none, or one faint hint | fewest gestures to learn, worst discoverability (hidden until held) and a hold latency per symbol; leaves KP7's swipe-up unused on this page |

Interaction cost per variant (mechanism-derived, not device-timed — the same
caveat as `docs/ANDROID-SYMBOL-INTERACTION.md` §Interaction-count comparison):
first member = 1 gesture under G1/G2/G3, 1 hold + 1 pick under G4; second
member = 1 hold + 1 pick under G2, 1 gesture under G3. G2 and G3 trade exactly
one direction against one hold-and-pick for the second members; the measured
frequency margins (§3) say the second members are the rare ones, so the
hold-and-pick tax falls on the rare path.

**Recommendation: G2** — swipe-up for the family's first member, long-press
popup for the whole family — because it adds no direction beyond the up =
alternate rule KP7 already states, and the popup converts the only memorisation
risk (which else lives here) into reading. *What would change it:* a measured
hold-latency cost that dominates second-member entry (favours G3), or a
maintainer ruling that even one popup is too much machinery (favours G1 with a
reduced inventory).

## 5. Accidental-swipe defences on symbol keys (KP5)

The layered defences measured in
`docs/ANDROID-SYMBOL-INTERACTION.md` §Avoiding the accidental swipe apply
unchanged to symbol keys: distance/direction/velocity gate,
return-to-cancel, commit-on-release with preview during the gesture, adjustable
sensitivity. They are cited, not restated. Two properties of the symbol page
make the residual risk cheaper here than on the text keypads:

- **No composition, no glide.** A mis-swipe on this page commits one wrong
  character; there is no preedit to corrupt and no competing gesture meaning.
  Backspace sits on the nav row (KP6), so recovery is one tap.
- **The popup is the preview.** Under G2 the long-press popup shows the family
  before release, and the swipe-up corner hint shows its target before the
  gesture — the display and the defences reinforce each other, as recorded for
  the number row.

The thresholds, timing and hint rendering are gesture-UI wiring (§7), not
settled here.

## 6. Data schema and the contract

Two shapes for the mapping in the app-read file:

- **S1 — a separate top-level `variants` object**, keyed by Tier 1 character;
  the ruled `rows` are untouched. The checker validates the new section; the
  Kotlin parser is extended in the same change so nothing accepts data the app
  silently ignores (the doctor/loader divergence recorded in
  `docs/DESIGN-ANDROID-SYMBOLS.md` §1.2 is the failure mode this avoids).
- **S2 — per-key objects inside `rows`** (`{"char": ".", "up": "·", …}`).
  Rewrites the row schema, the KP6 ruled-arrangement comparison, the Kotlin
  parser and every existing test for no expressive gain.

S1 is implemented. `schemaVersion` stays 1: the section is additive and both
owners of the contract (checker, parser) learn it in the same commit; there is
no external consumer of this asset that could silently ignore it.

The implemented shape, with the structural rules the checker enforces:

```json
"variants": {
  ".": { "up": "·", "popup": ["·", "…"] }
}
```

- every host key is a Tier 1 symbol of the ruled rows;
- `up` is `popup[0]` — one order, frequency order, no second ranking to drift;
- every variant is a single non-ASCII Unicode `P*`/`S*` character (Tier 2 by
  definition: a 104-key cannot type it);
- a variant character lives under exactly one host (one home per character);
- **popup cap: 4 members** — a KP8-derived bound, proposed not ruled: a popup
  is read at a glance, and four is the largest list that stays a glance rather
  than a search; the checker enforces it so growth is a contract change made
  deliberately;
- coverage: the twelve characters the issue names must all be reachable.

The checker validates **structure and scope, not the assignment**: which key
carries which variant is provisional (§8) and unruled, so encoding the
assignment as checker expectations would write a verdict into the contract.
The KP6 rows remain pinned to the ruling exactly as before.

## 7. What this does not settle

- **Gesture-UI wiring** — the touch state machine, thresholds, corner-hint and
  popup rendering, accessibility labels, landscape geometry. The data and its
  contract exist; the app does not yet act on `variants` beyond parsing them.
  That wiring is the soft-keyboard UI lane (#299's successors), and KP5's
  defences are requirements on it.
- **The inventory beyond the twelve.** The measured-plus-curated candidate set
  (curly quotes, dashes, shapes, arrows, math/units, more currency) is recorded
  in `docs/ANDROID-SYMBOL-INTERACTION.md`; under M1 most of it has obvious
  hosts (`‘’` on `'`, `“”` on `"`, `–—` on `-`, `×` on `*`, `÷` on `/`, `±` on
  `+`, `°` on `%` or `*`). Extending the inventory is a scope decision for the
  maintainer; the schema and checker already carry it without change.
- **Register** — a chat/notes corpus measurement for ※, ₩ and the ordering.
- **Device-measured** latency, search time and error rates for G2 vs G3.
- **The maintainer's choice** of mapping rule, ordering, gesture set and cap.

## 8. The provisional mapping (implemented, awaiting ruling)

The data file now carries the M1 + frequency-order + G2 candidate, so the
contract is checkable end to end. It is a **proposal artefact**: the maintainer
rules the mapping by accepting or changing it; nothing here records a verdict.
Frequency pointers are to the measured reading (§1); *(use case)* marks the
two placements the prose register does not justify.

| host | swipe-up | long-press popup | family rationale |
|---|---|---|---|
| `.` | `·` | `· …` | dot family; `·` ~69× `…` |
| `<` | `《` | `《 〈` | angle family; `《` ~7.6× `〈` (§3 fork) |
| `>` | `》` | `》 〉` | angle family, closer on closer |
| `[` | `「` | `「 『` | corner family; single outranks double |
| `]` | `」` | `」 』` | corner family, closer on closer |
| `*` | `※` | `※` | reference mark *(use case)* |
| `$` | `₩` | `₩` | currency *(use case)* |

Seven hosts, twelve variants, one direction, popup ≤ 2 everywhere (cap 4).

## 9. Verification

```sh
python3 tools/symbol-data-check.py            # ctest keypad-symbol-data (tier-B)
python3 tools/doctor.py libhunmin/data        # engine data untouched, stays clean
ctest --test-dir build -j16 -LE slow          # inner loop
ctest --test-dir build -R engine-android-e1   # SymbolScreen parse/commit lane
python3 tools/docs-lint.py --build-dir build
python3 tools/req-map-check.py
```

The checker's negative fixtures run inside the same invocation (a built-in
selfcheck, in the spirit of `tools/ist2data.py`), so the contract's failure
paths are asserted without a new test registration.