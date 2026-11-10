---
title: 22.04 packaging is a separate name, hunmin never redistributes fcitx5, and development runs in a CI-shaped VM
created: 2026-11-09
updated: 2026-11-09
type: decision
status: active
tags: [decision, linux, packaging, ibus, ci, environment]
sources:
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/MILESTONES.md
  - ../../../.github/workflows/ci.yml
confidence: high
---

# 22.04 packaging is a separate name, hunmin never redistributes fcitx5, and development runs in a CI-shaped VM

**Ruling (2026-11-09, issue #289).** Asked to choose between the priced options for the
Ubuntu 22.04 target, the maintainer answered the numbered list:

> 1-4 까지는 권고안 대로

which makes the recommended reading of four items the decision, and then answered the two
remaining ones in his own words:

> 5. release 시에는 CI에서 하면 되나, 개발 중에는 이 linux box에 vm을 하나 설치해서 개발하자.
> vm을 최대한 github CI 환경과 동일하게 만들어서 CI에서 실패가 발생하지 않게 하자.
>
> 6. fcitx5는 배포하지 않는다.

## What it settles

- **The jammy package gets its own binary package name** (`hunmin-ibus`), and the existing
  `hunmin` package keeps shipping the fcitx5 addon unchanged. `apt install hunmin` therefore
  means the same thing after this target is added as it does today.
- **hunmin never redistributes fcitx5.** No bundled build, no private prefix, no second
  source package for a target whose archive has an older fcitx5. The addon may depend on a
  distro fcitx5 that meets the 5.1.13 floor; it may not carry one. This closes the item
  §PLT-7 left open ("whether fcitx5 ≥ 5.1.13 is ever shipped by hunmin for a later target").
- **Release artefacts are built by CI.** No maintainer-host release build, no PPA as the
  primary route.
- **Development happens in a VM on this Linux box, shaped like the GitHub CI environment**,
  so that a difference between the developer's host and the runner stops being the reason a
  run goes red. The point named in the sentence is the *failure* it prevents, not the VM
  technology.
- **The tier-C harness model is to be corrected rather than the product deferred**: the
  engine's commit is delivered at the key it belongs to (item 1, "권고안 대로"), and the
  corpus's `.gold` files are regenerated with that model, reviewed line by line (item 2).

## What it does not settle

- **Which mechanism carries the corrected model.** §PLT-8 (below) records the measurement
  that forced this back open: a word-ending symbol and a digit both arrive as "commit with
  an empty preedit" in tier A's stream, so `project_word_buffered` cannot tell them apart.
  The three repairs and their costs are there; the sentence above chose the *outcome*
  (adapter delivers immediately), not the mechanism.
- **How the CI job treats a red test.** Item 3's recommendation was to fail the job on
  `ctest`'s exit code once items 1–2 are in; the sentence accepts it, but the change lands
  after the conformance case is green, and the mechanism (job failure vs a red-count gate)
  is still the implementer's to write down.
- **What the VM is.** Distribution and version, how it is created, and how it is kept in
  step with the runner image are not named. `ubuntu-latest` on the linux job means whatever
  GitHub ships today, so "identical to CI" is a target to be measured, not a property a VM
  has once.
- **Whether the 22.04 release also builds the fcitx5 addon for other targets.** "fcitx5는
  배포하지 않는다" forbids redistributing fcitx5 itself; it does not change the existing
  `.deb`'s `Depends: fcitx5`, which stays a dependency on the distro's package.

## Why the record matters later

Two of these read as implementation details until someone acts on the opposite reading. A
later session that finds jammy's fcitx5 older than the floor has three plausible moves —
bundle fcitx5, carry a second source package, or drop the addon for that target — and only
the last is allowed. And a session that reproduces a CI-only failure on this workstation will
be tempted to fix the workstation: the ruling says the workstation is made like the runner,
not that the failure is re-explained. See
[[2026-11-09-the-22-04-release-ships-ibus-only]] for the target this extends, and
[[decisions-belong-to-the-maintainer]] for why the sentences above are the rule.

---

## PLT-8 — how the tier-C model can carry "the engine committed here", since the projection cannot

This is the measurement that reopened item 1's mechanism, recorded here so the decision page
and the pricing section do not disagree.

**Measured (`tools/ime-host-probe.sh`'s engine aside; the layer driven directly):** the shared
layer's `hunmin_buffered_preedit_process_key` returns the commit on the key that ends the word,
under **both** commit policies:

| keys | word (`HUNMIN_COMMIT_AT_BOUNDARY`) | syllable (`HUNMIN_COMMIT_PER_SYLLABLE`) |
|---|---|---|
| `jfsh` (안 + ㄴ) | commit `''`, preedit `안ㄴ` | commit `안`, preedit `ㄴ` |
| `kf.` (가 + `.`) | **commit `가.`** | **commit `가.`** |
| `kf4` (가 + a digit) | commit `''`, preedit `가ㅛ` | commit `가`, preedit `ㅛ` |
| `kfks ` (가나 + space) | commit `가ᄀᅠᆫ` | commit `가` then `ᄀᅠᆫ` on the space |

So the adapter does deliver at the symbol, today, and tier C's two tools are what lag: both
`frontends/fcitx5/test/trace-emit.cpp` and `tools/trace-check.py#project_word_buffered`
deliver only on `consumed: false`. **A rule over tier A cannot fix that**: in tier A's stream
a word-ending symbol and a digit both appear as "commit non-empty, preedit empty", so any
local rule that catches `.` also fires on `1` (measured: the candidate rule changed 11 events
in `case-sebeolsik-final`, eight of them digits, and would have made the adapter assert
deliveries it never makes).

The three repairs, priced:

1. **The tools ask the layer.** `trace-emit` builds a second `hunmin_buffered_preedit` with the
   same layout and policy, replays the keys, and takes its per-key commit as the expectation;
   the checker's leg 2 becomes "the `.gold` equals that stream", which it can obtain through
   `hunmin-sim` only if the simulator gains a commit-policy flag. *Cost:* the flag, the
   emitter's replay, the checker's leg 2, and every word-buffered `.gold` regenerated.
   *Loses:* the current model's independence from the layer — tier C then asserts "the adapter
   relays the layer", which is FE4/FE9's own claim, while tier A keeps pinning composition.
2. **The trace carries the word-mode stream.** `gen-conformance --delivery word-buffered` emits
   the per-key stream the layer produces in word mode, so the `.in`'s commits ARE the delivery
   points and no projection is needed. *Cost:* the same `hunmin-sim` flag, the generator, the
   `.in`s for all word-buffered cases, and the spec's model section. *Loses:* the `.in` stops
   being tier A verbatim, which is what made "the expectation is tier A's" true.
3. **Keep the model and exclude the class.** Re-scope `case-sebeolsik-final` (and any later
   case that types a word-ending symbol) the way the first 31 multi-syllable cases were
   excluded on 2026-10-25. *Cost:* one case's coverage of the symbol tier. *Loses:* the corpus
   stops exercising exactly the interaction that R4 made meaningful.

What none of them fixes: the 23 other word-buffered `.gold` files were *generated by the
emitter under the same model*, so they cannot be cited as evidence that the model is right —
they agree with it by construction. That is the reason this section exists rather than a
one-line note.