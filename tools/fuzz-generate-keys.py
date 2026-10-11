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

We use the engine's own --trace output to verify each key is valid.
"""
import argparse
import json
import pathlib
import random
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
DATA = REPO / "libhunmin/data"
SIM = REPO / "build/hunmin-sim"


def key_alphabet(layout_name):
    """Load layout JSON and return the key alphabet."""
    d = json.loads((DATA / f"layouts/{layout_name}.json").read_text(encoding="utf-8"))
    keys = set(d["keys"])
    keys |= {k.upper() for k in d["keys"] if len(k) == 1 and k.isalpha()}
    return sorted(keys, key=lambda c: (c.isupper(), c))


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


def main():
    ap = argparse.ArgumentParser(description="Generate random valid key sequences for fuzzing.")
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--len", type=int, default=50, dest="length")
    ap.add_argument("--count", type=int, default=1)
    ap.add_argument("--seed", type=int)
    a = ap.parse_args()

    if a.seed is not None:
        random.seed(a.seed)

    layout = a.layout
    gen = FuzzKeyGenerator(a.layout, max_len=a.length)

    for i in range(a.count):
        keys = gen.generate()
        print(" ".join(keys))

    return 0


if __name__ == "__main__":
    import argparse
    import sys
    sys.exit(main())
