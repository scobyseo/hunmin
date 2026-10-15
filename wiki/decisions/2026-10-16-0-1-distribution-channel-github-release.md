---
title: "0.1: the distribution channel is a GitHub Release on tag 0.1.0"
created: 2026-10-16
updated: 2026-10-16
type: decision
status: active
tags: [decision, release-plan, question]
sources:
  - https://github.com/scobyseo/hunmin/issues/7
  - ../../../docs/MILESTONES.md
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

# 0.1: the distribution channel is a GitHub Release on tag `0.1.0`

Closes the channel half of issue #7. The question was where a 0.1 user obtains
the `.deb` and the APK, given that `README.md` §Install named the maintainer's
home directory as the place the artefact lived.

**Ruling (maintainer, recorded in `docs/MILESTONES.md` §0.1 as "decided
2026-10-12", confirmed 2026-10-16):** a **GitHub Release on tag `0.1.0`**, with
`hunmin_0.1.0_amd64.deb` attached. The procedure is in `docs/MILESTONES.md`
§0.1 and scripted in `tools/release-0.1.sh`. The tag is bare `0.1.0`, no `v`
prefix, per the versionName convention in
[[2026-10-12-versionname-keeps-the-phase-suffix]].

## Why this option and not the other two

Issue #7 named three channels as comparable and asked the maintainer to choose.
Cost, measured rather than asserted:

| channel | cost | satisfies #7's third check? |
|---|---|---|
| **GitHub Release** | none beyond `gh`, already configured; the build path already exists (`tools/build-release.sh`) | yes — one command on a clean machine |
| package repository (the `unstable` the changelog already claims) | a hosting account, a signing key, GPG setup, and publication — N6 being closed is not the same as being published | yes, later, at that cost |
| source-only | nothing, but the reader builds both artefacts and Android needs four ABIs | no |

It also does not foreclose the middle row: a repository can be added later
without changing where the `.deb` comes from.

## What this does NOT close

- **No tag `0.1.0` and no release exist yet.** The channel is a decision about
  where artefacts will live, not a published artefact. Issue #7's third
  verification item — reachable from a clean machine with one command — stays
  unchecked until `tools/release-0.1.sh --yes` has run, and that run is
  deferred. Nothing in the tree may describe the release as existing before it
  does; the README says "no such release exists yet" in as many words.
- **The `.deb` is unsigned.** `dpkg-buildpackage -us -uc` needs no key, and
  requiring one would make the artefact unbuildable on a fresh machine, but
  `dpkg -i` warns. A signed archive is what the middle row above would fix.
- **The signed release APK stays blocked on issue #1** (the keystore path, B8).
  CI's `android-release` job signs with a generated throwaway key, so it proves
  the release *build*, not a release *signature*.
- **The `.deb` is the only asset this decision places.** The APK's channel is
  not settled by it, because there is no shippable signed APK to place.

## Where the procedure and the tree fact live

`docs/MILESTONES.md` §0.1 holds the procedure — the one authoritative home.
This page records why the channel was chosen and what it does not close, which
is the wiki's half of the split (see [[the-rules-keep-breaking]] for what
happens when the same figure is written twice).

The tree fact is separate and lives in `docs/REMAINING-WORK.md`: row C1 records
that CI builds both artefacts (jobs `deb` and `android-release`), and row
`RELEASE-SCRIPT` asserts `tools/build-release.sh`. `tools/release-0.1.sh` calls
that script for the build and owns only tag, release and upload — one build path
in the tree, so the release cannot package differently from what CI and a local
run package.

See [[decisions-belong-to-the-maintainer]] — this page records a ruling the
maintainer made. The request is preserved in issue #7.