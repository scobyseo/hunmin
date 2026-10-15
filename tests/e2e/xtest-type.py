#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier-D3 key injector: fake key events through XTEST (libXtst, ctypes).

Xvfb has no evdev/uinput backend (and /dev/uinput is root-only in CI),
so HID-level injection is impossible there; XTEST enters the full stack
at the X server, exercising XKB -> fcitx5 XIM -> addon -> libhunmin ->
client. See docs/TESTING.md tier D3.

usage: xtest-type.py DISPLAY 'KEYS' [DELAY_MS]
KEYS: printable ASCII; uppercase implies Shift (US Xvfb layout).
      ^  prefix sends Control+space (fcitx5 trigger key).
      {NAME} sends the X keysym for a function/navigation key, e.g.
      {F9} -- the Hanja window trigger the engine consumes. Used by the
      tier-D3 candidate variant, which is the only tier that actually
      draws the window; see run-d3.sh's candidate mode.
"""
import ctypes
import ctypes.util
import re
import sys
import time

# Named keysyms, XK_* values. {F9} in a case file is what opens the candidate
# window; the rest exist so the same case file can drive navigation and the
# keys a case needs to settle the window's behaviour after it opens.
# XK_F9 = 0xFFC6 -- not 0xFFC2 (F5): sending F5 was measured to commit the
# syllable instead of opening the window, because the engine declines it as an
# unmapped keysym (tools/repro-f9-candidates.sh header, 2026-10-15).
NAMED_KEYSYMS = {
    "F9": 0xFFC6,
    "Return": 0xFF0D,
    "Escape": 0xFF1B,
    "Down": 0xFF54,
    "Up": 0xFF52,
    "Page_Down": 0xFF56,
    "Page_Up": 0xFF55,
}


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    display, keys = sys.argv[1], sys.argv[2]
    delay_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 40

    libx11 = ctypes.util.find_library("X11")
    libxtst = ctypes.util.find_library("Xtst")
    if not libx11 or not libxtst:
        print("xtest-type: libX11/libXtst not found", file=sys.stderr)
        return 2

    x11 = ctypes.CDLL(libx11)
    xtst = ctypes.CDLL(libxtst)
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    xtst.XTestFakeKeyEvent.argtypes = [
        ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong
    ]

    dpy = x11.XOpenDisplay(display.encode())
    if not dpy:
        print(f"xtest-type: cannot open display {display}", file=sys.stderr)
        return 1

    def send(code: int, down: bool) -> None:
        xtst.XTestFakeKeyEvent(dpy, code, 1 if down else 0, 0)
        x11.XFlush(dpy)
        time.sleep(delay_ms / 1000)

    # printable ASCII == its keysym; XTEST does not infer Shift, so
    # uppercase keys get an explicit Shift_L press/release pair.
    shift_l = x11.XKeysymToKeycode(dpy, 0xFFE1)  # XK_Shift_L
    ctrl_l = x11.XKeysymToKeycode(dpy, 0xFFE3)  # XK_Control_L
    space = x11.XKeysymToKeycode(dpy, 0x20)     # XK_space

    def send_ctrl_space() -> None:
        if not ctrl_l or not space:
            print("xtest-type: missing Control_L or space keycode", file=sys.stderr)
            return
        send(ctrl_l, True)
        send(space, True)
        send(space, False)
        send(ctrl_l, False)

    i = 0
    while i < len(keys):
        # {NAME} keysym token: scan it out before the single-character cases,
        # so a case file can spell `kfs{F9}` or `{F9},Return`.
        if keys[i] == "{":
            end = keys.find("}", i)
            if end < 0:
                print(f"xtest-type: unterminated '{{' in key sequence", file=sys.stderr)
                return 2
            name = keys[i + 1 : end]
            sym = NAMED_KEYSYMS.get(name)
            if sym is None:
                print(f"xtest-type: unknown named key {{{name}}}", file=sys.stderr)
                return 2
            code = x11.XKeysymToKeycode(dpy, sym)
            if not code:
                print(f"xtest-type: no keycode for {{{name}}}", file=sys.stderr)
                return 2
            send(code, True)
            send(code, False)
            i = end + 1
            # A `,` separator between named tokens: purely cosmetic, so
            # `{F9},Return` reads like the repro script's AFTER argument.
            if i < len(keys) and keys[i] == ",":
                i += 1
            # F9 opens the candidate window ASYNCHRONOUSLY: the key is
            # consumed, then fcitx5 -> addon -> libhunmin -> classicui draws
            # the window on the event loop. A Return injected 40ms later
            # arrives before candidateActive() is true, so it is NOT handled
            # as a candidate key -- it flushes the source syllable to the
            # client and the rest of the sequence lands as Latin garbage.
            # Measured 2026-10-15: `kfs{F9}Return` at 40ms committed
            # 간|ᅟᅥᆸ|ㅋ|더 instead of 刊. The repro script settles 1.5s after
            # F9 for exactly this reason; a shorter, still-generous pause
            # keeps the case file readable without a 1.5s per-token cost.
            if name == "F9":
                time.sleep(0.8)
            continue
        ch = keys[i]
        if ch == '^' and i + 1 < len(keys) and keys[i + 1] == ' ':
            send_ctrl_space()
            i += 2
            continue
        if ch == " ":
            # Space was refused here until 2026-10-09. It is the key whose
            # ownership is disputed -- does the engine commit it, or does the
            # adapter pass it through -- so refusing to inject it meant tier D3
            # could not settle the question it exists to answer. Its keysym is
            # its own codepoint, so it needs no special case beyond the range
            # test below admitting it.
            send(space, True)
            send(space, False)
            i += 1
            continue
        if not (" " < ch <= "~"):
            print(f"xtest-type: unsupported key {ch!r}", file=sys.stderr)
            return 2
        code = x11.XKeysymToKeycode(dpy, ord(ch))
        if not code:
            print(f"xtest-type: no keycode for {ch!r}", file=sys.stderr)
            return 2
        if ch.isupper():
            if not shift_l:
                print("xtest-type: no Shift_L keycode", file=sys.stderr)
                return 2
            send(shift_l, True)
            send(code, True)
            send(code, False)
            send(shift_l, False)
        else:
            send(code, True)
            send(code, False)
        i += 1

    time.sleep(0.1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
