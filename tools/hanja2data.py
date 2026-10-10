#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Convert the vendored hanja dictionary into hunmin's candidate table.

Source: data-orig/hanja/ (libhangul 0.2.0, BSD-3 for the dictionary -- NOT
LGPL; see libhunmin/data/hanja/LICENSE.md).

Candidate order is **libhangul's own frequency pipeline**, reimplemented from
data-orig/hanja/merge.py. Upstream sorts each key's candidates by the frequency
in freq-hanja.txt / freq-hanyaeo.txt (higher first), stably, so ties keep the
source order; it also merges duplicate candidates, combining their comments, and
unifies compatibility characters via compat-table.txt. The shipped hanja.txt is
*not* sorted -- upstream runs merge.py at build time, and the Debian package
carries the unsorted result -- so reimplementing the sort is the difference
between "자비" leading and not.

Deliberately not reimplemented from a third-party frequency source: Unihan's
kFrequency is gone in Unicode 18 and kHanyuPinlu ranks hanja by *Chinese*
frequency, which for Korean is wrong (水 is 34th for 수, 姊 before 慈 for 자).
libhangul already had Korean frequency data in the tree all along.

Only single-syllable keys are converted. The source also holds 222,154
multi-character keys -- Korean grammar and vocabulary (ㄷ불규칙용언, 학훈단)
rather than hanja readings -- which a hanja conversion mode cannot use. They
remain in data-orig/ if anything ever wants them.

Deterministic: same inputs -> byte-identical output (tools/selfcheck.py).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Upstream keeps every candidate. A 352-entry window is not a UI, so we cap;
# 99th percentile is 8, so this only trims the extreme tail.
MAX_CANDIDATES = 64

SCHEMA = "hunmin.hanja/1"



def input_pins(orig: Path) -> dict[str, str]:
    """sha256 of every input we read, recorded in the output.

    Read from disk rather than hardcoded so the metadata cannot drift from
    the vendored files -- a hardcoded table duplicated SHA256SUMS, and when
    SHA256SUMS was corrected the copy went stale.
    """
    import hashlib
    out = {}
    for p in sorted(orig.glob("*")):
        if p.is_file() and p.name not in ("SHA256SUMS", "NOTICE"):
            out[p.name] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def is_syllable_key(key: str) -> bool:
    return len(key) == 1 and "가" <= key <= "힣"


def load_compat(path: Path, warn) -> dict[str, str]:
    """|<hex> TAB <hex>| -> {from: to}, as merge.py's load_compat()."""
    out: dict[str, str] = {}
    if not path.is_file():
        warn(f"no compat table at {path}; compatibility chars left as-is")
        return out
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) != 2:
            warn(f"compat line {n}: expected 2 hex fields, got {line!r}")
            continue
        out[chr(int(parts[0], 16))] = chr(int(parts[1], 16))
    return out


def load_frequencies(orig: Path, warn) -> dict[str, float]:
    """Merge freq-hanja.txt and freq-hanyaeo.txt, taking the max, as upstream."""
    freq: dict[str, float] = {}
    for name in ("freq-hanja.txt", "freq-hanjaeo.txt"):
        path = orig / name
        if not path.is_file():
            warn(f"no frequency file at {path}; candidates keep source order")
            continue
        for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip():
                continue
            parts = line.split(":")
            if len(parts) != 2:
                warn(f"{name} line {n}: expected key:value, got {line!r}")
                continue
            try:
                value = float(parts[1])
            except ValueError:
                warn(f"{name} line {n}: non-numeric frequency {parts[1]!r}")
                continue
            key = parts[0]
            freq[key] = max(freq.get(key, 0.0), value)
    return freq


def unify(text: str, compat: dict[str, str]) -> str:
    return "".join(compat.get(ch, ch) for ch in text)


def build(orig: Path, warn) -> dict:
    src = orig / "hanja.txt"
    if not src.is_file():
        raise SystemExit(f"hanja2data: missing {src}")

    compat = load_compat(orig / "compat-table.txt", warn)
    freq = load_frequencies(orig, warn)

    # key -> list of {value, freq, comment}, in source order.
    table: dict[str, list[dict]] = {}
    skipped_words = 0
    dup_merged = 0
    for n, line in enumerate(src.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split(":")
        if len(parts) < 3:
            warn(f"line {n}: expected at least 3 colon-separated fields")
            continue
        key, raw_value, comment = parts[0], parts[1], parts[2].strip()
        if not key or not raw_value:
            warn(f"line {n}: empty key or candidate, skipped")
            continue
        if not is_syllable_key(key):
            skipped_words += 1
            continue
        value = unify(raw_value, compat)
        entry = {"value": value, "freq": freq.get(value, 0.0), "comment": comment}
        bucket = table.setdefault(key, [])
        existing = next((e for e in bucket if e["value"] == value), None)
        if existing is None:
            bucket.append(entry)
            continue
        # Upstream merges duplicates rather than repeating the candidate,
        # preferring to keep/extend the comment.
        dup_merged += 1
        if comment and not existing["comment"]:
            existing["comment"] = comment
        elif comment and comment != existing["comment"]:
            existing["comment"] = f"{existing['comment']}, {comment}" if \
                existing["comment"] else comment

    warn(f"skipped {skipped_words} multi-character key(s) "
         f"(Korean words, not hanja readings)")
    if dup_merged:
        warn(f"merged {dup_merged} duplicate candidate(s)")

    order = sorted(table)
    entries: dict[str, list[str]] = {}
    dropped = 0
    ranked = 0
    for key in order:
        # Python's sort is stable, so equal frequencies keep source order --
        # which is what merge.py's comparator does too.
        bucket = sorted(table[key], key=lambda e: -e["freq"])
        ranked += sum(1 for e in bucket if e["freq"] > 0)
        if len(bucket) > MAX_CANDIDATES:
            dropped += len(bucket) - MAX_CANDIDATES
        entries[key] = [e["value"] for e in bucket[:MAX_CANDIDATES]]

    if dropped:
        warn(f"{dropped} candidate(s) beyond the {MAX_CANDIDATES} cap were dropped")

    n_cands = sum(len(v) for v in entries.values())
    return {
        "version": 1,
        "schema": SCHEMA,
        "entries": entries,
        "meta": {
            "sources": {
                "candidates": "libhangul 0.2.0 data/hanja/hanja.txt",
                "candidate_licence": "BSD-3-Clause",
                "order": "libhangul data/hanja/merge.py frequency pipeline "
                         "(freq-hanja + freq-hanyaeo, compat-table unification, "
                         "stable descending sort)",
                "input_sha256": input_pins(orig),
            },
            "keys": len(order),
            "candidates": n_cands,
            "max_candidates_per_key": max(len(v) for v in entries.values()),
            "max_candidates_cap": MAX_CANDIDATES,
            "dropped_beyond_cap": dropped,
            "candidates_with_nonzero_frequency": ranked,
            "candidates_unranked": n_cands - ranked,
        },
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--orig", type=Path, required=True,
                    help="data-orig/hanja (hanja.txt, freq-*.txt, compat-table.txt)")
    ap.add_argument("--out", type=Path, required=True, help="libhunmin/data")
    ap.add_argument("--name", default="hanja", help="output file stem")
    args = ap.parse_args()

    warnings: list[str] = []
    def warn(msg: str) -> None:
        warnings.append(msg)

    table = build(args.orig, warn)
    for w in warnings:
        print(f"hanja2data: {w}", file=sys.stderr)

    out_dir = args.out / "hanja"
    out_dir.mkdir(parents=True, exist_ok=True)
    dst = out_dir / f"{args.name}.json"
    dst.write_text(json.dumps(table, ensure_ascii=False,
                              separators=(",", ":")) + "\n", encoding="utf-8")

    m = table["meta"]
    print(f"hanja2data: {m['keys']} keys, {m['candidates']} candidates "
          f"({m['candidates_with_nonzero_frequency']} with frequency data), "
          f"max {m['max_candidates_per_key']}/key -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())