#!/usr/bin/env python3
"""Assert the keypad's keycap texts do not overlap (maintainer condition for
the hint/alternate display: "글자가 겹치지만 않는다면 내가 제안한 방식을 적용해 봐").

Reads a uiautomator hierarchy dump (XML file argument, or stdin) and, for every
keycap (content-desc key:/fn:/symbol:), checks that its non-empty TextView
children's bounds are pairwise disjoint. The main label is WRAP_CONTENT-centered
for exactly this reason: view bounds are the text box, not the whole key.

Exit 0 when no overlap, 1 when any pair intersects, 2 when no keycaps found
(a dump without the IME window proves nothing).
"""
import sys
import xml.etree.ElementTree as ET


def bounds(node):
    b = node.get("bounds", "")
    nums = [int(x) for x in b.replace("[", " ").replace("]", " ").replace(",", " ").split()]
    return nums if len(nums) == 4 else None


def overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def main():
    src = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    root = ET.parse(src).getroot()
    caps = [n for n in root.iter("node")
            if (n.get("content-desc") or "").startswith(("key:", "fn:", "symbol:"))]
    if not caps:
        print("keypad-overlap: no keycaps in the dump (IME window absent?)", file=sys.stderr)
        return 2
    bad = []
    checked = 0
    for cap in caps:
        texts = [(t.get("text"), bounds(t)) for t in cap.iter("node")
                 if t.get("class") == "android.widget.TextView" and t.get("text")]
        texts = [(txt, b) for txt, b in texts if b]
        for i in range(len(texts)):
            for j in range(i + 1, len(texts)):
                checked += 1
                if overlap(texts[i][1], texts[j][1]):
                    bad.append((cap.get("content-desc"), texts[i][0], texts[j][0]))
    if bad:
        for desc, a, b in bad:
            print(f"OVERLAP on {desc}: '{a}' vs '{b}'", file=sys.stderr)
        return 1
    print(f"keypad-overlap: {len(caps)} keycaps, {checked} text pairs, no overlap")
    return 0


if __name__ == "__main__":
    sys.exit(main())
