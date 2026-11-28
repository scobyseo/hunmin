#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Grade G1/G2 from a `tools/modes-metrics.py` report; set no G2 pass line.

This is a checker, not a second harness. `tools/modes-metrics.py` already emits
the replay, the pool share, the manual-intervention share, the measured repair
cost and the wrong-direction counts (docs/MODES-MEASUREMENT.md §1.2/§1.4/§2).
This tool reads those numbers, verifies that the run used a pinned corpus, and
applies only the gates that are already ruled:

  * G1 -- `q_pool_share_measured >= 0` on the pinned corpus.
  * G2 -- report the stepwise K2 (`q`) and T3 (`u`) lines with the misfire
    budget implied by the run's own `k`: `r <= (1-u-q)/(2k+1)`. No number is
    graded unless the maintainer passes one with `--line Q U`.
  * wrong-direction guard -- zero switches against the typed script, broken out
    by direction. A rate is printed beside each count, but the guard itself is
    the predicate "no wrong-direction switch", not a numeric rate that this tool
    would be inventing.
  * G3 -- mid-word display flips reduced at an EQUAL pool share. Only graded when
    the caller names a baseline step with `--flip-baseline NAME`: every other
    step must carry the same corpus sha256, the same `q_pool_share_measured`
    (exactly -- a display change that moved the pool share is not the comparison
    G3 asks for), strictly fewer `flips.total`, no more `words_with_a_flip`, and
    zero `flips_below_the_guard`. Without a baseline it is reported, not graded.

The G2 arithmetic uses `q_pool_share_model` because that is the quantity the
budget formula is derived from; `q_pool_share_measured` is reported beside it
and is the quantity G1 grades. The tool fails when the two disagree with the
harness's own `a/r/u/k` identity, so a change in the model cannot quietly move
the budget line.

Usage
  tools/autoswitch-gates.py pin --corpus CORPUS --out PIN [--name NAME]
  tools/autoswitch-gates.py check --step NAME REPORT --pin PIN
                                  [--line Q U]... [--flip-baseline NAME]
                                  [--report-out PATH]
  tools/autoswitch-gates.py --selfcheck

`REPORT` is the `.meta.json` written by `modes-metrics.py --json-out` (a JSONL
run is also accepted). `PIN` is written by the `pin` subcommand and carries the
sha256 of the corpus and of its provenance, never the corpus itself: bulk
corpora stay outside the repository (AGENTS.md). Every path is an argument.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import sys

try:
    from _common import ROOT
except ImportError:
    ROOT = pathlib.Path(__file__).resolve().parents[1]

PIN_SCHEMA = "hunmin.modes-corpus-pin/1"
REPORT_SCHEMA = "hunmin.autoswitch-gates/1"
DIRECTIONS = ("hangul->latin", "latin->hangul")
EPS = 1e-9


def canonical(obj):
    """One byte string for a JSON object, however the file was pretty-printed."""
    return json.dumps(obj, sort_keys=True, ensure_ascii=False,
                      separators=(",", ":")).encode("utf-8")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path):
    text = pathlib.Path(path).read_text(encoding="utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        # A JSONL run from modes-metrics.py: instrument, corpus, cases..., totals.
        rows = [json.loads(line) for line in text.splitlines() if line.strip()]
        merged = {}
        for row in rows:
            if isinstance(row, dict):
                merged.update(row)
        required = ("instrument", "corpus", "totals")
        if all(k in merged for k in required):
            return merged
        raise


def provenance_for(corpus):
    """The meta object and its canonical hash, from the sibling meta or a JSONL head."""
    corpus = pathlib.Path(corpus)
    meta_path = corpus.with_suffix(corpus.suffix + ".meta.json")
    if meta_path.exists():
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
        return meta, sha256_bytes(canonical(meta)), meta_path
    if corpus.suffix == ".jsonl":
        for line in corpus.read_text(encoding="utf-8").splitlines():
            row = json.loads(line)
            if isinstance(row, dict) and "meta" in row:
                meta = row["meta"]
                return meta, sha256_bytes(canonical(meta)), meta_path
    raise SystemExit(f"{corpus}: no {meta_path.name} and no embedded meta line; "
                     "a figure without provenance is not evidence")


def make_pin(args):
    corpus = pathlib.Path(args.corpus)
    if not corpus.is_file():
        raise SystemExit(f"{corpus}: no such corpus -- pass the path as an argument")
    meta, meta_hash, meta_path = provenance_for(corpus)
    source = meta.get("source") or {}
    pin = {
        "schema": PIN_SCHEMA,
        "name": args.name or corpus.stem,
        "generated_by": "tools/autoswitch-gates.py pin",
        "generated_at": datetime.datetime.now(datetime.timezone.utc)
                                 .isoformat(timespec="seconds"),
        "corpus": {"basename": corpus.name, "sha256": sha256(corpus)},
        "provenance": {"meta_basename": meta_path.name,
                       "sha256": meta_hash,
                       "source_sha256": source.get("sha256")},
    }
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(pin, ensure_ascii=False, indent=2) + "\n",
                   encoding="utf-8")
    print(f"{out}: corpus {pin['corpus']['sha256'][:12]}… "
          f"provenance {pin['provenance']['sha256'][:12]}…")
    return 0


def check_pin(report, pin, pin_path, problems):
    """The run must name the pinned bytes and the pinned provenance."""
    rp = report.get("corpus") or {}
    in_report = rp.get("sha256")
    expected = (pin.get("corpus") or {}).get("sha256")
    if not expected:
        problems.append("pin has no corpus sha256")
    elif in_report != expected:
        problems.append(f"G1 precondition: corpus sha256 {in_report} is not pinned "
                        f"{expected}")
    cm = rp.get("meta") or {}
    meta_hash = sha256_bytes(canonical(cm))
    want_meta = (pin.get("provenance") or {}).get("sha256")
    if want_meta and meta_hash != want_meta:
        problems.append(f"G1 precondition: corpus provenance {meta_hash[:12]}… "
                        f"is not pinned {want_meta[:12]}…")
    src = (pin.get("provenance") or {}).get("source_sha256")
    if src and (cm.get("source") or {}).get("sha256") != src:
        problems.append("G1 precondition: the run's source sha256 differs from the pin")
    # When the pinned bytes are still on this host, check them too; a report that
    # merely repeats a hash is not the same as the file the hash names.
    corpus_path = rp.get("path")
    if corpus_path and pathlib.Path(corpus_path).is_file():
        actual = sha256(corpus_path)
        if actual != expected:
            problems.append(f"G1 precondition: corpus file on disk {actual} is not "
                            f"pinned {expected}")
    if pin.get("schema") != PIN_SCHEMA:
        problems.append(f"{pin_path}: unknown pin schema {pin.get('schema')!r}")
    return {
        "pin": str(pin_path), "pin_sha256": sha256(pin_path),
        "corpus_sha256": in_report, "provenance_sha256": meta_hash,
    }


def number(totals, key, problems):
    value = totals.get(key)
    if value is None:
        problems.append(f"the harness emitted no {key}")
        return 0.0
    try:
        return float(value)
    except (TypeError, ValueError):
        problems.append(f"{key} is not a number: {value!r}")
        return 0.0


def evaluate_step(name, report, pin, pin_path, ruled_lines):
    """One step's gates. Problems are findings, never verdicts about the feature."""
    problems = []
    identity = check_pin(report, pin, pin_path, problems)
    totals = report.get("totals")
    if not isinstance(totals, dict):
        return {"name": name, "problems": ["the report has no totals"],
                "pin": identity, "g1": "fail", "wrong_direction": "fail"}

    q = number(totals, "q_pool_share_measured", problems)
    q_model = number(totals, "q_pool_share_model", problems)
    u = number(totals, "u_manual_intervention", problems)
    r = number(totals, "r_misfires", problems)
    k = number(totals, "k_mean_backspaces", problems)
    t = number(totals, "transitions", problems)
    if t <= 0:
        problems.append("the run has no transitions -- q is undefined")
    if k < 0:
        problems.append(f"the measured repair cost k={k} is negative")
    divisor = 2 * k + 1
    budget = (1 - u - q_model) / divisor if divisor else 0.0

    # The harness prints q twice on purpose (MODES-MEASUREMENT §2.1). The model
    # must satisfy its own identity, and the measured figure must be the replay's
    # own (off-on)/t. Otherwise the budget line is arithmetic on air.
    model_rhs = 1 - u - divisor * r
    if abs(model_rhs - q_model) > EPS * max(1.0, abs(q_model)):
        problems.append(f"q_pool_share_model {q_model:.6f} disagrees with "
                        f"1-u-(2k+1)r = {model_rhs:.6f}")
    off, on = number(totals, "keys_off", problems), number(totals, "keys_on", problems)
    if t > 0 and abs((off - on) / t - q) > EPS * max(1.0, abs(q)):
        problems.append("q_pool_share_measured is not (keys_off-keys_on)/t")
    counts = totals.get("counts") or {}
    wrong_total = number(counts, "delivered_wrong", problems)
    backspaces = number(totals, "backspaces", problems)
    if wrong_total > 0 and abs(backspaces / wrong_total - k) > EPS * max(1.0, k):
        problems.append("k_mean_backspaces is not backspaces/delivered_wrong")
    cross = totals.get("cross_check") or {}
    if cross.get("mismatches"):
        problems.append(f"the harness disagrees with hunmin-sim on "
                        f"{cross['mismatches']} sentence(s)")

    g1 = q + EPS >= 0.0
    if not g1:
        problems.append(f"G1: q_pool_share_measured {q:.6f} < 0")

    # Per-direction wrong-direction guard, with an instrument cross-check so a
    # renamed or missing direction counter cannot pass vacuously.
    per_dir = []
    dir_wrong = totals.get("wrong_direction_by_direction") or {}
    outcomes = totals.get("outcomes_by_direction") or {}
    by_dir = totals.get("transitions_by_direction") or {}
    for direction in DIRECTIONS:
        row = outcomes.get(direction) or {}
        typed = row.get("switched_to_typed")
        other = row.get("switched_to_other")
        if not isinstance(typed, int) or not isinstance(other, int):
            problems.append(f"wrong-direction guard {direction}: missing switch counts")
            continue
        wrong = dir_wrong.get(direction)
        if wrong != other:
            problems.append(f"wrong-direction guard {direction}: "
                            f"wrong_direction_by_direction={wrong} but "
                            f"switched_to_other={other}")
        switches = typed + other
        rate = (wrong / switches) if switches and isinstance(wrong, int) else None
        passed = wrong == 0
        if not passed:
            problems.append(f"wrong-direction guard {direction}: {wrong} switch(es) "
                            "delivered against the script that was typed")
        per_dir.append({"direction": direction, "transitions": by_dir.get(direction),
                        "switches": switches, "wrong_switches": wrong,
                        "wrong_direction_rate": rate, "status": "pass" if passed else "fail"})

    lines = []
    for lined_q, lined_u in ruled_lines:
        line_budget = (1 - lined_u - lined_q) / divisor if divisor else 0.0
        passed = q + EPS >= lined_q and u - EPS <= lined_u
        if not passed:
            problems.append(f"G2 ruled line q>={lined_q}, u<={lined_u}: "
                            f"q={q:.6f}, u={u:.6f}")
        lines.append({"q": lined_q, "u": lined_u,
                      "r_budget": line_budget, "status": "pass" if passed else "fail"})

    return {
        "name": name,
        "pin": identity,
        "g1": "pass" if g1 else "fail",
        "wrong_direction": "pass" if all(d["status"] == "pass" for d in per_dir) else "fail",
        "metrics": {
            "transitions": t, "q_pool_share_measured": q, "q_pool_share_model": q_model,
            "u_manual_intervention": u, "k_mean_backspaces": k,
            "r_misfires": r, "r_budget_at_k": budget,
            "r_within_budget": r <= budget + EPS,
            "ruled_lines": lines,
        },
        "by_direction": per_dir,
        "problems": problems,
    }


def number_or_none(totals, key):
    value = totals.get(key)
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def grade_g3(baseline, candidate):
    """G3: fewer mid-word display flips at an equal pool share.

    `baseline` and `candidate` are whole reports from `modes-metrics.py` runs of
    the same pinned corpus with one display setting changed. The pool share must
    be EQUAL -- that is the "at equal pool share" half, and it is what stops a
    flip reduction that was bought by switching less from passing as a display
    improvement. The flips must then be strictly fewer, and the harness's own
    guard cross-check must hold (no flip under ReqMode9's three key inputs).
    """
    problems: list[str] = []
    bt = baseline.get("totals") or {}
    ct = candidate.get("totals") or {}
    record = {"baseline": (baseline.get("instrument") or {}).get("tool"),
              "graded": False, "problems": problems}
    bf = bt.get("flips")
    cf = ct.get("flips")
    if not isinstance(bf, dict) or not isinstance(cf, dict):
        problems.append("G3: a report has no flips block -- re-run "
                        "tools/modes-metrics.py, this build predates the unit")
        return record
    bc = (baseline.get("corpus") or {}).get("sha256")
    cc = (candidate.get("corpus") or {}).get("sha256")
    if bc != cc:
        problems.append(f"G3: the two runs are not the same corpus ({bc} vs {cc})")
    bq = number_or_none(bt, "q_pool_share_measured")
    cq = number_or_none(ct, "q_pool_share_measured")
    if bq is None or cq is None:
        problems.append("G3: a report has no q_pool_share_measured")
    elif abs(bq - cq) > EPS * max(1.0, abs(bq)):
        problems.append(f"G3: the pool share moved ({bq:.6f} -> {cq:.6f}); a flip "
                        "count at a different pool share is not the comparison")
    missing = False
    for key in ("total", "words_with_a_flip", "words_with_a_flip_back"):
        if not isinstance(bf.get(key), int) or not isinstance(cf.get(key), int):
            problems.append(f"G3: flips.{key} missing from a report")
            missing = True
    if missing:
        return record
    below = cf.get("flips_below_the_guard")
    if below:
        problems.append(f"G3 harness: {below} flip(s) below ReqMode9's "
                        "three-key guard -- the unit is not what it says")
    b_total, c_total = int(bf["total"]), int(cf["total"])
    b_words, c_words = int(bf["words_with_a_flip"]), int(cf["words_with_a_flip"])
    b_back, c_back = int(bf["words_with_a_flip_back"]), int(cf["words_with_a_flip_back"])
    if c_total >= b_total:
        problems.append(f"G3: mid-word display flips did not fall "
                        f"({b_total} -> {c_total})")
    if c_words > b_words:
        problems.append(f"G3: more words flicker ({b_words} -> {c_words})")
    record.update({
        "graded": True,
        "pool_share": {"baseline": bq, "candidate": cq, "equal": bq == cq},
        "flips": {"baseline": b_total, "candidate": c_total,
                  "fewer": c_total < b_total},
        "words_with_a_flip": {"baseline": b_words, "candidate": c_words},
        "words_with_a_flip_back": {"baseline": b_back, "candidate": c_back},
        "judge_hold": {"baseline": bf.get("judge_hold"),
                       "candidate": cf.get("judge_hold")},
        "status": "fail" if problems else "pass",
    })
    return record


def print_table(steps):
    print("step                 K2 q        T3 u        k      r      r_max   G1")
    for s in steps:
        m = s["metrics"]
        print(f"{s['name'][:20]:20} {m['q_pool_share_measured']:8.2%} "
              f"{m['u_manual_intervention']:11.2%} {m['k_mean_backspaces']:6.2f} "
              f"{m['r_misfires']:6.2%} {m['r_budget_at_k']:7.2%}  {s['g1']}")
    for s in steps:
        g3 = s.get("g3")
        if g3:
            if g3.get("graded"):
                f = g3["flips"]
                q = g3["pool_share"]
                print(f"G3 mid-word flips [{s['name']}] {f['baseline']} -> "
                      f"{f['candidate']} at q {q['candidate']:+.6f} "
                      f"(baseline {q['baseline']:+.6f}) -- {g3['status']}")
            else:
                print(f"G3 mid-word flips [{s['name']}] {g3.get('status')}")
        print(f"wrong-direction guard [{s['name']}]")
        for d in s["by_direction"]:
            rate = "n/a" if d["wrong_direction_rate"] is None else \
                   f"{100 * d['wrong_direction_rate']:.2f}%"
            print(f"  {d['direction']:13} {d['wrong_switches']} of "
                  f"{d['switches']} switches ({rate}) -- {d['status']}")
        for line in s["metrics"]["ruled_lines"]:
            print(f"  ruled line q>={line['q']:.2%}, u<={line['u']:.2%} "
                  f"=> r_max {line['r_budget']:.2%} -- {line['status']}")


def check(args):
    if not args.step:
        raise SystemExit("check needs at least one --step NAME REPORT")
    if not args.pin:
        raise SystemExit("check needs --pin PIN (make one with `pin --corpus`)")
    pin_path = pathlib.Path(args.pin)
    if not pin_path.is_file():
        raise SystemExit(f"{pin_path}: no such pin")
    pin = load_json(pin_path)
    steps, loaded = [], {}
    for name, path in args.step:
        report = load_json(path)
        loaded[name] = report
        steps.append(evaluate_step(name, report, pin, pin_path, args.line))
    if args.flip_baseline:
        if args.flip_baseline not in loaded:
            raise SystemExit(f"--flip-baseline {args.flip_baseline!r} is not one of "
                             f"the --step names ({', '.join(loaded)})")
        if len(loaded) < 2:
            raise SystemExit("--flip-baseline needs a second --step to grade: the "
                             "baseline is one of the runs, not a file of its own")
        base = loaded[args.flip_baseline]
        for step in steps:
            if step["name"] == args.flip_baseline:
                step["g3"] = {"status": "baseline", "problems": []}
                continue
            record = grade_g3(base, loaded[step["name"]])
            step["g3"] = record
            step["problems"].extend(record["problems"])
    problems = [f"[{s['name']}] {p}" for s in steps for p in s["problems"]]
    print_table(steps)
    for p in problems:
        print(f"FAIL {p}")
    status = "fail" if problems else "pass"
    graded_g3 = [s["g3"] for s in steps if s.get("g3") and s["g3"].get("graded")]
    print(f"autoswitch-gates: {status} -- "
          f"G1 {'pass' if all(s['g1'] == 'pass' for s in steps) else 'fail'}, "
          f"wrong-direction {'pass' if all(s['wrong_direction'] == 'pass' for s in steps) else 'fail'}, "
          f"G2 {'graded' if args.line else 'reported, not graded'}, "
          f"G3 {'pass' if graded_g3 and all(g['status'] == 'pass' for g in graded_g3) else ('fail' if graded_g3 else 'not graded')}")
    if args.report_out:
        out = pathlib.Path(args.report_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        doc = {
            "meta": {
                "schema": REPORT_SCHEMA,
                "tool": "tools/autoswitch-gates.py",
                "invocation": " ".join(sys.argv),
                "generated_at": datetime.datetime.now(datetime.timezone.utc)
                                        .isoformat(timespec="seconds"),
                "inputs": [{"step": n, "path": str(p), "sha256": sha256(p)}
                           for n, p in args.step],
                "pin": {"path": str(pin_path), "sha256": sha256(pin_path)},
            },
            "steps": steps, "problems": problems, "status": status,
            "G3_flip_baseline": args.flip_baseline,
            "G2_lines": {"ruled": [[q, u] for q, u in args.line],
                         "graded": bool(args.line)},
        }
        out.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        print(f"{out}: gate report")
    return 1 if problems else 0


def synthetic():
    """A report whose numbers satisfy every identity the checker asserts."""
    k, r, u, t = 2.0, 0.04, 0.20, 100
    q_model = 1 - u - (2 * k + 1) * r  # 0.60
    counts = {"switched_to_typed": 80, "switched_to_other": 0,
              "no_switch": 20, "delivered_wrong": 4}
    outcomes = {
        "hangul->latin": {"switched_to_typed": 40, "switched_to_other": 0,
                          "no_switch": 10, "delivered_wrong": 2,
                          "wrong_below_guard": 0, "wrong_at_or_above_guard": 2},
        "latin->hangul": {"switched_to_typed": 40, "switched_to_other": 0,
                          "no_switch": 10, "delivered_wrong": 2,
                          "wrong_below_guard": 0, "wrong_at_or_above_guard": 2},
    }
    totals = {
        "transitions": t, "keys_off": 1000, "keys_on": 1000 - int(q_model * t),
        "switches": 80, "backspaces": 8, "r_misfires": r,
        "k_mean_backspaces": k, "u_manual_intervention": u,
        "q_pool_share_model": q_model, "q_pool_share_measured": q_model,
        "counts": counts, "outcomes_by_direction": outcomes,
        "transitions_by_direction": {"hangul->latin": 50, "latin->hangul": 50},
        "wrong_direction_by_direction": {"hangul->latin": 0, "latin->hangul": 0},
        "cross_check": {"mismatches": 0, "sentences_compared": 1},
    }
    report = {"instrument": {"tool": "synthetic"},
              "corpus": {"path": "/no/such/corpus", "sha256": "a" * 64,
                         "meta": {"source": {"sha256": "b" * 64}}},
              "totals": totals}
    pin = {"schema": PIN_SCHEMA, "name": "synthetic",
           "corpus": {"sha256": "a" * 64},
           "provenance": {"sha256": sha256_bytes(canonical(report["corpus"]["meta"])),
                          "source_sha256": "b" * 64}}
    return report, pin


def selfcheck():
    """A green check that cannot fail is decoration; each mutation must turn it red."""
    import copy
    report, pin = synthetic()
    pin_path = ROOT / "tools" / "autoswitch-gates.py"  # any existing file hashes fine
    ok = evaluate_step("clean", report, pin, pin_path, [])
    if ok["problems"]:
        print("SELFCHECK FAIL clean report:", ok["problems"], file=sys.stderr)
        return 1
    tests = []

    bad = copy.deepcopy(report)
    bad["totals"]["q_pool_share_measured"] = -0.01
    bad["totals"]["keys_on"] = 1001
    tests.append(("G1", evaluate_step("g1", bad, pin, pin_path, [])))

    for direction in DIRECTIONS:
        bad = copy.deepcopy(report)
        bad["totals"]["wrong_direction_by_direction"][direction] = 1
        bad["totals"]["outcomes_by_direction"][direction]["switched_to_other"] = 1
        tests.append((f"wrong-direction {direction}",
                      evaluate_step(direction, bad, pin, pin_path, [])))

    bad = copy.deepcopy(report)
    bad["totals"]["wrong_direction_by_direction"]["hangul->latin"] = 1
    tests.append(("direction counter integrity",
                  evaluate_step("integrity", bad, pin, pin_path, [])))

    bad_pin = copy.deepcopy(pin)
    bad_pin["corpus"]["sha256"] = "c" * 64
    tests.append(("corpus pin", evaluate_step("pin", report, bad_pin, pin_path, [])))

    bad = copy.deepcopy(report)
    bad["totals"]["q_pool_share_model"] = 0.5
    tests.append(("q arithmetic", evaluate_step("arithmetic", bad, pin, pin_path, [])))

    tests.append(("ruled G2 line",
                  evaluate_step("line", report, pin, pin_path, [(0.7, 0.1)])))

    # G3: a flip drop at an equal pool share passes, and each way of buying it
    # wrongly -- fewer flips at a different q, no drop at all, a stale harness --
    # has to turn red.
    def with_flips(total, words_with_a_flip=10, below_guard=0, q=None):
        r = copy.deepcopy(report)
        if q is not None:
            r["totals"]["q_pool_share_measured"] = q
            r["totals"]["q_pool_share_model"] = q
            r["totals"]["keys_on"] = 1000 - int(q * r["totals"]["transitions"])
        r["totals"]["flips"] = {
            "judge_hold": 1, "total": total, "words": 6690,
            "words_with_a_flip": words_with_a_flip,
            "words_with_a_flip_back": 3, "display_keys": 40000,
            "flips_below_the_guard": below_guard,
        }
        return r

    base = with_flips(2546, 1018)
    g3_cases = [
        ("clean", grade_g3(base, with_flips(477, 434))),
        ("no reduction", grade_g3(base, with_flips(2546, 1018))),
        ("other pool share",
         grade_g3(base, with_flips(477, 434, q=report["totals"]["q_pool_share_measured"] - 0.01))),
        ("more flickering words", grade_g3(base, with_flips(477, 1200))),
        ("flip below the guard", grade_g3(base, with_flips(477, 434, below_guard=1))),
        ("stale harness", grade_g3(base, synthetic()[0])),
    ]
    if g3_cases[0][1]["problems"]:
        failures_g3 = ["clean G3 comparison: " + p for p in g3_cases[0][1]["problems"]]
    else:
        failures_g3 = []
    for label, result in g3_cases[1:]:
        if not result["problems"]:
            failures_g3.append(f"G3 {label}: mutation stayed green")

    failures = []
    for label, result in tests:
        if not result["problems"]:
            failures.append(f"{label}: mutation stayed green")
    for label, result in tests:
        if label.startswith("wrong-direction ") and result["problems"]:
            direction = label.split(" ", 1)[1]
            if not any(direction in p for p in result["problems"]):
                failures.append(f"{label}: the failure did not name the direction")
    failures.extend(failures_g3)
    if failures:
        print("SELFCHECK FAIL", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1
    print(f"OK  G1, both wrong-direction guards, pin, arithmetic, a ruled G2 "
          f"line and all {len(g3_cases) - 1} wrong ways to buy a G3 flip drop each "
          f"turned red ({len(tests)} + {len(g3_cases) - 1} mutations); clean "
          f"reports are green")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    sub = ap.add_subparsers(dest="command")
    p_pin = sub.add_parser("pin", help="write a corpus pin without copying the corpus")
    p_pin.add_argument("--corpus", required=True, type=pathlib.Path)
    p_pin.add_argument("--out", required=True, type=pathlib.Path)
    p_pin.add_argument("--name", default=None)
    p_check = sub.add_parser("check", help="grade G1/G2 from metrics reports")
    p_check.add_argument("--step", action="append", nargs=2, metavar=("NAME", "REPORT"),
                         default=[], help="repeat for a stepwise report")
    p_check.add_argument("--pin", required=True, type=pathlib.Path)
    p_check.add_argument("--line", action="append", nargs=2, type=float,
                         metavar=("Q", "U"), default=[],
                         help="a maintainer-ruled K2/T3 pair to grade (repeatable)")
    p_check.add_argument("--flip-baseline", metavar="NAME", default=None,
                         help="one of the --step names: grade G3 (fewer mid-word "
                              "display flips at an equal pool share) for every other "
                              "step against it")
    p_check.add_argument("--report-out", type=pathlib.Path,
                         help="write the self-contained gate report here")
    ap.add_argument("--selfcheck", action="store_true",
                    help="prove every guard can fail; no corpus or build needed")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck()
    if args.command == "pin":
        return make_pin(args)
    if args.command == "check":
        return check(args)
    ap.error("choose `pin` or `check`, or pass --selfcheck")


if __name__ == "__main__":
    sys.exit(main())
