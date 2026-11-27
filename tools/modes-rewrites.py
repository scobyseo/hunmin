#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Collect the words the automatic switch REWRITES, from a person's own typing.

Docs: `docs/MILESTONES.md` §0.4 Track 2 (purpose: find the design, not grade it),
`docs/MODES-MEASUREMENT.md` §1.5/§2.5, `docs/SPEC-MODES.md` ReqMode8/ReqMode10.

WHY THIS EXISTS. The gates count; this lists. "The feature costs keystrokes"
is an aggregate, and an aggregate cannot be looked at, argued with, or turned
into a test case — the maintainer asked for the concrete instances where the
automatic switch replaces what they typed ("자동 변환으로 재작성 하는 사례"). So
this tool drives the shipped layer over a text the person actually produced and
emits one record per **rewritten token**: the line it came from, what was typed,
what the engine delivered instead, and the judgement it acted on.

WHAT IT RECORDS. Per mismatch: `line`, `token`, `script`, `got`, `judged`, `keys`.
A rewrite is `delivered != typed` for one eojeol — the same comparison
`tools/modes-metrics.py` makes for its `correct` flag, so the two cannot disagree
about what "rewritten" means.

THE START MODE IS THE MODEL. A person typing a shell command is in **Latin** mode;
a person typing Korean prose is in **Hangul** mode. Which mode they were in is not
observable from the text, so the caller states it (`--mode`), and the run records
it in the artefact. That is the one difference from `modes-metrics.py`, whose
replay always starts in Hangul because it models a sentence whose first word is
Korean.

THE LAYER IS THE ORACLE, as everywhere in this tree: composition, the judgement
and the switch come from the built library through the same `Layer` object
`modes-metrics.py` uses. Nothing here re-implements a composer or a jamo map.

WHAT IT DOES NOT DO. It does not price anything (no keystroke arithmetic — that
is `modes-metrics.py`'s job), it does not replay a repair, and it does not judge
whether a rewrite is *wrong*: a rewrite of English→Latin (a command typed in
Hangul mode) is correct behaviour, and the record says which way it went so the
reader decides. Punctuation a layout does not map is declined and forwarded by
the replay, which ends the word in flight — a real person would have pressed a
mapped key for it, so a line with such characters can produce a rewrite that the
person would not have seen. `--ascii-only` drops those lines instead.

USAGE
  # a person's shell history (zsh extended or bash), commands typed in Latin mode
  tools/modes-rewrites.py --history ~/.zsh_history --mode latin \\
      --out ~/work/hunmin-measurements/rewrites-shell.jsonl
  # plain text, one line per sentence, e.g. the maintainer's own Korean quotes
  tools/modes-rewrites.py --corpus /tmp/ko-quotes.txt --mode hangul --out ...
  tools/modes-rewrites.py --selfcheck         # the rewrite detector's own guards

Exit: 0 always for a completed collection (a finding is not a failure);
2 for a usage or environment error (no library, no keytable, unreadable corpus).
"""
import argparse
import collections
import importlib.util
import json
import pathlib
import re
import sys

try:
    from _common import ROOT as REPO
except ImportError:
    REPO = pathlib.Path(__file__).resolve().parents[1]

HANGUL, LATIN, OTHER = "hangul", "latin", "other"

# Secret-shaped strings that must not leave the machine in an artefact or on a
# terminal. The patterns are deliberately broad: a false positive costs one
# masked command, a false negative leaks a token into a file.
SECRETS = [
    re.compile(r"(gh[pousr]_[A-Za-z0-9]{16,})"),
    re.compile(r"(github_pat_[A-Za-z0-9_]{20,})"),
    re.compile(r"(sk-[A-Za-z0-9-]{16,})"),
    re.compile(r"(xox[baprs]-[A-Za-z0-9-]{10,})"),
    re.compile(r"(AKIA[0-9A-Z]{12,})"),
    re.compile(r"(?i)((?:api[_-]?key|token|password|passwd|secret)=)(\S+)"),
    re.compile(r"(?i)(--(?:token|password|passwd|secret)[ =])(\S+)"),
    re.compile(r"\b([0-9a-f]{32,})\b"),
]


def redact(text):
    """Mask secret-shaped substrings; returns (text, count)."""
    n = 0
    out = text
    for pat in SECRETS:
        def sub(m):
            nonlocal n
            n += 1
            if m.lastindex and m.lastindex >= 2 and m.group(1):
                return m.group(1) + "<redacted>"
            return "<redacted>"
        out = pat.sub(sub, out)
    return out, n


def load(name):
    path = pathlib.Path(__file__).resolve().parent / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_corpus(path):
    """(line_number, text, redactions) from a plain-text or JSONL corpus."""
    rows, masked = [], 0
    raw = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    if str(path).endswith(".jsonl"):
        for i, line in enumerate(raw.splitlines(), 1):
            if not line.strip():
                continue
            rec = json.loads(line)
            if "text" not in rec:
                continue
            text, n = redact(str(rec["text"]))
            masked += n
            rows.append((i, text.strip()))
        return rows, masked
    for i, line in enumerate(raw.splitlines(), 1):
        text, n = redact(line)
        masked += n
        if text.strip():
            rows.append((i, text.strip()))
    return rows, masked


def read_history(path):
    """Commands from a zsh EXTENDED_HISTORY file (`: <ts>:<dur>;<cmd>`) or bash.

    Multi-line commands are joined on the trailing backslash, because a person
    typed one command and the shell wrote several lines; joining is what makes
    the replay see the same key run. Escaped newlines inside zsh's `\\` form are
    the only continuation this handles, which is what `setopt` writes by default.
    """
    rows, masked = [], 0
    pending = ""
    pending_no = 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            line = line.rstrip("\n")
            # zsh EXTENDED_HISTORY writes ": <start>:<elapsed>;<command>".
            if line.startswith(": ") and ";" in line:
                line = line.split(";", 1)[1]
            if not pending:
                pending_no = i
            if line.endswith("\\"):
                pending += line[:-1] + " "
                continue
            text, n = redact(pending + line)
            masked += n
            pending = ""
            if text.strip() and not text.startswith("#"):
                rows.append((pending_no, text.strip()))
    if pending:
        text, n = redact(pending)
        masked += n
        rows.append((pending_no, text.strip()))
    return rows, masked


def drive(layer, plan, mode, rewrites, line_no, line, max_keys=None):
    """Type one planned line and append a record per rewritten eojeol."""
    if layer.language() != mode:
        layer.toggle()
    typed_tokens = 0
    keys = 0
    for item in plan:
        token, script, events = item["token"], item["script"], item["events"]
        if script == OTHER:
            continue
        commits, forwarded, judged = [], [], []
        for base, shift in events + [item["space"]]:
            consumed, commit, _ = layer.key(base, shift)
            keys += 1
            if commit:
                commits.append(commit)
            if not consumed:
                forwarded.append(chr(base))
            j = layer.judgement()
            if j >= 0:
                judged.append(HANGUL if j == 0 else LATIN)
                sw = layer.toggle()  # the frontend performs the switch
                if sw:
                    commits.append(sw)
        typed_tokens += 1
        got = "".join(commits)
        chunk = got + "".join(forwarded)
        if chunk != token + " ":
            rewrites.append({
                "line": line_no, "token": token, "script": script,
                "got": got.rstrip(), "judged": judged, "keys": len(events) + 1,
                "start_mode": mode, "text": line,
            })
    return typed_tokens, keys


def collect(args):
    mm = load("modes-metrics")
    table_blob = mm.load_keytable(args.layout, args.keytable, args.sim, args.data)
    table, literals = table_blob["table"], table_blob.get("literals", {})
    engine = mm.Engine(args.lib, args.data)
    layer = engine.layer(args.layout, args.latin, 1)
    if args.history:
        rows, masked = read_history(args.history)
        source = {"kind": "history", "path": str(args.history)}
    else:
        rows, masked = read_corpus(args.corpus)
        source = {"kind": "corpus", "path": str(args.corpus)}
    if args.ascii_only:
        rows = [(n, t) for n, t in rows if t.isascii()]
    rewrites, tokens, keys, not_inputtable = [], 0, 0, 0
    for line_no, line in rows[:args.limit] if args.limit else rows:
        plan = mm.plan_sentence(line, table, literals)
        if any(item["missing"] for item in plan):
            not_inputtable += 1  # a syllable the layout cannot type
            continue
        t, k = drive(layer, plan, args.mode, rewrites, line_no, line)
        tokens += t
        keys += k
    layer.close()
    return {
        "provenance": {
            "tool": "tools/modes-rewrites.py",
            "source": source,
            "version": {"mtime": pathlib.Path(source["path"]).stat().st_mtime},
            "lines_read": len(rows), "redactions": masked,
            "model": {"layout": args.layout, "latin": args.latin,
                      "start_mode": args.mode, "autoswitch": 1,
                      "ascii_only": bool(args.ascii_only),
                      "quote": "each rewritten eojeol is delivered != typed; the "
                               "layer is the oracle, nothing is re-implemented"},
        },
        "counts": {"lines": len(rows), "tokens": tokens, "keys": keys,
                   "rewritten_tokens": len(rewrites),
                   "lines_not_inputtable": not_inputtable},
        "rewrites": rewrites,
    }


def summary(record):
    c = record["counts"]
    rw = record["rewrites"]
    by_script = collections.Counter(r["script"] for r in rw)
    by_judged = collections.Counter(",".join(r["judged"]) or "none" for r in rw)
    top = collections.Counter(r["token"] for r in rw)
    print(f"source: {record['provenance']['source']['kind']} "
          f"{record['provenance']['source']['path']} "
          f"(start mode {record['provenance']['model']['start_mode']}, "
          f"layout {record['provenance']['model']['layout']})")
    print(f"lines {c['lines']}  tokens {c['tokens']}  keys {c['keys']}  "
          f"not inputtable {c['lines_not_inputtable']}  "
          f"redactions {record['provenance']['redactions']}")
    print(f"REWRITTEN tokens {c['rewritten_tokens']} "
          f"({100 * c['rewritten_tokens'] / max(1, c['tokens']):.2f}% of tokens)")
    print(f"  by typed script: {dict(by_script)}")
    print(f"  by judgement acted on: {dict(by_judged)}")
    print(f"  top rewritten tokens: {top.most_common(12)}")


def selfcheck():
    """The detector must fire on a rewrite and stay silent on a clean line."""
    mm = load("modes-metrics")
    data = REPO / "libhunmin" / "data"
    lib = REPO / "build" / "libhunmin.so"
    problems = []
    if not lib.exists():
        print("SELFCHECK SKIP: build/libhunmin.so not built", file=sys.stderr)
        return 77
    engine = mm.Engine(lib, data)
    # Latin mode, both tokens: `dkssud` is not a word and its Hangul reading is
    # 안녕, so the engine replaces it — the shell-command case. `hello` is a word,
    # so it is kept: the control that a clean line records nothing.
    layer = engine.layer("dubeolsik", "us-qwerty", 1)
    rewrites = []
    plan = mm.plan_sentence("dkssud", None)
    drive(layer, plan, LATIN, rewrites, 1, "dkssud")
    layer.reset()
    rewrites_clean = []
    plan = mm.plan_sentence("hello", None)
    drive(layer, plan, LATIN, rewrites_clean, 2, "hello")
    layer.close()
    if (len(rewrites) != 1 or rewrites[0]["got"] != "안녕"
            or rewrites[0]["judged"] != [HANGUL]):
        problems.append(f"the Hangul rewrite was not detected: {rewrites}")
    if rewrites_clean:
        problems.append(f"a kept English word was reported as rewritten: {rewrites_clean}")
    if problems:
        print("SELFCHECK FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print("OK  a Latin token rewritten to Hangul is recorded with its judgement, "
          "a kept English word is not")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--history", type=pathlib.Path,
                     help="zsh EXTENDED_HISTORY or bash history (redacted)")
    src.add_argument("--corpus", type=pathlib.Path,
                     help="plain text (one sentence per line) or JSONL with a text field")
    ap.add_argument("--mode", choices=(HANGUL, LATIN), default=None,
                    help="the mode the person was typing in (the model's only free "
                         "parameter; recorded in the artefact)")
    ap.add_argument("--layout", default="sin3-p2-basic")
    ap.add_argument("--latin", default="us-qwerty")
    ap.add_argument("--data", type=pathlib.Path, default=REPO / "libhunmin" / "data")
    ap.add_argument("--lib", type=pathlib.Path, default=REPO / "build" / "libhunmin.so")
    ap.add_argument("--sim", type=pathlib.Path, default=REPO / "build" / "hunmin-sim")
    ap.add_argument("--keytable", type=pathlib.Path, default=None)
    ap.add_argument("--ascii-only", action="store_true",
                    help="drop lines with characters outside ASCII (the replay "
                         "declines those keys, which ends a word a person would not "
                         "have ended)")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--out", type=pathlib.Path, help="write the record as JSON here")
    ap.add_argument("--show", type=int, default=5,
                    help="print this many example rewrites (redacted)")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck()
    if not args.history and not args.corpus:
        ap.error("need --history FILE or --corpus FILE (both are arguments, never defaults)")
    if not args.mode:
        ap.error("--mode hangul|latin: the mode the person was tapping in")
    if not pathlib.Path(args.lib).exists():
        ap.error(f"{args.lib} not found — build first (cmake -B build && cmake --build build)")
    record = collect(args)
    summary(record)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, ensure_ascii=False, indent=1) + "\n",
                            encoding="utf-8")
        print(f"{args.out}: {len(record['rewrites'])} rewrite record(s)")
    for r in record["rewrites"][:args.show]:
        print(f"  line {r['line']}: typed '{r['token']}' -> '{r['got']}' "
              f"(judged {','.join(r['judged']) or 'none'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())