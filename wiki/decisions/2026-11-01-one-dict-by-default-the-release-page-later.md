---
title: One dictionary by default, and the release page for the next one
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, data, distribution, b3, packaging, 0-3]
sources:
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/SPEC-MODES.md
  - ../../../debian/control
confidence: high
---

# One dictionary by default, and the release page for the next one

**Ruling (2026-11-01, issue #172).** The maintainer's words, verbatim, in the
order they were sent:

> "Recommends."

> "have one dict by default because we don't have additional distribution mech.
> may use github release page for additional package distribution mech. we need
> to test the feature even before we have distribution mech."

> "close both, when this is closed."

## What it settles

- **The dependency is `Recommends`, not `Suggests` and not `Depends`.** A plain
  `apt install hunmin` brings one dictionary; the words package stays a package
  of its own, so `--no-install-recommends`, a distro that drops it, or an
  uninstall still produce an installation without the data.
- **One dictionary on every surface.** The Android APK keeps its dictionary in
  the assets, and the Windows installer keeps `data\` whole. This is not an
  omission to be fixed later; it is what "one dict by default" means.
- **The mechanism for a *second* dictionary is deferred, and its candidate is
  named**: the GitHub release page. Nothing about it is built.
- **The feature must be usable before any distribution mechanism exists.**
  Today's data is in the tree and every test drives it there
  (`HUNMIN_DATA_PATH`), so the judgement, the gate and the frontends can be
  developed and measured without waiting for packaging. The B3 gate (#169) is
  therefore *not* a predecessor of the feature; it is a second answer to a state
  that a default install no longer produces.

## What it does NOT settle

- **The release page's shape.** An asset name, a checksum, a version pin, who
  fetches it (the app at first use, an installer at install time, or the person
  by hand), and what happens when it is unreachable while someone is typing.
- **Whether the shape generalises to the other two surfaces.** The Debian seam
  (`hunmin-en-us`) is a package; a release-page artefact is a file. A second
  dictionary for the APK would need the runtime path the ruling leaves
  undefined, and the Windows installer would need a `[Components]` entry.
- **The absent-state behaviour at the option level.** `hunmin_ic_set_option(ic,
  "autoswitch", 1)` still succeeds where the data is absent (#169). The ruling
  makes that state rarer, not unreachable, and it does not say what the option
  should answer when it happens.
- **The package name.** `hunmin-en-us` stands in; B3's decision page ruled the
  name "design work, not ruled" and this ruling did not change that.

## Why the record matters later

The change that produced it (#172) is a split whose *default* is deliberately
"not split from the user's point of view". A later session reading only the
diff would see a second binary package and conclude that the data had been made
optional, then either wire the absent state into a settings row that users will
never meet, or — worse — remove the seam as unnecessary because `Recommends`
always installs it. The seam exists for the *next* dictionary: the one for which
no distribution mechanism is built yet. See also
[[2026-10-11-auto-switch-off-by-default]] for the B3 ruling this extends, and
[[decisions-belong-to-the-maintainer]] for why this page quotes rather than
paraphrases.
