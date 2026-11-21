#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Report suspected rework episodes in a personal typing trace.

This is a diagnostic for one person's own opt-in trace, not a shipped feature
and not a tier-C conformance check. It reads the existing JSONL trace format
(docs/trace-spec.md) and never writes new fields into that format: a judgement
is *inferred* from the commit sequence, which is lower confidence than a
recorded judgement and is stated as such in every report's provenance.

Detection: between two adjacent non-empty commits, a trigger key is a named
BackSpace, a chord (modifier bits beyond Shift), or any other named key. If the
two commits are similar (SequenceMatcher ratio >= --threshold) and share a
script, that is a suspected rework episode, tagged by the trigger kind:

  backspace-retype   plain repair (ReqMode11/12 pricing)
  chord-retype       autoswitch-off / Ctrl+HanEng correction (ReqMode7)
  named-key-retype   a mode or boundary key intervened

Provenance follows AGENTS.md: the tool writes meta.json beside its report,
carrying the source path, its sha256, the method, the threshold, the exclusions
and what the artefact does not represent. No corpus path is hardcoded.

    tools/typing-rework-analyze.py <trace.jsonl> --out <report.jsonl>
    tools/typing-rework-analyze.py --selfcheck

The report's meta.json is written beside it as <report>.meta.json.
"""
import argparse
import datetime
import difflib
import hashlib
import json
import pathlib
import sys

HUNMIN_KEY_TEXT = 0
BACKSPACE_BASE = 0x08
HUNMIN_MOD_CHORD = 0b1110


def script_of(text):
    hangul = latin = 0
    for ch in text:
        o = ord(ch)
        if 0xAC00 <= o <= 0xD7A3 or 0x1100 <= o <= 0x11FF:
            hangul += 1
        elif ("a" <= ch <= "z") or ("A" <= ch <= "Z"):
            latin += 1
    if hangul and not latin:
        return "hangul"
    if latin and not hangul:
        return "latin"
    return "mixed"


def trigger_kind(event):
    inp = event.get("in")
    if inp is None:
        return None
    base = inp.get("base", 0)
    kid = inp.get("id", HUNMIN_KEY_TEXT)
    mods = inp.get("mods", 0) or 0
    if kid == HUNMIN_KEY_TEXT and base == BACKSPACE_BASE:
        return "backspace-retype"
    if mods & HUNMIN_MOD_CHORD:
        return "chord-retype"
    if kid != HUNMIN_KEY_TEXT:
        return "named-key-retype"
    return None


def read_trace(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise ValueError("empty trace")
    first = json.loads(lines[0])
    if "meta" not in first:
        raise ValueError("first line must carry meta (docs/trace-spec.md)")
    return first["meta"], [json.loads(line) for line in lines[1:] if line.strip()]


def analyze(meta, events, threshold):
    commits = []
    pending = []
    for i, event in enumerate(events):
        if "flush" in event:
            text = event.get("flush", "") or ""
            if text:
                commits.append((i, text))
            continue
        kind = trigger_kind(event)
        if kind:
            pending.append((i, kind))
        commit = event.get("commit", "") or ""
        if commit:
            commits.append((i, commit))
    episodes = []
    for (prev_i, prev_text), (next_i, next_text) in zip(commits, commits[1:]):
        between = [(i, k) for i, k in pending if prev_i < i <= next_i]
        if not between:
            continue
        if script_of(prev_text) != script_of(next_text):
            continue
        ratio = difflib.SequenceMatcher(None, prev_text, next_text).ratio()
        if ratio < threshold:
            continue
        kinds = sorted({k for _, k in between})
        episodes.append({
            "trigger": kinds,
            "before": prev_text,
            "after": next_text,
            "similarity": round(ratio, 3),
            "script": script_of(prev_text),
            "event_index": prev_i,
        })
    return episodes


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def provenance(source, meta, threshold, episode_count):
    return {
        "source": str(source),
        "source_sha256": sha256(source),
        "platform": meta.get("platform", ""),
        "layout": meta.get("layout", ""),
        "method": "adjacent non-empty commits with a named trigger key between them",
        "threshold": threshold,
        "episode_count": episode_count,
        "created": datetime.date.today().isoformat(),
        "excluded": ["events with no commit", "commit pairs with no intervening trigger key", "cross-script pairs"],
        "does_not_represent": [
            "a recorded judgement — rework is inferred from commit text, not observed",
            "all rework — only adjacent commit pairs in this trace are examined",
        ],
    }


def write_report(out_path, source, meta, episodes, threshold):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps({"meta": provenance(source, meta, threshold, len(episodes))}, ensure_ascii=False)]
    lines += [json.dumps(e, ensure_ascii=False) for e in episodes]
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    meta_path = out_path.with_suffix(".meta.json")
    meta_path.write_text(
        json.dumps(provenance(source, meta, threshold, len(episodes)), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return meta_path


def selfcheck():
    import tempfile

    sample = {
        "meta": {"platform": "android", "layout": "sin3-p2", "case": "selfcheck", "dialect": "personal-key"},
        "events": [
            {"in": {"base": 111, "shift": 0, "mods": 0, "id": 0}, "char": "o", "commit": "opencode", "preedit": "", "consumed": True},
            {"in": {"base": 8, "shift": 0, "mods": 0, "id": 0}, "commit": "", "preedit": "", "consumed": False},
            {"in": {"base": 111, "shift": 0, "mods": 0, "id": 0}, "char": "o", "commit": "opencode", "preedit": "", "consumed": True},
            {"in": {"base": 104, "shift": 0, "mods": 0, "id": 0}, "char": "h", "commit": "hello", "preedit": "", "consumed": True},
            {"in": {"base": 104, "shift": 0, "mods": 0b1110, "id": 0}, "char": "h", "commit": "hello", "preedit": "", "consumed": False},
            {"in": {"base": 104, "shift": 0, "mods": 0, "id": 0}, "char": "h", "commit": "hello", "preedit": "", "consumed": True},
            {"in": {"base": 100, "shift": 0, "mods": 0, "id": 0}, "char": "d", "commit": "다른", "preedit": "", "consumed": True},
            {"in": {"base": 8, "shift": 0, "mods": 0, "id": 0}, "commit": "", "preedit": "", "consumed": False},
            {"in": {"base": 100, "shift": 0, "mods": 0, "id": 0}, "char": "d", "commit": "完全不同", "preedit": "", "consumed": True},
            {"flush": "flushed-word"},
        ],
    }
    with tempfile.TemporaryDirectory() as d:
        trace = pathlib.Path(d) / "in.trace"
        trace.write_text(
            "\n".join(json.dumps(x, ensure_ascii=False) for x in [{"meta": sample["meta"]}, *sample["events"]]) + "\n",
            encoding="utf-8",
        )
        meta, events = read_trace(trace)
        episodes = analyze(meta, events, 0.5)
        kinds = [e["trigger"] for e in episodes]
        assert ["backspace-retype"] in kinds, kinds
        assert ["chord-retype"] in kinds, kinds
        assert len(episodes) == 2, episodes
        out = pathlib.Path(d) / "report.jsonl"
        mp = write_report(out, trace, meta, episodes, 0.5)
        report = [json.loads(x) for x in out.read_text(encoding="utf-8").splitlines()]
        assert report[0]["meta"]["source_sha256"] == sha256(trace)
        assert mp.is_file()
        original = trace.read_text(encoding="utf-8")
        assert len(original.splitlines()) == len(sample["events"]) + 1
    print("ok: typing-rework-analyze selfcheck")
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("trace", nargs="?", help="personal trace JSONL")
    ap.add_argument("--out", help="report JSONL; <name>.meta.json is written beside it")
    ap.add_argument("--threshold", type=float, default=0.5)
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args(argv)
    if args.selfcheck:
        return selfcheck()
    if not args.trace or not args.out:
        ap.error("trace and --out are required unless --selfcheck is given")
    source = pathlib.Path(args.trace)
    meta, events = read_trace(source)
    episodes = analyze(meta, events, args.threshold)
    mp = write_report(pathlib.Path(args.out), source, meta, episodes, args.threshold)
    print(f"{len(episodes)} episode(s); provenance: {mp}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
