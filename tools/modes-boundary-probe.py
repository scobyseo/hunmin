#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Price the three parameters of the judgement boundary: SPEC-MODES §E2, §E3, §E6.

Docs: `docs/OPTIONS-MEASURED.md` §BND (the paper this produces), the requirement
text in `docs/SPEC-MODES.md` R4/R9 and §E2/§E3/§E6, and the measurement of the
structural half in §B1-3.

WHY A TOOL. The three items share one question — how far the composition buffer
may grow, how short a buffer is exempt from switching, and whether judgement ever
moves inside a word — and each of the three answers is a length. Lengths are
countable over data that already ships, so they are counted here rather than
argued about in prose.

WHAT IT MEASURES, and the unit each number is in:

  words   the shape of the population a cap has to hold: every form of
          `libhunmin/data/modes/en-US.json`, by characters, and the longest one.
          The cap question is "does N cover the longest word", so the longest
          word is the only figure the answer turns on; the tail table says how
          much of the population sits at each length.
  space   the short-word threshold's decision surface, exhaustively. Every
          `a`-`z` string of length 1..N is a thing a person can type; classify
          it by (english_valid, hangul_valid of its Hangul-layout rendering),
          which is R8's pair. `english_valid ∧ ¬hangul_valid` switches to
          English, `¬english_valid ∧ hangul_valid` switches to Hangul (the
          symmetric rule, E1), and a threshold N means "no switch below N".
          The four classes are counted, not sampled: the space is small enough.
  prefix  what moving judgement inside the word would cost (E6). For each
          dictionary form, the R8 decision at every prefix; a word whose
          decision reverses after it has already switched is one that would
          flicker (도깨비) if judgement ran per key. Counted over the whole
          shipped list, not a sample.
  buffer  where the buffer actually ends today (E2), measured, not read: the
          shared layer is driven key by key up to and past 4 KiB and the commit
          and preedit are compared with what was typed. The raw `hunmin_ic`
          path is measured beside it, because §E2's premise is about `STR_CAP`
          and that constant belongs to that path.

THE ENGINE IS THE ORACLE. Rendering, composition and `hangul_valid` come from
the built library through ctypes (`hunmin_ic_*`, `hunmin_hangul_valid`,
`hunmin_buffered_preedit_*`); only the dictionary membership test is Python's,
over the same file the layer loads. Nothing here re-implements composition, so a
number cannot be an artefact of a hand-written composer -- the failure mode
`docs/MODES-DATA.md` §4-1 records.

WHAT IT DOES NOT MEASURE. Running-text word lengths (no Korean corpus is in the
tree; the ones `docs/MODES-DATA.md` §4-3 names live outside it), the preedit
length an application tolerates (per-app, no input here), and felt flicker
(a person's report, not a count). The dictionary is a population of *forms*, not
of *tokens*: nothing below is frequency-weighted, and a number here says how much
of the vocabulary a rule touches, never how often a person would meet it.

USAGE
  cmake --build build-mingw                       # the engine, for --lib
  python3 tools/modes-boundary-probe.py words     # --lib defaults to the build tree
  python3 tools/modes-boundary-probe.py space --max-len 4
  python3 tools/modes-boundary-probe.py prefix
  python3 tools/modes-boundary-probe.py buffer
  python3 tools/modes-boundary-probe.py all --json > /tmp/bnd.json
"""

import argparse
import ctypes
import itertools
import json
import os
import string
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Hunmin's own constants, mirrored from libhunmin/src/hunmin_internal.h and
# libhunmin/include/hunmin.h so a reader does not have to open two files to check
# a column. STR_CAP is the raw engine's per-ic buffer; the 4 KiB is the value the
# maintainer proposed for the shared layer (2026-11-01).
STR_CAP = 64
PROPOSED_CAP_BYTES = 4096
BYTES_PER_SYLLABLE = 3  # UTF-8: every modern syllable is 3 bytes


# ----------------------------------------------------------------- ctypes ABI


class Engine:
    """The library's own entry points. Every number the probe reports comes
    through here or through a file both a person and the library can read."""

    def __init__(self, libpath, data):
        self.lib = ctypes.CDLL(libpath)
        L = self.lib
        L.hunmin_ctx_new.restype = ctypes.c_void_p
        L.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
        L.hunmin_ctx_free.argtypes = [ctypes.c_void_p]
        L.hunmin_ic_new.restype = ctypes.c_void_p
        L.hunmin_ic_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.hunmin_ic_free.argtypes = [ctypes.c_void_p]
        L.hunmin_ic_process.restype = ctypes.c_int
        L.hunmin_ic_process.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        L.hunmin_ic_process_key.restype = ctypes.c_int
        L.hunmin_ic_process_key.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
        L.hunmin_ic_commit.restype = ctypes.c_char_p
        L.hunmin_ic_commit.argtypes = [ctypes.c_void_p]
        L.hunmin_ic_preedit.restype = ctypes.c_char_p
        L.hunmin_ic_preedit.argtypes = [ctypes.c_void_p]
        L.hunmin_hangul_valid.restype = ctypes.c_int
        L.hunmin_hangul_valid.argtypes = [ctypes.c_char_p]
        L.hunmin_buffered_preedit_new.restype = ctypes.c_void_p
        L.hunmin_buffered_preedit_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
        L.hunmin_buffered_preedit_free.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_process_key.restype = ctypes.c_int
        L.hunmin_buffered_preedit_process_key.argtypes = [
            ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(KeyResult)]
        L.hunmin_buffered_preedit_commit_word.restype = ctypes.c_char_p
        L.hunmin_buffered_preedit_commit_word.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_display.restype = ctypes.c_char_p
        L.hunmin_buffered_preedit_display.argtypes = [ctypes.c_void_p]

        dirs = (ctypes.c_char_p * 1)(data.encode())
        self.ctx = L.hunmin_ctx_new(dirs, 1)
        if not self.ctx:
            raise SystemExit("no context: %s is not a data directory" % data)

    def hangul_valid(self, text):
        return self.lib.hunmin_hangul_valid(text.encode()) == 1

    def render(self, keys, layout, prefix_cb=None):
        """Type `keys` into a fresh ic on `layout` and return the accumulated
        commit plus the open preedit. The commit is read after EVERY key:
        hunmin_ic_process() clears it on entry, so a single read at the end keeps
        only the last key's text (the same trap `tools/sim.c --batch` records)."""
        ic = self.lib.hunmin_ic_new(self.ctx, layout.encode())
        if not ic:
            raise SystemExit("layout unavailable: %s" % layout)
        out = []
        try:
            for k in keys:
                self.lib.hunmin_ic_process(ic, ord(k))
                c = self.lib.hunmin_ic_commit(ic) or b""
                if c:
                    out.append(c.decode())
                if prefix_cb is not None:
                    prefix_cb("".join(out) + (self.lib.hunmin_ic_preedit(ic) or b"").decode())
            pre = (self.lib.hunmin_ic_preedit(ic) or b"").decode()
        finally:
            self.lib.hunmin_ic_free(ic)
        return "".join(out) + pre

    def buffer_run(self, syllables, keys_per_syllable="dks", layout="dubeolsik"):
        """One word of `syllables` syllables through the SHARED layer
        (HUNMIN_COMMIT_AT_BOUNDARY, the shipped default), returning the longest
        preedit seen, the committed word, and whether it equals what was typed."""
        layer = self.lib.hunmin_buffered_preedit_new(self.ctx, layout.encode(), b"us-qwerty", 0)
        if not layer:
            raise SystemExit("layer unavailable")
        maxpre = 0
        try:
            for _ in range(syllables):
                for k in keys_per_syllable:
                    out = KeyResult()
                    self.lib.hunmin_buffered_preedit_process_key(layer, ord(k), 0, ctypes.byref(out))
                    maxpre = max(maxpre, len(out.preedit or b""))
            word = (self.lib.hunmin_buffered_preedit_commit_word(layer) or b"").decode()
        finally:
            self.lib.hunmin_buffered_preedit_free(layer)
        return maxpre, word


class KeyResult(ctypes.Structure):
    _fields_ = [("consumed", ctypes.c_int), ("commit", ctypes.c_char_p), ("preedit", ctypes.c_char_p)]


# -------------------------------------------------------------------- inputs


def load_words(data):
    path = os.path.join(data, "modes", "en-US.json")
    with open(path, encoding="utf-8") as f:
        forms = json.load(f)
    return path, forms


def letter_keys(form):
    """The layout keys of a form: hunspell's `'` and `-` are not keys on the
    layouts this prices, so those forms are excluded and counted, never silently
    transliterated (the same rule `docs/OPTIONS-MEASURED.md` §B1-3 used)."""
    if form.isalpha() and form.isascii():
        return form
    return None


# ------------------------------------------------------------------- measures


def measure_words(forms):
    lens = sorted(len(f) for f in forms)
    n = len(lens)
    longest = max(forms, key=len)

    def share_le(k):
        # bisect would be faster and matter not: this runs once.
        i = 0
        while i < n and lens[i] <= k:
            i += 1
        return i / n

    tail = []
    for k in (1, 2, 3, 4, 5, 6, 8, 10, 16, 24, 32, 64):
        tail.append({"chars": k, "forms_le": sum(1 for x in lens if x <= k), "share_le": share_le(k)})
    keys = [letter_keys(f) for f in forms]
    usable = [k for k in keys if k]
    return {
        "forms": n,
        "longest": {"form": longest, "chars": len(longest), "bytes": len(longest.encode())},
        "longest_letters_only": {"form": max(usable, key=len), "chars": max(len(k) for k in usable)},
        "mean_chars": sum(lens) / n,
        "form_keys": len(usable),
        "no_keys": n - len(usable),
        "tail": tail,
        "proposed_cap_bytes": PROPOSED_CAP_BYTES,
        "proposed_cap_syllables": PROPOSED_CAP_BYTES // BYTES_PER_SYLLABLE,
        "headroom_over_longest": PROPOSED_CAP_BYTES / len(longest),
        "forms_over_cap": sum(1 for f in forms if len(f.encode()) > PROPOSED_CAP_BYTES),
    }


def measure_space(engine, forms, hangul_layout, max_len):
    words = set(forms)
    rows = []
    for n in range(1, max_len + 1):
        tally = {"both": 0, "english_only": 0, "hangul_only": 0, "neither": 0}
        examples = {"both": [], "english_only": [], "hangul_only": [], "neither": []}
        rendered_chars = 0
        total = 0
        for tup in itertools.product(string.ascii_lowercase, repeat=n):
            s = "".join(tup)
            total += 1
            e = s in words
            text = engine.render(s, hangul_layout)
            rendered_chars += len(text)
            h = engine.hangul_valid(text)
            cls = ("both" if (e and h) else
                   "english_only" if e else
                   "hangul_only" if h else "neither")
            tally[cls] += 1
            if len(examples[cls]) < 4:
                examples[cls].append(s)
        rows.append({"len": n, "total": total, "classes": tally, "examples": examples,
                     "rendered_chars_per_key": rendered_chars / (total * n) if n else 0.0})
    return rows


def r8_decision(english_valid, hangul_valid, current_hangul=True):
    """R8's formula for the mode the buffer was typed in, expressed as the
    language the buffer would be DISPLAYED in after judging at this prefix.

    Italicised on purpose: the third line ("both valid / both invalid -> keep the
    current mode") is what makes a display quantity. With the current mode
    Hangul, a prefix that switches to English shows English; every other prefix --
    english-only after the formula fails, both-valid, both-invalid -- shows
    Hangul, because that is what R8 says to keep."""
    if english_valid and not hangul_valid:
        return "english"
    if hangul_valid and not english_valid:
        return "hangul"
    return "hangul" if current_hangul else "english"


def measure_prefix(engine, forms, hangul_layout):
    words = set(forms)
    keys = [letter_keys(f) for f in forms]
    usable = [k for k in keys if k]
    flips = 0       # >= 1 change: the intended switch, or worse
    flickers = 0    # >= 2 changes: it flipped and went back inside one word
    histogram = {}
    deepest = None
    for k in usable:
        decisions = []
        state = {"i": 0}

        def note(text, _k=k, _d=decisions, _s=state):
            # english_valid asks about the LATIN prefix typed so far (that is what
            # the dictionary lookup takes); hangul_valid asks about the rendering.
            i = _s["i"]
            _s["i"] += 1
            _d.append(r8_decision(_k[: i + 1] in words, engine.hangul_valid(text)))

        engine.render(k, hangul_layout, prefix_cb=note)
        # A display flip is a change of the judged language from the prefix
        # before it; the word starts in the current mode (Hangul).
        changes, seen = 0, decisions[0] if decisions else "hangul"
        for d in decisions[1:]:
            if d != seen:
                changes += 1
                seen = d
        histogram[changes] = histogram.get(changes, 0) + 1
        if changes >= 1:
            flips += 1
        if changes >= 2:
            flickers += 1
            if deepest is None or changes > deepest[1]:
                deepest = (k, changes)
    n = len(usable)
    return {
        "forms": n,
        "with_a_flip": flips,
        "with_a_flip_back": flickers,
        "share_with_a_flip_back": flickers / n if n else 0.0,
        "change_histogram": {str(c): histogram[c] for c in sorted(histogram)},
        "deepest": {"form": deepest[0], "changes": deepest[1]} if deepest else None,
    }


def measure_buffer(engine):
    rows = []
    for syl in (1, 21, 22, 64, 200, 1365, 1366, 2000):
        maxpre, word = engine.buffer_run(syl)
        rows.append({
            "syllables": syl,
            "bytes_typed": syl * BYTES_PER_SYLLABLE,
            "preedit_bytes": maxpre,
            "commit_bytes": len(word.encode()),
            "commit_chars": len(word),
            "lossless": len(word) == syl,
            "over_proposed_cap": maxpre > PROPOSED_CAP_BYTES,
        })
    # The raw ic path §E2's premise is about. Its `commit[STR_CAP]` accumulates,
    # so the question is how much one read can ever return: the commit is cleared
    # at the entry of every process call, and a finished syllable is at most three
    # characters, so the cap is either reachable in one read or it is not. Both
    # readings are taken -- the longest single read, and whether a read after the
    # whole word (the shape a caller takes when it does not read per key) keeps
    # everything.
    raw = []
    for syl in (1, 21, 22, 30):
        ic = engine.lib.hunmin_ic_new(engine.ctx, b"dubeolsik")
        longest_read, total = 0, 0
        for _ in range(syl):
            for k in "dks":
                engine.lib.hunmin_ic_process(ic, ord(k))
                c = (engine.lib.hunmin_ic_commit(ic) or b"").decode()
                longest_read = max(longest_read, len(c))
                total += len(c)
        late = (engine.lib.hunmin_ic_commit(ic) or b"").decode()
        engine.lib.hunmin_ic_free(ic)
        raw.append({"syllables": syl, "longest_single_read": longest_read,
                    "chars_read_per_key": total, "kept_by_one_late_read": len(late),
                    "lost_by_one_late_read": syl - len(late)})
    return {"shared_layer": rows, "raw_ic_single_read": raw, "str_cap": STR_CAP}


# ---------------------------------------------------------------------- main


def find_lib(explicit):
    if explicit:
        return explicit
    for cand in ("build-mingw/libhunmin.dll", "build/libhunmin.dll", "build-mingw/libhunmin.so", "build/libhunmin.so"):
        p = os.path.join(ROOT, cand)
        if os.path.exists(p):
            return p
    raise SystemExit("no built library found; pass --lib (see the docstring)")


def main(argv):
    ap = argparse.ArgumentParser(description="price SPEC-MODES E2/E3/E6 against the shipped tree")
    ap.add_argument("what", choices=["words", "space", "prefix", "buffer", "all"])
    ap.add_argument("--data", default=os.path.join(ROOT, "libhunmin", "data"))
    ap.add_argument("--lib", default=None)
    ap.add_argument("--hangul-layout", default="dubeolsik")
    ap.add_argument("--max-len", type=int, default=3, help="longest string the `space` sweep covers")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)

    wordpath, forms = load_words(args.data)
    result = {"data": os.path.relpath(wordpath, ROOT).replace(os.sep, "/"),
              "forms": len(forms), "hangul_layout": args.hangul_layout}
    if args.what in ("words", "all"):
        result["words"] = measure_words(forms)
    if args.what in ("space", "prefix", "buffer", "all"):
        engine = Engine(find_lib(args.lib), args.data)
        result["lib"] = os.path.relpath(find_lib(args.lib), ROOT).replace(os.sep, "/")
        if args.what in ("space", "all"):
            result["space"] = measure_space(engine, forms, args.hangul_layout, args.max_len)
        if args.what in ("prefix", "all"):
            result["prefix"] = measure_prefix(engine, forms, args.hangul_layout)
        if args.what in ("buffer", "all"):
            result["buffer"] = measure_buffer(engine)

    if args.json:
        json.dump(result, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    if "words" in result:
        w = result["words"]
        print("WORDS — %d forms in %s" % (w["forms"], result["data"]))
        print("  longest %r: %d chars / %d bytes; longest letters-only %d chars"
              % (w["longest"]["form"], w["longest"]["chars"], w["longest"]["bytes"],
                 w["longest_letters_only"]["chars"]))
        print("  mean length %.2f chars; %d forms are layout keys, %d are not (%s)"
              % (w["mean_chars"], w["form_keys"], w["no_keys"], "'/-" + "' excluded"))
        print("  the proposed %d-byte cap is %d syllables, %.1fx the longest word; %d forms exceed it"
              % (w["proposed_cap_bytes"], w["proposed_cap_syllables"],
                 w["headroom_over_longest"], w["forms_over_cap"]))
        print("  tail: %s" % " ".join("<=%d:%d" % (r["chars"], r["forms_le"]) for r in w["tail"]))
    if "space" in result:
        print("SPACE — every a-z string of length 1..%d, typed on %s" % (args.max_len, args.hangul_layout))
        print("  len | total | both | english_only | hangul_only | neither | switches-at-or-above")
        for r in result["space"]:
            c = r["classes"]
            print("  %3d | %5d | %4d | %12d | %11d | %7d"
                  % (r["len"], r["total"], c["both"], c["english_only"], c["hangul_only"], c["neither"]))
            print("      english_only examples %s" % " ".join(r["examples"]["english_only"]))
            print("      hangul_only  examples %s" % " ".join(r["examples"]["hangul_only"]))
    if "prefix" in result:
        p = result["prefix"]
        print("PREFIX — per-key judgement over %d forms" % p["forms"])
        print("  %d forms (%.2f%%) would reverse inside the word; mean %.2f reversals among them; deepest %s"
              % (p["with_a_reversal"], 100 * p["share_with_a_reversal"],
                 p["mean_reversals_when_present"],
                 "%r x%d" % (p["deepest"]["form"], p["deepest"]["reversals"]) if p["deepest"] else "none"))
    if "buffer" in result:
        b = result["buffer"]
        print("BUFFER — the shared layer, %s, at increasing word length" % args.hangul_layout)
        print("  syllables | bytes typed | preedit bytes | commit bytes | lossless | > 4 KiB")
        for r in b["shared_layer"]:
            print("  %9d | %11d | %12d | %12d | %8s | %s"
                  % (r["syllables"], r["bytes_typed"], r["preedit_bytes"], r["commit_bytes"],
                     r["lossless"], "yes" if r["over_proposed_cap"] else "no"))
        print("  raw hunmin_ic, STR_CAP %d:" % b["str_cap"])
        for r in b["raw_ic_single_read"]:
            print("    %4d syllables typed: longest single read %d chars, %d read per-key, "
                  "one late read keeps %d and loses %d"
                  % (r["syllables"], r["longest_single_read"], r["chars_read_per_key"],
                     r["kept_by_one_late_read"], r["lost_by_one_late_read"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
