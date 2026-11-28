#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""The projection a tier-A corpus asserts: the text a person ends up with.

`visible` is the ONE quantity that does not depend on the commit unit
(docs/DESIGN-FRONTEND.md §5.2, docs/SPEC-FRONTEND.md §FE1/§FE18):

    document = the text the engine has delivered that still survives
    visible  = document + what is composing

and a key the driver DECLINES changes `document` the way the CLIENT would,
because the engine never sees that edit:

    document += this key's commit                      (engine delivered)
    document -= its last code point   if a BackSpace is declined
    document += the key's character   if a printable key is declined

Measured 2026-11-27: with this rule the same keys produce the same `visible`
sequence through `hunmin_ic`, the boundary layer and the syllable layer --
including the FE18 walk `rkrk\\b\\b\\b\\b` (가가 → 가ㄱ → 가 → "" → ""), where
per-key `consumed` differs (the layer still holds the text the engine has
already delivered). That is why tier A asserts `visible` per key and tier C
asserts `commit`/`preedit`/`consumed` per key.

Why this file exists: the rule is consumed by three tools (the case golden
generator, the case merger and the trace generator) and by the checker in
`tools/sim.c`. One Python home + the C checker keeps the cross-check (two
independent implementations) without a third copy.
"""
import json
import pathlib
import subprocess

try:
    from _common import ROOT
except ImportError:  # run from elsewhere: derive from this file
    ROOT = pathlib.Path(__file__).resolve().parents[1]

DATA = ROOT / "libhunmin" / "data"

BACKSPACE_SPELLINGS = ("\b", "\u0008")


class ProjectionError(Exception):
    pass


def keys_from_events(events):
    """The `hunmin-sim --keys` alphabet for a list of tier-A events.

    A plain event is its `ch`. A `{"shift": true}` event is `\\s<char>` (the
    base character with Shift held), which is how the schema spells a shifted
    key. A BackSpace is `\\b` in either spelling. A literal backslash is
    refused: every corpus excludes it from its key alphabet, and the `\\`
    escape exists precisely so an unescaped one cannot be silent.
    """
    out = []
    for ev in events:
        spec = ev["in"]
        if spec.get("key") == "backspace" or spec.get("ch") in BACKSPACE_SPELLINGS:
            out.append("\\b")
            continue
        ch = spec["ch"]
        if ch == "\\":
            raise ProjectionError("a literal backslash is not in any corpus alphabet")
        out.append(("\\s" + ch) if spec.get("shift") else ch)
    return "".join(out)


def run_trace(sim, layout, keys, data=None):
    """Every key's answer from the engine's own trace (the tier-A oracle)."""
    proc = subprocess.run(
        [str(sim), "--data", str(data or DATA), "--layout", layout, "--keys", keys, "--trace"],
        capture_output=True, text=True)
    if proc.returncode != 0:
        raise ProjectionError("hunmin-sim failed (%d): %s" % (proc.returncode, proc.stderr.strip()))
    rows = [json.loads(line) for line in proc.stdout.splitlines() if line.startswith("{")]
    expected = len([k for k in _split_keys(keys)])
    if len(rows) != expected:
        raise ProjectionError("hunmin-sim produced %d rows for %d keys" % (len(rows), expected))
    return rows


def _split_keys(keys):
    """Mirror of tools/sim.c's keys_step, for the row-count check only."""
    i = 0
    while i < len(keys):
        if keys[i] == "\\" and i + 1 < len(keys):
            n = 2 if keys[i + 1] == "b" else 3 if keys[i + 1] == "s" else 2
            yield keys[i:i + n]
            i += n
        else:
            yield keys[i]
            i += 1


def _drop_last_code_point(text):
    if not text:
        return text
    i = len(text)
    while i > 0 and (ord(text[i - 1]) & 0xC0) == 0x80:
        i -= 1
    return text[:i - 1] if i > 0 else text


def project(rows):
    """The `visible` text after each row (see the module docstring)."""
    document = ""
    out = []
    for row in rows:
        document += row.get("commit", "")
        if not row.get("consumed"):
            spec = row.get("in", {})
            ch = spec.get("ch", "")
            if ch in BACKSPACE_SPELLINGS:
                document = _drop_last_code_point(document)
            elif spec.get("shift"):
                raise ProjectionError(
                    "a declined shifted key needs its resolved character; projection "
                    "corpora spell shifted keys resolved (docs/SPEC-SCHEMA.md)")
            elif ch and (ch.isprintable() or ch == " "):
                document += ch
        out.append(document + row.get("preedit", ""))
    return out

