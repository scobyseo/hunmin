#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier C gate: replay a platform-native trace through a frontend and diff it
against the expectation tier A implies (docs/trace-spec.md).

    tools/trace-check.py <case>.trace.in [--emit [--accept-changed]] [--runner <cmd>]

Exit 0 diff-clean, 1 mismatch, 77 skip (frontend not built for this platform).

`--emit` writes the `.gold` from the runner's own output rather than comparing.
That is the "make retrace" path: it must never be the way a red test becomes
green, so it is a mechanism, not a request (issue #306): when the runner's
output differs from the committed `.gold`, `--emit` prints the unified diff,
leaves the file untouched and exits 1. Overwriting a changed expectation
requires `--emit --accept-changed`, so every expectation change lands in a
commit as a reviewed diff. A `.gold` that does not exist yet is written
directly -- there is no prior expectation to re-baseline. Review the diff
line by line.

Two things are checked, and the second is the one that matters:

1. the frontend's trace matches the expectation, and
2. the expectation still matches the tier-A golden it was generated from.

Check 2 is why this is not just "run the adapter and eyeball it". A stale `.gold`
-- one left over from before an engine change -- would make check 1 pass forever.
"""
import argparse
import difflib
import json
import os
import pathlib
import shlex
import subprocess
import sys

from _hostcompat import posix_shell

ROOT = pathlib.Path(__file__).resolve().parent.parent
SIM_DATA = ROOT / "libhunmin" / "data"


def find_sim(explicit: pathlib.Path | None) -> pathlib.Path:
    """The simulator this checker asks about a word-buffered case's deliveries."""
    if explicit is not None:
        if not explicit.exists():
            raise SystemExit(f"--sim {explicit} does not exist")
        return explicit
    for pattern in ("build/hunmin-sim", "build-*/hunmin-sim", "build*/hunmin-sim"):
        hits = sorted(ROOT.glob(pattern))
        if hits:
            return hits[0]
    raise SystemExit(
        "no hunmin-sim to ask: configure and build the tree, or pass --sim PATH. "
        "A word-buffered case's deliveries are the layer's own answers and cannot be "
        "derived from tier A (docs/OPTIONS-MEASURED.md §PLT-8).")


def load(path: pathlib.Path) -> list[dict]:
    recs = []
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            recs.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"{path}:{n}: {e}")
    if not recs or "meta" not in recs[0]:
        raise SystemExit(f"{path}: line 1 must be a meta object (trace-spec.md)")
    return recs


def normalise(recs: list[dict]) -> list[dict]:
    """Comparable projection: only what the spec defines as observable."""
    out = []
    for r in recs:
        if "meta" in r:
            continue
        extra = set(r) - {"in", "commit", "preedit", "consumed"}
        if extra:
            raise SystemExit(
                f"trace record has fields the spec does not define: {sorted(extra)}"
            )
        out.append(
            {
                "in": r.get("in"),
                "commit": r.get("commit", ""),
                "preedit": r.get("preedit", ""),
                "consumed": r.get("consumed"),
            }
        )
    return out


def check_invariants(recs: list[dict], path: pathlib.Path) -> None:
    """The invariants in docs/trace-spec.md that are checkable from the file alone.

    A trace that breaks one is a bad test, not a failing adapter -- caught here so
    the diagnosis is not misattributed to the frontend. Note that commit and
    preedit are deliberately NOT checked for mutual exclusion: tier A's goldens
    have both non-empty on 700+ lines, and the spec records that correction.
    """
    events = normalise(recs)
    if not events:
        raise SystemExit(f"{path}: no events")
    for n, e in enumerate(events, 1):
        if "in" not in e:
            raise SystemExit(f"{path}: event {n} has no `in`")
        if e["consumed"] is None:
            raise SystemExit(f"{path}: event {n} has no `consumed`")


def project_word_buffered(declared: list[dict]) -> list[dict]:
    """The expectation a WORD-BUFFERED frontend produces, from tier A's stream.

    Tier A records the engine's answer per key: a syllable finalises on the key
    that starts the next one, so `commit` appears mid-word. A word-buffering
    adapter hands the client nothing until the eojeol ends, so no per-key commit
    of tier A is what is delivered and the two models cannot be compared
    verbatim (measured: fcitx5 shows the whole buffered word in `preedit`,
    `안녕` progressing ㅇ → 아 → 안 → 안ㄴ → 안녀 → 안녕).

    The two models do agree on the derived quantities, and those are what this
    computes -- never what the answer "should" be, only what tier A's own
    numbers imply under the adapter's documented policy:

      * non-boundary event: nothing is delivered; the preedit is the word
        accumulated so far, then the engine's in-flight buffer;
      * boundary event (`consumed` false): the accumulated word is delivered
        in one commit and the buffer starts empty.

    What is NOT derivable is delivery at teardown (a case whose keys stop
    mid-word): the runner flushes it with no event to report it, so it is
    covered by the cumulative total instead of by a per-event expectation.
    """
    out = []
    word = ""
    for e in declared:
        word += e["commit"] or ""
        if e["consumed"]:
            out.append({"in": e["in"], "commit": "", "preedit": word + (e["preedit"] or ""),
                        "consumed": True})
        else:
            out.append({"in": e["in"], "commit": word, "preedit": e["preedit"] or "",
                        "consumed": False})
            word = ""
    return out


def layer_stream(recs: list[dict], sim: pathlib.Path) -> list[dict]:
    """What the SHARED LAYER delivers, key by key, in the adapter's own policy.

    This replaces a rule over tier A's stream, and the reason is measured
    (docs/OPTIONS-MEASURED.md §PLT-8): tier A records the per-syllable engine
    API, so "commit non-empty and preedit empty" means both "a symbol ended the
    word" (delivered now) and "a digit was committed" (not a delivery point at
    all). The layer that every frontend drives answers the question directly --
    it commits on the key the word ends on, which is the key the client is owed
    text at. So the expectation is obtained by replaying the case through
    `hunmin-sim --layer word`, not by projecting anything.

    The key string is reconstructed from the trace's own keysyms: ASCII keys are
    themselves, KEY_BackSpace is the `\\b` escape `hunmin-sim --keys` understands.
    Any other non-ASCII keysym is refused rather than guessed.
    """
    keys = []
    for e in recs:
        if "meta" in e:
            continue
        ks = (e.get("in") or {}).get("keysym")
        if ks is None:
            raise SystemExit("trace has an event without in.keysym; cannot replay it")
        if ks == 0xFF08:  # XK_BackSpace
            keys.append("\\b")
        elif 32 <= ks < 127:
            keys.append(chr(ks))
        else:
            raise SystemExit(
                f"keysym {ks} has no --keys spelling; extend layer_stream() before "
                "relying on this trace")
    layout = (recs[0].get("meta") or {}).get("layout")
    if not layout:
        raise SystemExit("trace meta has no layout; cannot replay it")
    proc = subprocess.run(
        [str(sim), "--data", str(SIM_DATA), "--layout", layout,
         "--keys", "".join(keys), "--trace", "--layer", "word"],
        capture_output=True, text=True, encoding="utf-8",
    )
    if proc.returncode != 0:
        raise SystemExit(f"hunmin-sim --layer word failed rc={proc.returncode}: {proc.stderr.strip()}")
    rows = [json.loads(line) for line in proc.stdout.splitlines() if line.strip().startswith("{")]
    ins = [e.get("in") for e in recs if "meta" not in e]
    out = []
    for n, row in enumerate(rows):
        # The trace's own native event, so a diff message names the key the reader
        # is looking at; the sim emits its own `in.ch` and they are the same event.
        out.append({"in": ins[n], "commit": row.get("commit") or "",
                    "preedit": row.get("preedit") or "",
                    "consumed": row.get("consumed")})
    if len(out) != len(recs) - 1:
        raise SystemExit(
            f"hunmin-sim replayed {len(out)} events, the trace has {len(recs) - 1}")
    return out


def cumulative_text(events: list[dict]) -> str:
    """Everything the case puts in the client: every commit + the live preedit.

    The preedit is included because it is delivered when the word is flushed
    (on a boundary key or at teardown), so the document the client ends up with
    is [commits][preedit]. On a trace that ends at a boundary the preedit is
    empty and this is just the sum of the commits.
    """
    return "".join(e["commit"] or "" for e in events) + (events[-1]["preedit"] or "" if events else "")


DELIVERY_MODELS = ("word-buffered",)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("trace", type=pathlib.Path)
    ap.add_argument("--emit", action="store_true")
    ap.add_argument(
        "--accept-changed",
        action="store_true",
        help="with --emit: overwrite a .gold whose content differs from the "
             "runner's output (the diff is printed either way); without this "
             "flag a changed expectation is a refusal, not a write",
    )
    ap.add_argument(
        "--runner",
        default="frontends/fcitx5/build/trace-emit",
        help="command that reads a .trace.in on stdin and writes a trace on stdout",
    )
    ap.add_argument(
        "--sim",
        type=pathlib.Path,
        default=None,
        help="hunmin-sim to ask for a word-buffered case's deliveries "
             "(default: build/hunmin-sim, build-*/hunmin-sim)",
    )
    a = ap.parse_args()
    if a.accept_changed and not a.emit:
        ap.error("--accept-changed only means something with --emit")

    recs = load(a.trace)
    check_invariants(recs, a.trace)

    # Cross-platform spawn: CMake passes --runner as one argv token, which may
    # be a .sh script. POSIX hosts exec it directly; Windows' CreateProcess
    # cannot run a shell script, so the runner is wrapped in a located bash
    # (whose usr/bin goes first in the child's PATH for find/paste/realpath).
    # Any OS failure to spawn becomes the documented skip: "runner not built
    # for this platform" includes "cannot be executed on this platform".
    tokens = shlex.split(a.runner, posix=(os.name != "nt"))
    bashp = None
    if os.name == "nt" and tokens and tokens[0].lower().endswith(".sh"):
        bashp = posix_shell()
        if bashp is None:
            print(f"skip: no POSIX shell to run {a.runner!r}", file=sys.stderr)
            return 77
        argv = [bashp, tokens[0]]
    else:
        argv = tokens
    delivery = (recs[0].get("meta") or {}).get("delivery")
    projected = None
    if delivery is not None:
        projected = layer_stream(recs, find_sim(a.sim))

    env = dict(os.environ)
    if bashp:
        env["PATH"] = os.path.dirname(bashp) + os.pathsep + env["PATH"]
    if projected is not None:
        # The runner's TestFrontend turns an expectation queue into its only
        # channel for observing commits, so the queue must be the same expectation
        # this checker holds -- otherwise the two verify different things and a
        # wrong expectation passes silently (the defect this file's header names).
        # One entry per EVENT (empty where nothing is delivered), because the
        # runner echoes this list into the trace it writes -- handing it only the
        # non-empty commits would leave the runner free to place them by its own
        # rule, which is the attribution this change exists to fix.
        env["HUNMIN_TRACE_EXPECT"] = json.dumps(
            [p["commit"] for p in projected], ensure_ascii=False)

    try:
        proc = subprocess.run(
            argv + [str(a.trace)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            env=env,
        )
    except OSError as e:
        print(f"skip: cannot run {a.runner!r}: {e}", file=sys.stderr)
        return 77
    if proc.returncode == 77:
        print("skip: runner reports the frontend is not built", file=sys.stderr)
        return 77
    if proc.returncode != 0:
        print(proc.stdout + proc.stderr, file=sys.stderr)
        print(f"runner failed rc={proc.returncode}", file=sys.stderr)
        return 1

    gold = a.trace.with_suffix("").with_suffix(".trace.gold")
    got = load_text_as_records(proc.stdout, a.trace)

    if a.emit:
        text = proc.stdout if proc.stdout.endswith("\n") else proc.stdout + "\n"
        if gold.exists():
            old = gold.read_text(encoding="utf-8")
            if old == text:
                print(f"unchanged: {gold}", file=sys.stderr)
                return 0
            # A silent re-baseline is the cheapest way to turn a red gate
            # green, and then the gate asserts that the tool agrees with
            # itself (issue #306). Print the diff and refuse; writing a
            # changed expectation is an explicit act.
            sys.stderr.write("".join(difflib.unified_diff(
                old.splitlines(keepends=True), text.splitlines(keepends=True),
                fromfile=str(gold), tofile=f"{gold} (runner output)")))
            if not a.accept_changed:
                print(f"{gold} differs from the runner's output -- review the diff "
                      "above; the file is unchanged. To accept it, re-run with "
                      "--emit --accept-changed", file=sys.stderr)
                return 1
        gold.write_text(text, encoding="utf-8")
        print(f"wrote {gold} -- review the diff, do not blind-accept", file=sys.stderr)
        return 0

    if not gold.exists():
        print(f"missing {gold}; generate with --emit and review it", file=sys.stderr)
        return 1

    want = normalise(load(gold))
    have = normalise(got)

    # The .in carries its OWN expectations, copied verbatim from tier A. If they
    # disagree with the .gold, the runner is replaying an input it is no longer
    # being asked to satisfy -- and on the commit path TestFrontend asserts the
    # .in's value, so the two are verified against DIFFERENT things and a wrong
    # expectation in the .in passes silently. Comparing .in against .gold is
    # what makes the pair self-consistent; only then does adapter-vs-gold mean
    # anything. Found by mutating a preedit in the .in and watching it pass.
    declared = normalise(recs)

    delivery = (recs[0].get("meta") or {}).get("delivery")
    if delivery is not None:
        # A word-buffered frontend is the one case where .in and .gold are not
        # meant to be equal: tier A's commit is the engine finalising a syllable
        # mid-word, and the client sees the whole eojeol at the boundary. So
        # both sides are projected through the model and the *totals* are
        # compared, which is the claim neither model can dodge (issue #220).
        model = delivery.get("model") if isinstance(delivery, dict) else None
        if model not in DELIVERY_MODELS:
            raise SystemExit(
                f"{a.trace}: meta.delivery.model {model!r} is not one of "
                f"{list(DELIVERY_MODELS)}; refusing to guess what the runner does"
            )
        total = delivery.get("total")
        if not isinstance(total, str) or not total:
            raise SystemExit(
                f"{a.trace}: meta.delivery.total must be a non-empty string "
                "(tools/gen-conformance.py --delivery computes it from the golden)"
            )
        # Leg 1: the generator's number is the golden's, not somebody's typing.
        from_in = cumulative_text(declared)
        if from_in != total:
            print(f"{a.trace}: meta.delivery.total says {total!r}, but the .in's "
                  f"tier-A stream accumulates {from_in!r} -- regenerate with --emit",
                  file=sys.stderr)
            return 1
        # Leg 2: the committed .gold is what the shared layer delivers on the same
        # keys, event for event. A pair that disagrees about what it is asserting
        # verifies nothing (same rule as the branch below).
        assert projected is not None  # computed above whenever a delivery model exists
        drift = [(n, p, g) for n, (p, g) in enumerate(zip(projected, want), 1)
                 if (p["commit"], p["preedit"], p["consumed"]) !=
                    (g["commit"], g["preedit"], g["consumed"])]
        if drift or len(projected) != len(want):
            for n, p, g in drift[:3]:
                print(f"event {n}: the model says {p}\n         .gold says {g}", file=sys.stderr)
            if len(projected) != len(want):
                print(f"length: model {len(projected)}, .gold {len(want)}", file=sys.stderr)
            print(f"{a.trace}: the .gold is not what the shared layer delivers on these "
                  "keys (hunmin-sim --layer word) -- regenerate with --emit and review",
                  file=sys.stderr)
            return 1
        # Leg 3: the frontend delivered exactly that text, no byte dropped or
        # duplicated. The runner asserts each commit it was given, but only a
        # total says nothing went missing at a boundary or at teardown.
        for label, events in ((".gold", want), ("runner", have)):
            got_total = cumulative_text(events)
            if got_total != total:
                print(f"{a.trace}: {label} delivered {got_total!r}, the case's "
                      f"cumulative text is {total!r}", file=sys.stderr)
                return 1
    else:
        drift = [(n, d, g) for n, (d, g) in enumerate(zip(declared, want), 1) if d != g]
        if drift or len(declared) != len(want):
            for n, d, g in drift[:3]:
                print(f"event {n}: .in says {d}\n         .gold says {g}", file=sys.stderr)
            if len(declared) != len(want):
                print(f"length: .in has {len(declared)}, .gold has {len(want)}", file=sys.stderr)
            print(f"{a.trace}: .in and .gold disagree -- regenerate with --emit",
                  file=sys.stderr)
            return 1

    if want == have:
        print(f"ok: {a.trace.name} ({len(have)} events)")
        return 0

    for n, (w, h) in enumerate(zip(want, have), 1):
        if w != h:
            print(f"event {n}: expected {w}\n         got      {h}", file=sys.stderr)
    if len(want) != len(have):
        print(f"length: expected {len(want)}, got {len(have)}", file=sys.stderr)
    return 1


def load_text_as_records(text: str, origin: pathlib.Path) -> list[dict]:
    recs = []
    for n, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            recs.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"{origin}: runner emitted bad JSON line {n}: {e}")
    return recs


if __name__ == "__main__":
    sys.exit(main())