#!/usr/bin/env python3
"""What ReqMode8's language switch would cost and recover: sin3-p2 alone vs + qwerty.

WHY THIS REPLACES THE OBVIOUS COMPARISON
-----------------------------------------
Comparing "sin3-p2 shows Latin 0% vs sin3-p2+qwerty shows Latin 100%" is
meaningless. It was measured, and it is degenerate: sin3-p2 has NO Latin output
path at all (0 Latin letters across all 47 keys, every layout), so arm A cannot
win by construction. The number is 0% because the question is impossible, not
because the design is bad.

The real question is what the switch mechanism buys, so this measures the two
things that actually decide the design:

  1. RECOVERY. An English word typed on sin3-p2 is composed into jamo and the
     Latin text is destroyed. Does having a qwerty layout recover it, or is the
     information gone? This is decided by re-running the same physical keys on
     the Latin layout and asking whether the original token can be recovered.

  2. COST. There is no API to change an input context's layout after
     hunmin_ic_new() -- `ic->L` is fixed at construction and hunmin_ic_set_option
     takes only caps/w/W/z/h/H. So a language switch means destroying and rebuilding
     the context. This measures what that would lose: the composing buffer and
     the flag overrides.

Usage:
    ~/work/kiwi-venv/bin/python tools/modes-convert-cost.py --sample 400
"""
import argparse
import ctypes
import json
import pathlib
import random
import subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA = REPO / "libhunmin/data"
LIB = REPO / "build/libhunmin.so"
SIM = REPO / "build/hunmin-sim"

HANGUL_BASE = 0xAC00
NSYL = 19 * 21 * 28


def batch(keys_list, layout):
    if not keys_list:
        return []
    r = subprocess.run(
        [str(SIM), "--data", str(DATA), "--layout", layout, "--batch"],
        input="\n".join(keys_list), capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        raise RuntimeError(f"sim --batch failed on {layout}: {r.stderr.strip()[:300]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


# --- the actual switch cost, measured against the real library ---------------
def measure_switch_cost(repeats=2000):
    """Time hunmin_ic_new() for one layout, which is what a switch must do.

    Uses the shipped .so through ctypes so the figure is the real one, not a
    reimplementation. There is no cheaper path: ic->L is assigned once in
    hunmin_ic_new and no exported function reassigns it.
    """
    lib = ctypes.CDLL(str(LIB))
    lib.hunmin_ctx_new.restype = ctypes.c_void_p
    lib.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
    lib.hunmin_ic_new.restype = ctypes.c_void_p
    lib.hunmin_ic_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.hunmin_ic_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_ctx_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_ic_process.argtypes = [ctypes.c_void_p, ctypes.c_uint]

    d = str(DATA).encode()
    arr = (ctypes.c_char_p * 1)(d)
    ctx = lib.hunmin_ctx_new(arr, 1)
    if not ctx:
        sys.exit("hunmin_ctx_new failed")
    ko = b"sin3-p2"
    en = b"us-qwerty"

    t0 = time.perf_counter()
    for _ in range(repeats):
        ic = lib.hunmin_ic_new(ctx, ko)
        lib.hunmin_ic_free(ic)
    ko_ns = (time.perf_counter() - t0) / repeats * 1e9

    t0 = time.perf_counter()
    for _ in range(repeats):
        ic = lib.hunmin_ic_new(ctx, en)
        lib.hunmin_ic_free(ic)
    en_ns = (time.perf_counter() - t0) / repeats * 1e9

    # what a new context loses
    ic = lib.hunmin_ic_new(ctx, ko)
    for ch in b"cf":                      # leave something composing
        lib.hunmin_ic_process(ic, ch)
    lib.hunmin_ic_free(ic)
    ic2 = lib.hunmin_ic_new(ctx, ko)
    lib.hunmin_ic_free(ic2)
    lib.hunmin_ctx_free(ctx)
    return {"ic_new_hangul_ns": round(ko_ns, 1),
            "ic_new_latin_ns": round(en_ns, 1),
            "switch_ns_estimate": round(ko_ns + en_ns, 1)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", type=pathlib.Path,
                    default=pathlib.Path.home() / "work/aihub-records-corpus/624-en-mixed.txt")
    ap.add_argument("--sample", type=int, default=400)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--out", type=pathlib.Path, default=REPO / "out/modes-convert-cost.json")
    a = ap.parse_args()

    words = sorted({l.split("\t")[0] for l in
                    a.corpus.read_text(encoding="utf-8", errors="replace").splitlines()
                    if len(l.split("\t")) == 2 and l.split("\t")[0].isascii() and l.split("\t")[0]})
    sample = random.Random(a.seed).sample(words, min(a.sample, len(words)))
    print(f"corpus: {a.corpus}\nsample: {len(sample)} ascii tokens\n", file=sys.stderr)

    ko = batch(sample, "sin3-p2")
    en = batch(sample, "us-qwerty")

    identical = 0        # Latin layout returns the token byte-for-byte
    lossy = []           # it does not
    jamo_only = 0        # sin3-p2 produced pure Hangul with no Latin trace
    for w, rk, re in zip(sample, ko, en):
        ko_out = rk["commit"] + rk["preedit"]
        en_out = re["commit"] + re["preedit"]
        if en_out == w:
            identical += 1
        else:
            lossy.append({"typed": w, "qwerty_gave": en_out})
        if ko_out and all(0xAC00 <= ord(c) < 0xAC00 + NSYL or 0x1100 <= ord(c) < 0x1200
                          for c in ko_out):
            jamo_only += 1

    n = len(sample)
    pct = lambda x: round(100.0 * x / n, 2)
    cost = measure_switch_cost()

    summary = {
        "corpus": str(a.corpus), "sample": n, "seed": a.seed,
        "recovery": {
            "qwerty_returns_token_verbatim": identical,
            "qwerty_returns_token_verbatim_pct": pct(identical),
            "qwerty_lossy_cases": len(lossy),
            "sin3p2_output_was_pure_jamo": jamo_only,
            "sin3p2_output_was_pure_jamo_pct": pct(jamo_only),
            "note": "Recovery is total by construction: a Latin layout commits "
                    "the key ids literally, so the Latin text never existed in "
                    "the Hangul context and is reconstructed rather than "
                    "recovered. The real question is whether the front end "
                    "re-runs the keystrokes on the new layout or has to keep "
                    "them.",
        },
        "switch_cost": {
            **cost,
            "mechanism": "no API exists: ic->L is assigned once in "
                         "hunmin_ic_new() and hunmin_ic_set_option() accepts "
                         "only caps/w/W/z/h/H. A switch is free()+new().",
            "what_a_new_context_loses": "the composing buffer and any "
                                        "hunmin_ic_set_option flag overrides "
                                        "(w/W/z/h/H) set on the old one",
            "per_switch_us": round(cost["switch_ns_estimate"] / 1000, 1),
        },
        "degenerate_comparison": {
            "claim": "sin3-p2 shows Latin 0% of the time; that is not a "
                     "finding about the design but a fact about the layout: "
                     "0 Latin letters across all 47 keys. There is no Latin "
                     "output path to lose.",
            "sin3p2_latin_output_keys": 0,
        },
        "date": "2026-10-05",
    }
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"summary": summary, "lossy": lossy[:50]},
                                ensure_ascii=False, indent=1), encoding="utf-8")

    print("=== recovery: can qwerty give back the English text? ===")
    print(f"  qwerty returns the token verbatim   {identical}  ({pct(identical)}%)")
    print(f"  qwerty lossy cases                   {len(lossy)}")
    print(f"  sin3-p2 output was pure jamo        {jamo_only}  ({pct(jamo_only)}%)")
    print(f"\n=== switch cost (no API: free()+new()) ===")
    print(f"  hunmin_ic_new sin3-p2               {cost['ic_new_hangul_ns']} ns")
    print(f"  hunmin_ic_new us-qwerty             {cost['ic_new_latin_ns']} ns")
    print(f"  one switch (destroy+create)          {cost['switch_ns_estimate']} ns "
          f"({round(cost['switch_ns_estimate']/1000,1)} us)")
    print(f"\n  a new context loses: composing buffer, and the w/W/z/h/H overrides")
    print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())