#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Screenshot the root window of the current X display via GTK's own pixbuf
API (no xwd/ImageMagick needed -- python3-gi is already a D3 harness dep).

usage: xvfb-screenshot.py OUT.png
Exit 77 = no GTK via gobject-introspection (the same skip shape as
tests/e2e/gtk-harness.py).
"""
import sys


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        import gi

        gi.require_version("Gdk", "3.0")
        from gi.repository import Gdk  # pyright: ignore[reportAttributeAccessIssue] -- gi.repository is dynamic
    except Exception as exc:  # noqa: BLE001 - missing gi is a skip, not a crash
        print(f"xvfb-screenshot: no Gdk via gobject-introspection: {exc}", file=sys.stderr)
        return 77
    display = Gdk.Display.get_default()
    if display is None:
        print("xvfb-screenshot: no display", file=sys.stderr)
        return 1
    screen = display.get_default_screen()
    root = screen.get_root_window()
    width, height = root.get_width(), root.get_height()
    pixbuf = Gdk.pixbuf_get_from_window(root, 0, 0, width, height)
    if pixbuf is None:
        print("xvfb-screenshot: pixbuf_get_from_window failed", file=sys.stderr)
        return 1
    pixbuf.savev(sys.argv[1], "png", [], [])
    print(f"wrote {sys.argv[1]} ({width}x{height})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
