---
title: The practices this project runs on, and why they were written down
created: 2026-10-10
updated: 2026-10-10
type: concept
status: active
tags: [tooling, postmortem, decision]
sources:
  - ../raw/sessions/2026-10-02-working-practices.md
confidence: high
---

# The practices this project runs on, and why they were written down

`AGENTS.md` states these as one line each. It has to — a standing instruction
that carries its own reasoning stops being read as a rule and starts being read
as a preference. The reasoning is what survives a change of circumstances, so
it is kept here, in the maintainer's words, with the date each was given.

Source: session `20261002_025752_d7f56b`, predating the MODES work entirely. It
existed only in that transcript and was nearly lost; the hanja research in it is
what produced `libhunmin/data/krdict/`.

## Documentation is not a deliverable

> "지금 만든 문서를 네가 지속적으로 업데이트하면서, 코드의 오류를 발견하거나, 잘못된
> 수업을 방지할 수 있게 활용할 수 있는지 확인해."

> "디버깅에 시간을 많이 들인 것들은 꼭 기록해 두자."

> "둘 다 해소하자. 특히 문서화하지 않으면 네가 같은 실수를 반복하기 때문에 문서화를
> 빠뜨리지 마."

The reason is stated, not implied: **documentation is the thing that stops the
same defect recurring.** A document that is written once and describes what
shipped does not do that; the note has to be where the next session will look,
and it has to say what was *wrong*, not only what is now true.

## A mistake list needs its own amendment path

> "개발 중 추정한 것이 틀리거나, 실수한 것을 발견하면, 실수 목록으로 정리해서 반복하지
> 않게 하는 스킬을 만들자. 주의해야 할 것은, 지금은 실수이지만 나중에는 그렇게 하는 게
> 맞을 수 있으니, 개정할 방법도 필요해."

**A mistake list that cannot be amended becomes superstition.** The same caution
applies to this wiki, and it is why a `status: superseded` page is kept and
marked rather than quietly deleted — see [[the-rules-keep-breaking]], where two
of the entries record how this project has been wrong about the rules themselves.

## Read what the code assumes, not what it does

> "현재 코드를 점검하자. 각 코드가 어떤 가정을 가지고 어떤 역할을 하도록 구현했는지
> 어떤 인터페이스를 가지는지 정리하자. 그러면 그 가정이 맞는지 확인해 볼 수 있을 것
> 같아."

Produced `docs/FRONTEND-CONTRACT.md`. The insight generalises past that
document: **a comment saying what code does is worth less than a statement of
what would have to be true for it to be correct.** The first is readable from
the source; the second is falsifiable.

The same session produced `docs/FCITX5-ADDON-NOTES.md` by being asked to read
upstream's *own* For Developers documentation before designing against it — the
rule has since been restated three times, and is why the fcitx5 contract is
written from what upstream actually does rather than from what a addon would
plausibly do.

## Do not let an unreachable phase block the reachable ones

> "phase 단위로 commit을 하고, windows IME는 지금 개발 못하니까 그거 빼고 할 수있는 데까지
> 쭉 하자."

> "필요한 문서는 업데이트하고, master와 merge부터 해야 해."

Phase-scoped commits are in `AGENTS.md` under Working rules. The second half is
not a rule but a consequence, and it is why `PLAN.md` lists the Windows TSF
frontend (P6) as **not started** with a recorded architecture reference rather
than as an open blocker. It is also why `AGENTS.md` opens by saying a Windows
frontend is planned and does not exist yet, rather than implying the gap is
temporary or imminent.

## Trust the recorded state over a fresh impression

> "engine-d3-x11은 구체적으로 어떤 문제야?"

> "F9만 먼저 검사해 조건부 소비를 유지할 수 있는데, 왜 모든 키를 삼켜야 했는가?"

Both are the maintainer refusing a summary and asking what is actually true —
the same instinct as *"이거 사실인지 확인해 봐. 내가 말한 게 진리는 아니야."*,
now in `AGENTS.md` as **check the maintainer's claims as carefully as your own**.
It has twice paid for itself.

See also [[decisions-belong-to-the-maintainer]] for the instruction this
project's reporting shape comes from, and [[2026-10-03-requirement-shaped]] for
the requirement in the same register.