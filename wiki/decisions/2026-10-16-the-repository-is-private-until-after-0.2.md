---
title: "The repository is private until after 0.2"
created: 2026-10-16
updated: 2026-10-16
type: decision
status: active
tags: [decision, release-plan, question]
sources:
  - https://github.com/scobyseo/hunmin/issues/7
  - ../../../docs/MILESTONES.md
  - ../../../tests/manual-checklists/release-0.1.md
confidence: high
---

# The repository is private until after 0.2

**Ruling (maintainer, 2026-10-16):** `scobyseo/hunmin` stays **private through
0.1** and goes public **after 0.2**. Said as a response to the measurement below,
which is what prompted it.

## The measurement that prompted it

Issue #7's third verification item asks whether the chosen channel is reachable
from a clean machine with one command. Measured 2026-10-16, after the 0.1.0
release was published:

```
$ gh repo view scobyseo/hunmin --json visibility    →  "PRIVATE"
$ curl -sSL -o /dev/null -w '%{http_code}' \
    https://github.com/scobyseo/hunmin/releases/download/0.1.0/hunmin_0.1.0_amd64.deb
404
$ curl -s -o /dev/null -w '%{http_code}' https://github.com/scobyseo/hunmin/releases/tag/0.1.0
404
$ curl -s -o /dev/null -w '%{http_code}' https://github.com/scobyseo/hunmin
404
```

The release exists, the tag exists, and the attached `.deb` is the correct
artefact — verified by download and `dpkg -c`, 1,252,740 bytes, the build the
release was supposed to carry and not the stale 1,253,210-byte one that
`f904d6a` records. None of that is reachable without an account.

So #7 stays **open**. Its first two items pass and the third cannot, and the
reason is now a recorded fact rather than an open question.

## What this ruling does not settle

- **It does not make 0.1 unreachable in principle.** Access is granted per
  account; anyone the maintainer invites can fetch 0.1 today. What does not exist
  is an *anonymous* download.
- **It does not say what "after 0.2" means mechanically** — whether the flip
  happens before the 0.2 tag or after it, and who presses the button. That is a
  release-time act, like the tag itself.
- **It does not retire issue #7.** #7 closes when a clean machine can reach the
  channel, which under this ruling is 0.2 at the earliest. The honest disposition
  is to leave it open and re-measure at the flip, not to close it against the
  intent of the wording.

## The consequence that has to be managed

0.2 is the first release whose words are read by someone who does not already
have access to the tree. That makes every user-facing document part of 0.2's
definition of done rather than incidental prose: `README.md`,
`docs/release-notes/`, and the `/usr/share/doc/hunmin/README.md.gz` inside the
`.deb` — the last one travels with the artefact, so a stale claim in `README.md`
is a stale claim in every installed copy.

This is not hypothetical. The 0.1 `.deb` as published ships a `README.md.gz`
byte-identical to master's, and that README said, at the moment it was packaged
and published:

> No such release exists yet: `git tag` holds no `0.1.0`, so today the only way
> to obtain the artefacts is the CI run artefact or a local
> `tools/build-release.sh`.

True when written, false when packaged, and it reached an artefact that was
itself the proof. `docs/MILESTONES.md` §0.2 and `README.md` now carry the real
state.

## The shape of the mistake, which is the third instance

`f904d6a` named two earlier instances in this same piece of work — the tier E2
skip and the unregistered Android tiers — and stated the common shape: *a check
that asserts existence where it needed to assert identity*. This is the same
shape again, one level out. Every assertion about the release was about
existence: the tag exists, the release exists, the asset is there, the file has
the right bytes. The one property issue #7 actually asks for — **can somebody who
is not already inside the project fetch it** — was never measured, by anything,
including by hand.

`tools/release-0.1.sh` now closes it: after upload it fetches the asset URL
**unauthenticated**, compares the status against the repository's actual
visibility, and refuses to finish when the documents' access claim and the
measured reachability disagree. A private repository returning 404 is a recorded
state; documents claiming an anonymous download while the repository is private
is a contradiction, and that is what the check refuses.

See [[decisions-belong-to-the-maintainer]] — this page records a ruling the
maintainer made. The measurement is preserved in issue #7, and the channel the
ruling is about is recorded in
[[2026-10-16-0-1-distribution-channel-github-release]]; the two are separate
questions — *where* the artefact lives versus *who* can reach it — and answering
the first is not answering the second.