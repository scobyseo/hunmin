#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Read a personal key log with the maintainer's model of typing intent.

Docs: `docs/MODES-MEASUREMENT.md` §1.6 (the model and the units),
`docs/trace-spec.md` §"Personal key log" (the fields), `docs/SPEC-MODES.md` §E6.

THE MODEL, in the maintainer's words (2026-11-27, verbatim):

> "사람이 타이핑 할 때는, 내가 한글을 입력 중인지, 영어를 입력 중인지 하나의
> 가정을 두고 입력해. 그 가정은 실제 한글 모드에 있는지 영어 모드에 있는지와
> 상관없이, 한글을 입력하기 위해서는 이렇게 타이핑해야 해야 한다는 의지라고 볼 수
> 있어. 그래서 똑같은 입력이 한글로도 영어로도 입력될 수 있지만, 사람은 그 순간에
> 한글 입력으로만 인식하는 거고, 그게 영어로 입력될 수 있다는 사실을 인지하지
> 못해. 반대도 성립하고. 그래서 네가 사람이 의도한 것이 한글 입력이었구나 인식할
> 수 있는 건, 최종적으로 입력하고 넘어간 단어를 보고 알 수 있는 거고, 그 중간에
> 여러 실수가 있었다면, 그게 내가 느끼는 불편함일 거야."

What that fixes for this tool, and why no other unit will do:

  intent      = the text of the word the person finally accepted and moved past.
                Not the mode they were in (they were not tracking it), not the
                first attempt (it may have been wrong). The accepted word is the
                one observable that means what they meant.
  discomfort  = the distance between the attempts and that accepted word: keys
                spent, deletions, mode switches, and above all an ENGINE REWRITE
                THE PERSON THEN UNDID. That last one is the case this tool exists
                to separate from ordinary typing.
  the engine  = the only actor whose change the person did not type. `judged` on
                the boundary key names it, which is why that field exists.

A SLOT is one word attempt: the run of keys up to a key the engine declined and
forwarded (space, Enter, an unmapped key). Its `delivered` text is what the
application holds for that slot when the person moves on.

CLASSES (mutually exclusive, first match wins)

  judged_then_reverted  the engine rewrote the word and the person deleted it and
                        typed the word again in the OTHER script (the replacement
                        is its own slot in the log; this class is the pair) — the
                        engine was wrong from the typist's side, which is the
                        discomfort the model names. Its replacement slot is
                        reported as `replacement`, with its keys and deletions
                        folded into the rewrite's cost.
  judged_kept           the engine rewrote the word and the person kept it.
  repaired_typo         deletions, no engine rewrite, the final script is the one
                        the slot started in — their own typo, not the engine's.
  emptied               deletions, nothing left: the word was abandoned.
  clean                 no deletions, no rewrite.

ASSUMPTIONS, stated because they are not observable here

  - A declined BackSpace/Delete removes ONE character from the application's
    text. Where surrounding text is available it should replace this; the log
    does not carry it yet.
  - A boundary key's commit belongs to the slot BEFORE it (ReqMode4: a delimiter
    terminates the word), and the boundary character itself is not part of the
    word.

USAGE
  tools/personal-log-words.py --log ~/.local/state/hunmin/key-log.jsonl
  tools/personal-log-words.py --log LOG --show 20 --json
  tools/personal-log-words.py --selfcheck      # three synthetic slots, no build
"""
import argparse
import collections
import json
import pathlib
import sys

HANGUL, LATIN = "hangul", "latin"
BACKSPACE, DELETE = 0x08, 0x7F


def script_of(text):
    has_h = any("\uac00" <= c <= "\ud7a3" or "\u1100" <= c <= "\u11ff" for c in text)
    has_l = any(c.isascii() and c.isalpha() for c in text)
    if has_h and not has_l:
        return HANGUL
    if has_l and not has_h:
        return LATIN
    return None


def read_log(path):
    meta, events = None, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue  # a torn last line is a killed process, not a finding
            if "meta" in rec:
                meta = rec["meta"]
            elif "in" in rec:
                events.append(rec)
    return meta, events


def slots_of(events):
    """One record per word slot, anchored on the text the application accepted."""
    slots, cur = [], None
    for ev in events:
        in_ = ev.get("in") or {}
        base = int(in_.get("base", 0))
        consumed = bool(ev.get("consumed"))
        commit = ev.get("commit") or ""
        preedit = ev.get("preedit") or ""
        is_delete = (not consumed) and base in (BACKSPACE, DELETE)
        is_lang = bool(ev.get("lang"))
        if cur is None:
            cur = {"client": "", "keys": 0, "deletes": 0, "lang_keys": 0,
                   "judged": [], "first_text": "", "preedit": ""}
        if commit:
            cur["client"] += commit
        if not consumed:
            if is_delete:
                cur["deletes"] += 1
                cur["client"] = cur["client"][:-1]
            elif base and base >= 0x20 and base < 0x7F:
                cur["client"] += chr(base)  # forwarded printable key
        if is_lang:
            cur["lang_keys"] += 1
        if ev.get("judged"):
            cur["judged"].append(ev["judged"])
        if not cur["first_text"]:
            cur["first_text"] = cur["client"] + preedit
        cur["preedit"] = preedit
        cur["keys"] += 1
        boundary = (not consumed) and not is_delete and not is_lang and base < 0x10000
        if boundary:
            # The boundary character is the separator, not part of the word; the
            # commit that arrived with it is already in `client`.
            word = cur["client"]
            if cur["client"].endswith(chr(base)) and base and base < 0x7F:
                word = cur["client"][:-1]
            cur["delivered"] = word
            slots.append(cur)
            cur = None
    if cur is not None:  # a log that ends mid-word still has a slot
        cur["delivered"] = cur["client"]
        slots.append(cur)
    return slots


def pair_rejections(slots):
    """Mark slots whose word was a rewrite the person then replaced.

    The rejection spans two slots, because the engine delivers at the boundary:
    the rewritten word is one slot, and the deletion + re-typed word is the next.
    Pairing them is what the model asks for -- the intent is the word finally
    accepted, and the rewrite before it is the mistake.
    """
    for i, slot in enumerate(slots[:-1]):
        nxt = slots[i + 1]
        if not slot["judged"] or slot["deletes"]:
            continue
        if nxt["deletes"] < max(1, len(slot["delivered"])):
            continue
        final = script_of(nxt["delivered"])
        if not final or final in slot["judged"]:
            continue
        slot["class"] = "judged_then_reverted"
        slot["replacement"] = {"delivered": nxt["delivered"], "keys": nxt["keys"],
                               "deletes": nxt["deletes"]}
        nxt["class"] = "replacement"
    return slots


def classify(slot):
    delivered = slot["delivered"]
    final = script_of(delivered)
    first = script_of(slot["first_text"])
    judged = slot["judged"]
    if judged and slot["deletes"] and final and final not in judged:
        return "judged_then_reverted"  # undone inside the same slot (mid-word)
    if judged and not slot["deletes"]:
        return "judged_kept"
    if slot["deletes"] and not delivered:
        return "emptied"
    if slot["deletes"]:
        return "repaired_typo"
    if judged:
        return "judged_kept"
    return "clean"


def summarise(slots):
    counts = collections.Counter()
    keys = collections.Counter()
    deletes = collections.Counter()
    examples = collections.defaultdict(list)
    for slot in pair_rejections(slots):
        cls = slot.get("class") or classify(slot)
        counts[cls] += 1
        keys[cls] += slot["keys"]
        deletes[cls] += slot["deletes"]
        if cls == "judged_then_reverted" and slot.get("replacement"):
            keys[cls] += slot["replacement"]["keys"]
            deletes[cls] += slot["replacement"]["deletes"]
        if len(examples[cls]) < 5 and slot["delivered"]:
            examples[cls].append({"word": slot["delivered"], "judged": slot["judged"],
                                  "keys": slot["keys"], "deletes": slot["deletes"]})
    replaced = counts.get("replacement", 0)
    return {"slots": len(slots), "intents": len(slots) - replaced,
            "by_class": dict(counts),
            "keys_by_class": dict(keys), "deletes_by_class": dict(deletes),
            "examples": dict(examples)}


def report(record):
    s = record["summary"]
    print(f"log {record['log']['path']}  layout {record['log']['meta'].get('layout') if record['log']['meta'] else '?'}")
    print(f"slots {s['slots']}  intents {s['intents']} "
          f"(a rejected rewrite is two slots, one intent)")
    for cls in ("clean", "judged_kept", "repaired_typo", "emptied",
                "judged_then_reverted", "replacement"):
        n = s["by_class"].get(cls, 0)
        if not n:
            continue
        share = 100 * n / s["slots"] if s["slots"] else 0
        print(f"  {cls:22} {n:5} ({share:4.1f}%)  keys {s['keys_by_class'].get(cls, 0):6}"
              f"  deletes {s['deletes_by_class'].get(cls, 0)}")
    eng = s["by_class"].get("judged_kept", 0) + s["by_class"].get("judged_then_reverted", 0)
    rev = s["by_class"].get("judged_then_reverted", 0)
    if eng:
        print(f"  engine rewrites: {eng}, of which the person UNDID {rev} "
              f"({100 * rev / eng:.0f}%) -- the discomfort the model names")
    for cls, rows in record["summary"]["examples"].items():
        print(f"  e.g. {cls}: {[r['word'] for r in rows]}")


def selfcheck():
    """Three synthetic slots; the engine-rewrite-reverted case must be separated."""
    def ev(base, consumed, commit="", preedit="", judged=None, lang=False, mode="hangul"):
        out = {"in": {"base": base, "shift": 0}, "consumed": 1 if consumed else 0,
               "commit": commit, "preedit": preedit, "mode": mode}
        if judged:
            out["judged"] = judged
        if lang:
            out["lang"] = True
        return out

    events = []
    # slot 1: `hello` typed in Hangul mode, the engine rewrites it, kept.
    for ch in "hello":
        events.append(ev(ord(ch), True, preedit="ㅗ"))
    events.append(ev(0x20, False, commit="hello", judged="latin", mode="latin"))
    # slot 2: the same keys, the engine rewrites, the person undoes it and
    # re-types with the other mode -- the discomfort case.
    for ch in "hello":
        events.append(ev(ord(ch), True, preedit="ㅗ"))
    events.append(ev(0x20, False, commit="hello", judged="latin", mode="latin"))
    for _ in range(5):
        events.append(ev(BACKSPACE, False, mode="latin"))
    events.append(ev(0xFFEA, True, lang=True, mode="hangul"))
    for ch in "gksrmf":
        events.append(ev(ord(ch), True, preedit="한"))
    events.append(ev(0x20, False, commit="한글", mode="hangul"))
    # slot 3: their own typo, no engine involvement.
    for ch in "hellp":
        events.append(ev(ord(ch), True, preedit="hellp"))
    events.append(ev(BACKSPACE, False))
    for ch in "o":
        events.append(ev(ord(ch), True, preedit="hello"))
    events.append(ev(0x20, False, commit="hello"))
    slots = slots_of(events)
    summary = summarise(slots)
    got = [sl.get("class") or classify(sl) for sl in slots]
    problems = []
    if summary["by_class"].get("judged_kept") != 1:
        problems.append(f"the kept rewrite was not counted: {summary['by_class']}")
    if summary["by_class"].get("judged_then_reverted") != 1:
        problems.append(f"the undone rewrite was not paired: {summary['by_class']}")
    if summary["by_class"].get("repaired_typo") != 1:
        problems.append(f"the plain typo was not counted: {summary['by_class']}")
    if summary["by_class"].get("replacement") != 1:
        problems.append(f"the replacement slot was not named: {summary['by_class']}")
    if slots[1]["delivered"] != "hello" or slots[2]["delivered"] != "한글":
        problems.append(f"slots read {[s['delivered'] for s in slots]}, "
                        f"expected ['hello', 'hello', '한글', 'hello']")
    if problems:
        print("SELFCHECK FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print("OK  kept rewrite, undone rewrite (the discomfort case) and a plain typo "
          "are three different classes")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--log", type=pathlib.Path)
    ap.add_argument("--show", type=int, default=5)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck()
    if not args.log:
        ap.error("--log is an argument, never a default")
    meta, events = read_log(args.log)
    if not events:
        raise SystemExit(f"{args.log}: no event lines -- is this a personal key log?")
    slots = slots_of(events)
    record = {"log": {"path": str(args.log), "meta": meta, "events": len(events)},
              "summary": summarise(slots),
              "slots": [{**{k: v for k, v in s.items() if k != "preedit"},
                         "class": classify(s)} for s in slots]}
    if args.json:
        print(json.dumps(record, ensure_ascii=False))
    else:
        report(record)
    return 0


if __name__ == "__main__":
    sys.exit(main())