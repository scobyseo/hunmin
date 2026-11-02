#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""What tier C actually exercises of a frontend's wiring -- reported, not gated.

Tier C's claim is "this adapter maps native events to engine keys and applies the
answer correctly". That claim is only as wide as the events the corpus carries:
a corpus of mapped keys with a commit on every line cannot say anything about the
key that is DECLINED and must reach the client, and a corpus that never presses
the digit row cannot say anything about it either.

So the metric is made of what the traces contain, per layout:

    keys     distinct layout keys pressed / keys the layout maps
    events   consumed (eaten by the adapter) vs passed (reached the client)
    mods     events carrying CapsLock / Shift state
    policy   events replayed under each commit unit (meta.commit_mode)
    families tier-A goldens replayed, and which ones are missing

Nothing here fails. A gap is a statement about the corpus, not about the code,
and the decision whether to widen it belongs to whoever owns the tier -- see
docs/TESTING.md "Tier C" for what each gap implies.

    python3 tools/tier-c-wiring-coverage.py [--platform x11|win32|android]
"""
import argparse
import collections
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CONF = ROOT / "tests/conformance"

# Native event -> the character it means. Named keys come back as "<name>";
# anything outside the printable range is reported as its own token so it still
# counts as a distinct key rather than being silently dropped.
NAMED_SYMS = {0xFF08: "backspace", 0xFF0D: "return", 0xFF09: "tab",
              0xFF51: "left", 0xFF52: "up", 0xFF53: "right", 0xFF54: "down"}
NAMED_VKS = {0x08: "backspace", 0x0D: "return", 0x09: "tab",
             0x25: "left", 0x26: "up", 0x27: "right", 0x28: "down"}
NAMED_KEYCODES = {67: "backspace", 66: "return", 61: "tab",
                  21: "left", 22: "up", 23: "right", 20: "down"}


def key_of(event: dict, platform: str) -> str:
    if platform == "x11":
        ks = event.get("keysym", 0)
        if ks in NAMED_SYMS:
            return NAMED_SYMS[ks]
        return chr(ks) if 0x20 <= ks <= 0x7E else f"keysym:{ks:#x}"
    if platform == "win32":
        vk = event.get("vk", 0)
        if vk in NAMED_VKS:
            return NAMED_VKS[vk]
        if 0x30 <= vk <= 0x5A:  # digits, then VK_A..VK_Z
            return chr(vk).lower()
        return f"vk:{vk:#x}"
    kc = event.get("keycode", 0)
    if kc in NAMED_KEYCODES:
        return NAMED_KEYCODES[kc]
    # The android dialect writes printable keys as their ASCII value, not as
    # android.view.KeyEvent.KEYCODE_* (tools/gen-conformance.py ANDROID_KEYCODES),
    # so the traces drive the JNI shim's own table rather than the platform's.
    # Whatever the number is, it still counts as one distinct key here.
    return chr(kc) if 0x20 <= kc <= 0x7E else f"keycode:{kc}"


def layout_keys(layout: str) -> set[str]:
    path = ROOT / "libhunmin/data/layouts" / f"{layout}.json"
    if not path.is_file():
        return set()
    keys = json.loads(path.read_text(encoding="utf-8")).get("keys", [])
    return set(keys) if isinstance(keys, list) else set(keys)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", choices=["x11", "win32", "android"])
    a = ap.parse_args()
    platforms = [a.platform] if a.platform else ["x11", "win32", "android"]

    for platform in platforms:
        d = CONF / platform
        if not d.is_dir():
            continue
        per: dict[str, dict] = collections.defaultdict(
            lambda: {"keys": set(), "consumed": 0, "passed": 0, "caps": 0,
                     "shift": 0, "other_mods": 0, "events": 0, "policy": collections.Counter(),
                     "families": set()})
        for f in sorted(d.glob("*.trace.in")):
            lines = [json.loads(l) for l in f.read_text(encoding="utf-8").splitlines() if l.strip()]
            head = lines[0]["meta"]
            layout = head.get("layout", "?")
            st = per[layout]
            st["families"].add(head.get("source", "(none)").split(" (")[0])
            st["policy"][head.get("commit_mode", "word")] += 1
            for r in lines[1:]:
                if "in" not in r:
                    continue
                st["events"] += 1
                st["keys"].add(key_of(r["in"], platform))
                if r.get("consumed"):
                    st["consumed"] += 1
                else:
                    st["passed"] += 1
                mods = r["in"].get("mods", 0)
                # 0x02 is X LockMask / the corpus's caps bit; win32 has no caps
                # encoding at all (docs/trace-spec.md "in -- the native event").
                if mods & 0x02:
                    st["caps"] += 1
                if mods & 0x01:
                    st["shift"] += 1
                if mods & ~0x03:
                    st["other_mods"] += 1

        print(f"== {platform}")
        print(f"   {'layout':16s}{'events':>7}{'consumed':>9}{'passed':>7}"
              f"{'caps':>6}{'shift':>6}{'keys':>10}  families")
        for layout, st in sorted(per.items()):
            mapped = layout_keys(layout)
            pressed = {k for k in st["keys"] if len(k) == 1}
            pressed |= {k.upper() for k in pressed}
            covered = mapped & pressed if mapped else pressed
            keycell = f"{len(covered)}/{len(mapped)}" if mapped else f"{len(pressed)}/? "
            fam = ",".join(sorted(f for f in st["families"] if f != "(none)")) or "(unnamed)"
            print(f"   {layout:16s}{st['events']:>7}{st['consumed']:>9}{st['passed']:>7}"
                  f"{st['caps']:>6}{st['shift']:>6}{keycell:>10}  {fam[:44]}")
            missing = sorted(mapped - covered)
            if missing:
                print(f"   {'':16s}unpressed keys: {' '.join(missing)}")
            policy = ", ".join(f"{k}={v}" for k, v in sorted(st["policy"].items()))
            print(f"   {'':16s}commit policy (traces): {policy}")

    print("\npassed = the adapter declined the key and it reached the client. A corpus "
          "with passed=0 cannot observe the half of the frontend contract that decides "
          "what the client receives.")
    return 0


if __name__ == "__main__":
    sys.exit(main())