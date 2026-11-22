#!/usr/bin/env python3
"""Generate random valid key sequences for fuzzing the Hunmin engine.

Unlike sin3p2-ksgen.py which does exhaustive BFS search for specific target
syllables, this tool generates random valid key sequences by sampling from
the layout's key alphabet and using the engine's state machine to ensure
valid sequences. It's designed for fuzzing with AddressSanitizer/LeakSanitizer.

The engine's state machine determines what keys are valid at each step:
- Cho (choseong) phase: expects leading consonants
- Jung (jungseong) phase: expects vowels
- Jong (jongseong) phase: expects trailing consonants or space to commit
- Space commits the current syllable and starts a new one

--selfcheck replays the generated streams through hunmin-sim --batch: a
key outside the simulator's --keys grammar exits non-zero, which is the
failure that used to abort the nightly job (issue #309).
"""
import argparse
import json
import pathlib
import random
import subprocess
import sys
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

REPO = _COMMON_ROOT
DATA = REPO / "libhunmin/data"
SIM = REPO / "build/hunmin-sim"


def key_alphabet(layout_name):
    """Single-character keys from the layout, plus the space key and shifts.

    The layout JSON also names `space`, `enter` and `backspace`, but those are
    names in the JSON, not keystrokes: printing `space` into a --keys string
    replays s, p, a, c, e. Only single characters (and the explicit " " token)
    survive here. Uppercase tokens stand for a shifted base key; encode_keys()
    writes them in hunmin-sim's explicit-shift grammar.
    """
    d = json.loads((DATA / f"layouts/{layout_name}.json").read_text(encoding="utf-8"))
    keys = {k for k in d["keys"] if len(k) == 1 and 0x20 <= ord(k) < 0x7F}
    keys.add(" ")
    keys |= {k.upper() for k in keys if k.isalpha()}
    return sorted(keys, key=lambda c: (c.isupper(), c))


def encode_keys(keys):
    """Render generated tokens into the grammar tools/sim.c's keys_step reads.

    A bare backslash starts an escape (\\ = literal, \\s<char> = shift,
    \\b = backspace), so a backslash KEY has to be doubled. The nightly fuzz
    job died on iteration 125 with `--keys: unknown key escape \\ ` when the
    generator happened to pick a backslash as the first key; hunmin-sim then
    returned 2 with the loaded context still live, which LeakSanitizer
    reported as a startup-data leak (issue #309). Uppercase tokens become
    \\s<base>, the same explicit shift Tier C records.
    """
    out = []
    for k in keys:
        if k == "\\":
            out.append("\\\\")
        elif len(k) == 1 and k.isupper():
            out.append("\\s" + k.lower())
        else:
            out.append(k)
    return " ".join(out)


def trace_keystrokes(keys_list, layout, data_dir, sim_bin):
    """Run keys through hunmin-sim --trace and return the trace events."""
    inp = "\n".join(keys_list) + "\n"
    r = subprocess.run(
        [str(sim_bin), "--data", str(data_dir), "--layout", layout, "--trace"],
        input=inp, capture_output=True, text=True, timeout=60,
    )
    if r.returncode != 0:
        raise RuntimeError(f"hunmin-sim failed: {r.stderr.strip()[:400]}")
    events = []
    for line in r.stdout.splitlines():
        if line.strip():
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    return events


class FuzzKeyGenerator:
    """Generate random valid key sequences by simulating the engine's state machine."""

    def __init__(self, layout, max_len=50):
        self.layout = layout
        self.max_len = max_len
        self.keys = key_alphabet(layout)
        self.key_set = set(self.keys)

    def generate(self, seed=None):
        """Generate a single random valid key sequence."""
        if seed is not None:
            random.seed(seed)

        keys = []
        # Start with a random key
        if not self.keys:
            return []
        key = random.choice(self.keys)
        keys.append(key)

        # Track engine state roughly: 0=cho, 1=jung, 2=jong
        state = 0  # 0=cho, 1=jung, 2=jong
        remaining = self.max_len - 1

        while remaining > 0:
            # Determine valid next keys based on state
            valid_keys = self._valid_keys_for_state(key, state)

            if not valid_keys:
                # Try to commit with space
                if state == 2:
                    key = " "
                    keys.append(key)
                    state = 0
                    remaining -= 1
                    continue
                else:
                    # Force commit with space
                    key = " "
                    keys.append(key)
                    state = 0
                    remaining -= 1
                    continue

            key = random.choice(valid_keys)
            keys.append(key)

            # Update state based on key type
            state = self._next_state(state, key)

            remaining -= 1

        return keys

    def _valid_keys_for_state(self, last_key, state):
        """Return valid next keys based on current state and last key."""
        if state == 0:  # Cho phase - need consonants
            return [k for k in self.keys if self._is_consonant(k)]
        elif state == 1:  # Jung phase - need vowels
            return [k for k in self.keys if self._is_vowel(k)]
        else:  # state == 2, Jong phase - need consonants or space
            consonants = [k for k in self.keys if self._is_consonant(k)]
            return consonants + [" "]

    def _next_state(self, state, key):
        if key == " ":
            return 0
        if self._is_consonant(key):
            if state == 0 or state == 2:
                return 1  # Consonant in cho/jong -> jung
            else:  # state == 1
                return 2  # Consonant in jung -> jong
        elif self._is_vowel(key):
            return 1  # Vowel -> jung
        return state

    def _is_consonant(self, key):
        # Heuristic: consonants are typically lowercase single letters
        # that are not vowels. Shifted forms (uppercase) are also consonants.
        if len(key) == 1 and key.isalpha():
            return key.lower() not in "aeiou"  # Simplified
        return False

    def _is_vowel(self, key):
        if len(key) == 1 and key.isalpha():
            return key.lower() in "aeiou"
        return False


def selfcheck(layout, sim, length, count):
    """Replay generated streams through hunmin-sim --batch.

    Every alphabet member is forced into the first position, so a key that
    needs escaping cannot hide behind a lucky seed. One process replays all
    the lines; a bad escape makes --batch exit non-zero.
    """
    gen = FuzzKeyGenerator(layout, max_len=length)
    lines = [encode_keys(gen.generate(seed=s)) for s in range(count)]
    lines += [encode_keys([k] + gen.generate(seed=i)[1:])
              for i, k in enumerate(gen.keys)]
    r = subprocess.run(
        [str(sim), "--data", str(DATA), "--layout", layout, "--batch"],
        input="\n".join(lines) + "\n", capture_output=True, text=True, timeout=60,
    )
    if r.returncode != 0:
        print(f"selfcheck: hunmin-sim --batch exited {r.returncode}\n{r.stderr[:2000]}", file=sys.stderr)
        return 1
    if len(r.stdout.splitlines()) != len(lines):
        print(f"selfcheck: expected {len(lines)} replays, got {len(r.stdout.splitlines())}", file=sys.stderr)
        return 1
    print(f"selfcheck: {len(lines)} key streams from {layout} replayed cleanly")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Generate random valid key sequences for fuzzing.")
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--len", type=int, default=50, dest="length")
    ap.add_argument("--count", type=int, default=1)
    ap.add_argument("--seed", type=int)
    ap.add_argument("--sim", default=str(SIM), help="hunmin-sim used by --selfcheck")
    ap.add_argument("--selfcheck", action="store_true", help="replay generated streams and verify they parse")
    ap.add_argument("--selfcheck-count", type=int, default=32)
    a = ap.parse_args()

    if a.selfcheck:
        return selfcheck(a.layout, a.sim, a.length, a.selfcheck_count)

    if a.seed is not None:
        random.seed(a.seed)

    gen = FuzzKeyGenerator(a.layout, max_len=a.length)

    for i in range(a.count):
        keys = gen.generate()
        print(encode_keys(keys))

    return 0


if __name__ == "__main__":
    import argparse
    import sys
    sys.exit(main())
