---
title: Session 2026-10-02 — where the working practices came from
created: 2026-10-05
updated: 2026-10-10
type: measurement
status: superseded
ingested: 2026-10-05
tags: [postmortem, tooling, decision]
superseded_by: concepts/where-the-practices-came-from.md
confidence: high
---

# Session 2026-10-02 — the practices this project still runs on

> **Superseded.** The content of this record was folded into
> `wiki/concepts/where-the-practices-came-from.md`, which now owns it: the five
> practices in the maintainer's words, with the date each was given and the
> artefact it produced. This page is kept, marked, because deleting it destroys
> the evidence that the reasoning existed before `AGENTS.md` compressed it to one
> line per rule (`SCHEMA.md`: a superseded page stays).

Session `20261002_025752_d7f56b`, 1,777 messages, 87 of them the maintainer's.
It predates the MODES work entirely and is where most of `docs/ENGINEERING.md`,
the postmortem convention, and `AGENTS.md`'s hard rules came from. It is
recorded because **the instructions are still in force and were nearly lost** —
they existed only in that transcript.

What moved where:

| in this record | now in |
|---|---|
| the documentation rule, and why documentation is not a deliverable | `wiki/concepts/where-the-practices-came-from.md` |
| the mistake-list convention and its amendment path | `wiki/concepts/where-the-practices-came-from.md`; the rule itself is `AGENTS.md` §Working rules |
| read the upstream's own developer documentation first | `wiki/concepts/where-the-practices-came-from.md`; `docs/FCITX5-ADDON-NOTES.md` |
| read what the code assumes, not what it does | `wiki/concepts/where-the-practices-came-from.md`; `docs/FRONTEND-CONTRACT.md` |
| phase commits; do not let an unreachable phase block the reachable ones | `wiki/concepts/where-the-practices-came-from.md`; `PLAN.md` P6 |
| trust the recorded state over a fresh impression | `wiki/concepts/where-the-practices-came-from.md`; `AGENTS.md` §Working rules |
| the hanja research that produced `libhunmin/data/krdict/` | `docs/GLOSSARY.md`, `PLAN.md`, and `AGENTS.md` §Licences |

The failure experiences belong to `wiki/concepts/the-rules-keep-breaking.md`. The
practices that arrived *after* this session and are equally load-bearing are on
[[2026-10-04-stemming-investigation]], which is where the standing instruction
recorded on `wiki/decisions/decisions-belong-to-the-maintainer.md` was broken the
same day it was given.