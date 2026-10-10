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
"""
import ctypes
import ctypes.util
import sys
import time


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
