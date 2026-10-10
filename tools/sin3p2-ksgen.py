#!/usr/bin/env python3
"""Generate sin3-p2 keystroke sequences by searching the layout automaton.

WHY SEARCH, NOT A TABLE
-----------------------
sin3-p2 is a signal transduction automaton: every key value is a `case` tree
over D/E/F/T/P/J plus the wWzHh flags (346 of 768 value forms). What a key
emits depends on the slots *already* filled, so there is no static
"jamo -> key" table to read. Measured, not assumed:

    F       -> ㅏ   (vowel, alone)          f        -> ㅍ   (consonant, alone)
    cF      -> ㄱㅏ  (vowel after a cho)     cf       -> ㄱㅍ  (consonant again)
    CF      -> ㅔㅏ                         Cf       -> ㅔᅟᅦᇁ

The same key id yields a vowel or a consonant depending on state, and even the
two vowel branches of one key disagree. jong is worse: it never appears from a
single key at all, only on a consonant pressed when D and E are already filled
(`D&&!E&&F`). A table keyed on jamo cannot represent any of this; the automaton
is the model, so the generator searches it.

HOW
---
BFS over key strings, shortest first, using hunmin-sim --batch as the oracle
(~869k evaluations/second; a per-keystroke process spawn was 3.9 ms and made
the search take hours). Successors are deduped on *rendered output*, not on the
key string: two keystrokes that leave the engine in the same visible state are
the same node, which is what keeps the frontier small.
"""
import argparse
import json
import pathlib
import subprocess
import sys
from collections import deque

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA = REPO / "libhunmin/data"
SIM = REPO / "build/hunmin-sim"


def key_alphabet(layout):
    """The key ids the layout declares, read from the data file, not typed.

    Includes the shifted (uppercase) form of every letter key: 된소리 and
    other shift-only jamo (dubeolsik ㄲ = Shift+r, rendered 'R') live only
    under the shifted code point, so an alphabet of bare lowercase keys can
    never reach them -- measured: 까/끄 stayed not_found through depth 5
    (12.7M evaluations) purely because 'R' was absent from the alphabet.

    Sorted lowercase-first (a,b,...,z,A,B,...,Z), NOT plain sorted(): a plain
    key and its shifted twin can render the same text (dubeolsik k and K are
    both ㅏ), and BFS keeps the first keystroke it sees for a state. With
    uppercase sorted first (ASCII order), the shifted form won every tie and
    came out ~40% of the time; lowercase-first makes the plain form win, which
    is how the layout is actually typed. Shift stays reachable for the jamo
    that only exist there (ㄲ), it just loses the ties it has no right to win.
    """
    d = json.loads((DATA / f"layouts/{layout}.json").read_text(encoding="utf-8"))
    keys = set(d["keys"])
    keys |= {k.upper() for k in d["keys"] if len(k) == 1 and k.isalpha()}
    return sorted(keys, key=lambda c: (c.isupper(), c))


def evaluate(keys_list, layout):
    """Run many keystroke strings through the engine in one process."""
    if not keys_list:
        return []
    inp = "\n".join(keys_list)
    r = subprocess.run(
        [str(SIM), "--data", str(DATA), "--layout", layout, "--batch"],
        input=inp, capture_output=True, text=True, timeout=600,
    )
    if r.returncode != 0:
        raise RuntimeError(f"hunmin-sim --batch failed: {r.stderr.strip()[:400]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def rendered(rec):
    return rec["commit"] + rec["preedit"]


def _shift_count(keys):
    """Number of shifted (uppercase) letters in a keystroke string."""
    return sum(1 for c in keys if c.isupper())


def exact_hit(rec, target):
    """True when `target` is the whole tail of the rendered output.

    NOT `target in out`. That was the first version and it is wrong: BFS
    reached 가 as 'nkf', which renders 'ㅅ가' -- the syllable is there, but only
    because a bare ㅅ got committed in front of it, so the keystroke really
    types ㅅ first. Requiring the target to be the entire final preedit (or the
    entire commit when nothing is composing) makes the recorded keystroke the
    one a user would actually press for that syllable alone.

    A one-syllable target with an empty preedit is a commit that landed
    exactly, so commit == target counts too.

    Conjoining jamo (옛한글) targets are matched as the *tail* of the preedit
    rather than the whole of it, because a jamo composes onto whatever is
    already there -- e.g. aF renders 'ㅇᅟᅠᇰ' and the target ᄍ (U+11F0) is the
    final codepoint. A jamo buried mid-string (vV rendering 'ㅈ〮ᄍᅠᆽ', where
    ᄍ is followed by ᅠᆽ) is NOT a hit: the engine went on composing past it,
    so that keystroke does not stop at the jamo.
    """
    com, pre = rec["commit"], rec["preedit"]
    is_jamo = len(target) == 1 and (
              0x1100 <= ord(target) <= 0x11FF or
              0xA960 <= ord(target) <= 0xA97F or
              0xD7B0 <= ord(target) <= 0xD7FB)
    if is_jamo:
        if pre:
            return pre.endswith(target)
        return com.endswith(target)
    if pre:
        return pre == target
    return com == target


def find_all(targets, alphabet, layout, max_len, progress=None):
    """Shortest keystroke per target char, by BFS over engine states.

    Returns (found, missing, sim_runs). A target is 'found' when the rendered
    output contains it -- including while it is still in the preedit, because a
    syllable that has not committed yet is a legitimate state to record.

    ONE keystroke per target, not all of them. A plain key and its shifted twin
    can render the same text (dubeolsik: k and K are both ㅏ); recording every
    such twin would let one target flood the frontier with dozens of equal
    states and make the search far larger than the shortest-path question needs.
    Shift is still a real keyboard key, so the layout is not restricted to
    plain-only -- see verify_shift_variants() for proving that the shifted form
    of a found keystroke renders identically. Here the job is reachability, so
    the first shortest path wins and the rest are pruned.
    """
    targets = set(targets)
    remaining = set(targets)
    found = {}
    # node key = rendered state; value = shortest keystroke reaching it
    seen = {"": ""}
    frontier = deque([""])
    depth = 0
    runs = 0

    while frontier and remaining and depth < max_len:
        cands = [p + k for p in frontier for k in alphabet]
        runs += len(cands)
        for rec in evaluate(cands, layout):
            out = rendered(rec)
            keys = rec["keys"]
            # First keystroke to reach a *state* wins (rendered-output dedup).
            # But a target can be reached by several distinct key strings of the
            # same length (이 is both dl and dL), and those are different nodes,
            # so both stay in the search and the last one evaluated would win.
            # Prefer the one with fewer shifted keys: plain k and shifted K can
            # render identically, and shift should only appear where it is
            # required (ㄲ), never just because it tied. The lowercase-first
            # alphabet makes this win most ties already; this closes the rest.
            if out not in seen:
                seen[out] = keys
                frontier.append(keys)
            for t in remaining:
                if exact_hit(rec, t):
                    if t not in found or _shift_count(keys) < _shift_count(found[t]):
                        found[t] = keys
        remaining -= set(found)
        depth += 1
        if progress:
            progress(depth, len(found), len(remaining), runs)
    return found, sorted(remaining), runs


def verify_shift_variants(found, layout):
    """Prove each found keystroke still renders its text under shifted keys.

    dubeolsik vowels sit on plain keys but their shifted twins render the same
    vowel (k and K are both ㅏ), and shift is a real key. For every keystroke
    found, this re-renders it with every letter key also toggled to its shifted
    form and records which toggles leave the output byte-identical -- so the
    artefact states, per syllable, that the layout is typable with shift, not
    just without it. This is a bounded re-render (one per toggled key), not a
    search, so it does not enlarge the BFS.
    """
    variants = {}
    for target, keys in found.items():
        toggles = []
        for i, c in enumerate(keys):
            if c.isalpha():
                shifted = keys[:i] + c.upper() + keys[i+1:]
                if shifted != keys:
                    toggles.append(shifted)
        variants[target] = toggles
    return variants


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--words", default="",
                    help="space-separated whole-word targets (an eojeol); "
                         "each is searched as one string, which keeps the BFS "
                         "to a few syllables instead of a whole line")
    ap.add_argument("--text-file", type=pathlib.Path)
    ap.add_argument("--out", type=pathlib.Path, default=REPO / "out/sin3p2-ksgen.json")
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--max-len", type=int, default=4)
    a = ap.parse_args()

    if a.words:
        # Whole-eojeol targets: skip the single-char filter, exact_hit already
        # matches a multi-char target as pre == target. Each eojeol is searched
        # as one string so the BFS stays to a few syllables (see --words help).
        chars = set(a.words.split(" "))
        src = "--words"
    elif a.text_file:
        text = a.text_file.read_text(encoding="utf-8", errors="replace")
        chars = {c for c in text if not c.isspace()}
        src = str(a.text_file)
        # Targets may be Hangul syllables, ASCII letters, or conjoining jamo.
        # ASCII: this script types the English tokens through the engine too,
        # which is the whole point of the comparison. Precomposed Hangul
        # syllables: a decomposed syllable is several codepoints and the engine
        # never produces it from a keystroke. Conjoining jamo (U+1100-11FF)
        # plus the Hangul Jamo Extended-A/B blocks: 옛한글 (old hangul) text is
        # written in these, and 훈민정음-style preambles are nothing but them --
        # restricting targets to precomposed syllables silently dropped every
        # jamo in such a text.
        chars = {c for c in chars
                 if len(c) == 1 and (c.isascii() or
                      0xAC00 <= ord(c) <= 0xD7A3 or
                      0x1100 <= ord(c) <= 0x11FF or
                      0xA960 <= ord(c) <= 0xA97F or
                      0xD7B0 <= ord(c) <= 0xD7FB)}
    else:
        sys.exit("need --words or --text-file")
    if not chars:
        sys.exit("no usable targets in the input")

    alphabet = key_alphabet(a.layout)

    def prog(depth, nfound, nleft, runs):
        print(f"  depth {depth}: {nfound} found, {nleft} left, "
              f"{runs} sim runs", file=sys.stderr, flush=True)

    print(f"layout {a.layout}: {len(alphabet)} key ids, "
          f"{len(chars)} target syllables, max-len {a.max_len}", file=sys.stderr)

    found, missing, runs = find_all(chars, alphabet, a.layout, a.max_len, prog)

    # Re-render every shift toggle in one batch and keep only those whose output
    # equals the found keystroke's output. Bounded (one eval per toggled
    # letter), so it does not enlarge the search; it makes the artefact assert
    # that each syllable is typable with shift, not just without it.
    shift_ok = verify_shift_variants(found, a.layout)
    keys_list = list(found.values())
    base_out = {k: rendered(r) for k, r in zip(keys_list, evaluate(keys_list, a.layout))}
    probe = [v for vs in shift_ok.values() for v in vs]
    if probe:
        probe_out = {v: rendered(r) for v, r in
                     zip(probe, evaluate(probe, a.layout))}
        shift_ok = {
            t: [v for v in vs if probe_out.get(v) == base_out[found[t]]]
            for t, vs in shift_ok.items()
        }

    meta = {
        "layout": a.layout,
        "method": "BFS over engine states; hunmin-sim --batch is the oracle; "
                  "shortest keystroke wins",
        "why_no_table": "sin3-p2 key values are case trees over D/E/F/T/P/J and the "
                        "wWzHh flags, so the same key id emits a vowel or a "
                        "consonant depending on buffer state (measured: F=ㅏ, "
                        "f=ㅍ, cF=ㄱㅏ, cf=ㄱㅍ). jong appears only via D&&!E&&F. "
                        "No static jamo->key table can represent this.",
        "dedup": "on rendered output, not key string: same visible state is the "
                 "same node",
        "shift_variants": "per target, the shifted-key forms of the found "
                          "keystroke that render byte-identically -- proves the "
                          "syllable is typable with shift, without enlarging the "
                          "BFS (a bounded re-render, not a search)",
        "alphabet_size": len(alphabet),
        "max_len": a.max_len,
        "sim_runs": runs,
        "source": src,
        "date": "2026-10-05",
    }
    out = {"meta": meta, "keystrokes": found, "shift_variants": shift_ok,
           "not_found": missing}
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"\nfound {len(found)}/{len(chars)} in {runs} sim runs -> {a.out}",
          file=sys.stderr)
    if missing:
        print(f"not found: {''.join(missing)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())