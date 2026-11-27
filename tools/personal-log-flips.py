#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Read a personal key log: how much does the screen flicker, and what would fix it.

Docs: `docs/MODES-MEASUREMENT.md` §2.5 (the flip unit), `docs/trace-spec.md`
§"Personal key log" (the format), `docs/SPEC-MODES.md` §E6 (`judge_hold`).

WHY THIS EXISTS. The maintainer's requirement (2026-11-27) is not a corpus metric
("method X reduces problem Y") but a **case-attributed** one: from what actually
happened while they typed, how many words would have come out differently — and
*how many of the same words* would stop flickering under a candidate setting. A
trace answers the first half only; this tool answers both by replaying the SAME
key sequence (`in.base/shift` from the log) through the layer with a different
`judge_hold`, so the comparison is on their own words, not on a corpus.

WHAT IT REPORTS

  from the log      words, mid-word display flips, words that flip back (the
                    도깨비 shape), and the direction (mode at the word s start vs
                    the language rendered), exactly `docs/MODES-MEASUREMENT.md`
                    §2.5's unit;
  counterfactual    the same keys replayed at `--replay-hold` values (default 0 and
                    1), i.e. "with this setting, how many of MY words would have
                    stopped flickering", plus how many would still flicker;
  repairs           a descriptive count, not a verdict: a declined BackSpace and
                    the gap before it. Telling a typo from a repair needs the key
                    relation (same keys re-typed after a mode change) and that
                    judgement is deliberately NOT made here.

WHAT IT DOES NOT DO. It does not know what the person intended — it cannot say a
word was "wrong", only what was rendered and what the judgement did. It does not
replay the client's edits (the log has the engine's keys, not the application's
text). Both are why the findings it produces are candidates for review.

USAGE
  tools/personal-log-flips.py --log ~/.local/state/hunmin/key-log.jsonl
  tools/personal-log-flips.py --log LOG --replay-hold 0 1 2
  tools/personal-log-flips.py --selfcheck      # synthesises a log, checks both halves

The layer is the oracle, as everywhere in this tree: the replay drives the built
`hunmin_buffered_preedit`, and the flip detector is `modes-metrics.py`'s own
(tools/modes-metrics.py `display_language`), not a second definition.
"""
import argparse
import collections
import importlib.util
import json
import pathlib
import sys

try:
    from _common import ROOT as REPO
except ImportError:
    REPO = pathlib.Path(__file__).resolve().parents[1]

HANGUL, LATIN = "hangul", "latin"


def load(name):
    path = pathlib.Path(__file__).resolve().parent / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_log(path):
    """[(meta, [event, ...])] — the meta line plus the event lines, in order."""
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
                continue
            if "in" in rec:
                events.append(rec)
    return meta, events


def words_of(events):
    """Split the event stream into words the way the layer does.

    A word is the run of CONSUMED keys; a declined key is the boundary (space,
    punctuation, Enter), which is what a frontend forwards. Every word carries
    the rendered preedit per key and the mode the layer was in.
    """
    words, cur = [], []
    for ev in events:
        cur.append(ev)
        if not ev.get("consumed"):
            if len(cur) > 1:  # the boundary key alone is not a word
                words.append(cur)
            cur = []
    if len(cur) > 1:
        words.append(cur)
    return words


def flips_of_word(mm, word):
    """(changes, direction) — §2.5's unit over one word's rendered preedits."""
    shown, changes, first_mode = None, 0, None
    for ev in word:
        if ev.get("mode") in (HANGUL, LATIN) and first_mode is None:
            first_mode = ev["mode"]
        lang = mm.display_language(ev.get("preedit", ""))
        if lang is None:
            continue
        if shown is None:
            shown = lang
        elif lang != shown:
            changes += 1
            shown = lang
    direction = f"{first_mode}->{shown}" if first_mode and shown else "?"
    return changes, direction


def summarise(mm, words):
    per_dir = collections.Counter()
    changes = with_flip = with_flip_back = 0
    for word in words:
        n, direction = flips_of_word(mm, word)
        changes += n
        if n:
            with_flip += 1
            per_dir[direction] += 1
        if n >= 2:
            with_flip_back += 1
    return {"words": len(words), "flips": changes, "words_with_a_flip": with_flip,
            "words_with_a_flip_back": with_flip_back, "by_direction": dict(per_dir)}


def repairs_of(events):
    """Descriptive only: declined BackSpace/Delete presses and the gap before them."""
    count, gaps, last_t = 0, [], None
    for ev in events:
        t = ev.get("t")
        base = (ev.get("in") or {}).get("base")
        if base in (0x08, 0x7F) and not ev.get("consumed"):
            count += 1
            if isinstance(t, int) and isinstance(last_t, int):
                gaps.append(t - last_t)
        if isinstance(t, int):
            last_t = t
    gaps.sort()
    return {"backspaces": count,
            "median_gap_before_ms": gaps[len(gaps) // 2] if gaps else None,
            "max_gap_before_ms": gaps[-1] if gaps else None}


def replay(mm, engine, events, meta, hold):
    """Return the same summarise() over the SAME keys, at one judge_hold value."""
    hungul = (meta or {}).get("layout") or "sin3-p2-basic"
    layer = engine.layer(hungul, "us-qwerty", 1)
    if hold:
        layer.set_option("judge_hold", hold)
    words, cur = [], []
    for ev in events:
        in_ = ev.get("in") or {}
        consumed, _commit, _preedit = layer.key(int(in_.get("base", 0)),
                                                int(in_.get("shift", 0)))
        cur.append({"consumed": 1 if consumed else 0,
                    "preedit": _preedit, "mode": layer.language()})
        if not consumed:
            if len(cur) > 1:
                words.append(cur)
            cur = []
        if layer.judgement() >= 0:  # -1 is "no judgement", and it is truthy
            layer.toggle()          # the frontend performs the switch
    if len(cur) > 1:
        words.append(cur)
    layer.close()
    return summarise(mm, words)


def collect(args):
    mm = load("modes-metrics")
    meta, events = read_log(args.log)
    if not events:
        raise SystemExit(f"{args.log}: no event lines -- is this a personal key log?")
    words = words_of(events)
    engine = mm.Engine(args.lib, args.data)
    record = {
        "log": {"path": str(args.log), "meta": meta, "events": len(events),
                "keys_with_clock": sum(1 for e in events if isinstance(e.get("t"), int))},
        "from_the_log": summarise(mm, words),
        "repairs": repairs_of(events),
        "replay": {},
    }
    for hold in args.replay_hold:
        record["replay"][str(hold)] = replay(mm, engine, events, meta, hold)
    return record


def report(record):
    log = record["log"]
    base = record["from_the_log"]
    print(f"log {log['path']}  layout {log['meta'].get('layout') if log['meta'] else '?'}  "
          f"started {log['meta'].get('started') if log['meta'] else '?'}")
    print(f"events {log['events']}  keys with a clock {log['keys_with_clock']}")
    print(f"from the log: words {base['words']}, flips {base['flips']}, "
          f"words with a flip {base['words_with_a_flip']}, "
          f"flip-back {base['words_with_a_flip_back']}")
    if base["by_direction"]:
        print(f"  direction (mode at word start -> rendered): {base['by_direction']}")
    rep = record["repairs"]
    print(f"repairs seen: backspace presses {rep['backspaces']}, "
          f"median gap before one {rep['median_gap_before_ms']} ms "
          f"(descriptive: a typo and a repair look alike without the key relation)")
    for hold, r in sorted(record["replay"].items(), key=lambda kv: int(kv[0])):
        delta = base["flips"] - r["flips"]
        pct = f"{100 * delta / base['flips']:.0f}%" if base["flips"] else "n/a"
        print(f"  judge_hold {hold}: flips {r['flips']} "
              f"({delta:+d} vs the log, {pct} of them), "
              f"flip-back words {r['words_with_a_flip_back']}")


def selfcheck(args):
    """Synthesise a log, then prove both halves: the log reads, and the replay moves."""
    mm = load("modes-metrics")
    lib = pathlib.Path(args.lib)
    if not lib.exists():
        print("SELFCHECK SKIP: build/libhunmin.so not built", file=sys.stderr)
        return 77
    engine = mm.Engine(lib, args.data)
    keys = [ord(c) for c in "alphabetically "]
    # A log as a trace build would write it: same keys, hold 0, mode tracked.
    layer = engine.layer("dubeolsik", "us-qwerty", 1)
    events, last_t = [], 0
    for base in keys:
        consumed, _commit, preedit = layer.key(base, 0)
        last_t += 90
        ev = {"in": {"base": base, "shift": 0}, "consumed": 1 if consumed else 0,
              "preedit": preedit, "mode": layer.language(), "t": last_t}
        events.append(ev)
        if layer.judgement() >= 0:
            ev["judged"] = "latin"
            layer.toggle()
    layer.close()
    meta = {"layout": "dubeolsik", "platform": "x11", "started": "synthetic"}
    problems = []
    live = summarise(mm, words_of(events))
    if live["flips"] < 2:
        problems.append(f"the synthetic log should flicker; it reports {live}")
    engine = mm.Engine(lib, args.data)
    held = replay(mm, engine, events, meta, 1)
    if held["flips"] >= live["flips"]:
        problems.append(f"judge_hold 1 did not reduce the synthetic flicker: "
                        f"{live['flips']} -> {held['flips']}")
    if problems:
        print("SELFCHECK FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print(f"OK  synthetic log flickers {live['flips']} times, the same keys at "
          f"judge_hold 1 flicker {held['flips']}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--log", type=pathlib.Path)
    ap.add_argument("--replay-hold", type=int, nargs="+", default=[0, 1])
    ap.add_argument("--data", type=pathlib.Path, default=REPO / "libhunmin" / "data")
    ap.add_argument("--lib", type=pathlib.Path, default=REPO / "build" / "libhunmin.so")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck(args)
    if not args.log:
        ap.error("--log is an argument, never a default")
    record = collect(args)
    if args.json:
        print(json.dumps(record, ensure_ascii=False, indent=1))
    else:
        report(record)
    return 0


if __name__ == "__main__":
    sys.exit(main())