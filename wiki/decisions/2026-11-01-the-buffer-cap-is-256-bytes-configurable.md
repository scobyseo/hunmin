---
title: The buffer cap is 256 bytes by default and configurable
created: 2026-11-01
updated: 2026-11-08
type: decision
status: active
tags: [decision, modes, engine]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../libhunmin/src/buffered_preedit.c
confidence: high
---

# The buffer cap is 256 bytes by default and configurable

**Ruling (2026-11-01, issue #163, E2).** The maintainer's steer, verbatim, given
mid-session while the parameter was being priced:

> "buffer cap should be reasonable; at least longest english word should be
> covered by it. i think 4KB buffer can be managable; if it is to be overflowed,
> we can flush first half of the buffer."

and then the value, verbatim:

> "make cap 256B by default, and make it configurable."

## What it settles

- **The cap is bytes of UTF-8, 256 by default** — `word_cap` on the layer,
  `0` meaning no cap. It is a layer option, not a constant, so a host may raise
  or remove it without an engine change.
- **An overflow commits the word's first half at a UTF-8 syllable boundary and
  keeps the rest.** The flush is lossless, and a flushed half is never judged —
  judgement is boundary-only, and the flush empties the key log
  (`docs/SPEC-MODES.md` §ReqMode4).
- **The value is below the steer's own suggestion.** 4 KB was the shape the
  person had in mind; 256 B is what the ruling chose after seeing what the cap
  costs at each length. The two are not in conflict: the steer said what the cap
  must be *reasonable for*, and the chosen number keeps the longest English word
  inside it.

## What it does not settle

- **The digit-run interaction.** A digit costs one byte and a syllable three, so
  a digit run reaches the default at 256 keys where syllables reach it at 85 —
  the same rule, counted in another unit; ReqMode6's run and the cap are
  measured together in `engine-buffered-preedit`.
- **Whether a host should expose the cap.** Nothing binds a frontend to a
  settings row; the option exists for the platform that needs it.

See [[2026-11-01-the-engine-judges-the-frontend-switches]] for the ownership
ruling this parameter belongs to, and
[[2026-11-01-the-judgement-timing-is-per-key-by-default]] for the other boundary
parameter decided the same day.