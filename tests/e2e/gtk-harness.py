#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier-D3 GTK client harness: a real GTK window with a GtkEntry, built
through GObject introspection, that echoes its buffer contents as TEXT:<utf8>
on stdout.

Why this exists
---------------
fcitx5's XIM server forwards a key the input method declined back to the client
as a synthesised X key event (src/frontend/xim/xim.cpp:664). Over XIM that is
one of two ways a declined key reaches the application; a GTK application does
not speak XIM, it speaks GtkIMContext, and the fcitx5 GTK IM module
(/usr/lib/.../gtk-3.0/3.0.0/immodules/im-fcitx5.so, fcitx5-frontend-gtk3) is a
different piece of code with its own fallback path. So "the space arrives"
cannot be inferred from the D3 XIM result.

This harness is deliberately the SAME SHAPE as tests/e2e/xim-harness.c -- a
minimal client that prints what the input method gave it, nothing else -- so
that the two frontends are comparable. It is a genuine GTK application: the
real libgtk, the real GTK input method module, driven by real X keys through
XTEST exactly as the XIM harness is.

WHAT IS MEASURED AND WHAT IS MODELLED
------------------------------------
Measured: the text that ends up in a real GtkEntry's buffer. That is the
application-visible outcome, the same class of assertion as tier E2's
`uiautomator` dump.

Not measurable from inside a GTK client, and therefore NOT claimed here:
  * whether the character arrived as an fcitx5 commit or as a fallback
    filterKeyEvent() insertion. GTK's GtkIMContext API exposes no such
    distinction to a widget -- the module inserts text into the buffer either
    way. The mechanism is established by reading the module's source, and
    recorded on the measurement page, not asserted by this harness.
  * whether the composition was drawn as a client-side preedit or inserted
    into the buffer. See PREEDIT below; the buffer alone cannot tell.

HARNESS PROTOCOL (consumed by tests/e2e/run-d3.sh)
  READY              the window is mapped and the entry holds the X focus
  MODULE:...         which GTK IM module library is mapped into this process
  TEXT:<content>     the entry's buffer, on every change and once at the end
  CASESTART:<id>     a case is starting, on a FRESH entry, already focused
  CASEFAIL:<id>      the fresh entry never took the X focus; the case cannot run

READY must be unbuffered (flush after write) because the runner greps a
redirected stdout; see the same note in xim-harness.c about setbuf.

ONE HARNESS PER LAYOUT GROUP
----------------------------
run-d3.sh starts one harness per layout group and reuses it. Between cases the
runner sends SIGUSR1 after writing the case id to HUNMIN_CASE_ID_FILE; the
handler schedules on_case_marker() on the main loop, which throws the GtkEntry
away and builds a new one, and the marker is printed only once that new entry
holds the X focus.

The new entry is the whole reason reuse is safe here. Two things would
otherwise carry over, and both are invisible in a green run:

  * the BUFFER. A GtkEntry has one buffer and it accumulates. Case 2 would
    compare "안녕" + its own text against case 2's expectation.
  * the INPUT CONTEXT, and with it the engine's composition. **Four of the
    eight cases leave a preedit in flight** -- measured with
    `hunmin-sim --trace`: `dubeolsik dkssuds` ends on preedit ㄴ,
    `sin3-p2 kfcQ` on ㅒ, and the two long `stream` cases on 세 and 를.
    Reusing the GtkEntry would carry that into the next case in the group.

A fresh GtkEntry gives a fresh buffer AND a fresh GtkIMContext, which is a
fresh fcitx5 input context and therefore a fresh hunmin IC with no preedit --
exactly the state each case started from when the harness was started per
case. That is why this is a per-case entry and not `entry.set_text("")`:
clearing the text does not touch the input context.

The XIM client is NOT reused, for the same reason and without the remedy: an
XIC carries the server-side composition and the XIM client has no per-case
reset for it here. See the comment at the top of run_case in run-d3.sh.

usage: gtk-harness.py [--gtk-version 3.0|4.0]
env:   GTK_IM_MODULE=fcitx5  (set by the runner; this harness does not set it,
       because which module a GTK client loads is part of what is measured)
       HUNMIN_CASE_ID_FILE  the case id, read on each SIGUSR1
"""

import argparse
import os
import re
import sys
import time

sys.stdout.reconfigure(line_buffering=True)

# The GTK 4 focus workaround addresses the window by title through xdotool, so
# the title has to be one the harness can find and unique enough not to match
# anything else on the (empty, private) test display.
WINDOW_TITLE = "hunmin-gtk-harness"


def out(line):
    sys.stdout.write(line + "\n")
    sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gtk-version", default=os.environ.get("HUNMIN_GTK", "3.0"))
    args = ap.parse_args()

    # The case id control channel: run-d3.sh rewrites this file and then sends
    # SIGUSR1, once per case, for as long as this harness serves its layout
    # group. A file rather than an argument because the harness outlives the
    # case it was started for. Same choice, and the same reason, as
    # HUNMIN_CASE_ID_FILE in xim-harness.c.
    case_id_file = os.environ.get("HUNMIN_CASE_ID_FILE")

    try:
        import gi

        gi.require_version("Gtk", args.gtk_version)
        if not args.gtk_version.startswith("4"):
            gi.require_version("Gdk", "3.0")
        from gi.repository import GLib, Gtk
        if not args.gtk_version.startswith("4"):
            from gi.repository import Gdk
    except Exception as exc:  # noqa: BLE001 - the runner turns this into a skip
        out("HARNESS-ERROR: no Gtk %s via gobject-introspection: %s"
            % (args.gtk_version, exc))
        return 77

    gtk4 = args.gtk_version.startswith("4")

    # state["entry"] is the GtkEntry the keys go into, and it is REPLACED once
    # per case (see make_entry). Every reference below goes through cur_entry()
    # rather than closing over a name, because after the first swap the name
    # would be a different widget than the one on screen.
    state = {"entry": None, "ready": False, "printed_text": False,
             "pending": None, "pending_at": 0.0}

    def cur_entry():
        return state["entry"]

    win = Gtk.Window(title=WINDOW_TITLE)
    win.set_default_size(480, 60)
    entry = Gtk.Entry()
    state["entry"] = entry
    if gtk4:
        win.set_child(entry)
    else:
        win.add(entry)

    entry.set_width_chars(40)

    def on_changed(widget):
        out("TEXT:" + widget.get_text())

    entry.connect("changed", on_changed)

    def make_entry():
        """Build a fresh GtkEntry to replace the current one.

        This is the per-case reset, and it is a new WIDGET on purpose: a new
        buffer AND a new GtkIMContext, so no text and no in-flight composition
        survive into the next case. The first case does not come through here
        -- it is the entry built above, in a process that has never run a case
        -- so its state is clean by construction.
        """
        new = Gtk.Entry()
        new.set_width_chars(40)
        new.connect("changed", on_changed)
        if gtk4:
            win.set_child(new)
        else:
            old = state["entry"]
            if old is not None:
                win.remove(old)
            win.add(new)
            # The toplevel is already visible, so show_all() here is what puts
            # the replacement on screen; GTK 4 maps it through set_child.
            win.show_all()
        new.set_can_focus(True)
        new.grab_focus()
        state["entry"] = new
        return new

    # Which IM module library is actually mapped into this process. Without
    # this the harness cannot tell "the fcitx5 GTK module declined the space
    # and did nothing" from "the fcitx5 GTK module was never loaded and GTK
    # handled the keys itself" -- which would make the run a measurement of
    # GTK's built-in fallback rather than of the fcitx5 GTK frontend.
    def report_module():
        mapped = []
        try:
            with open("/proc/self/maps") as fh:
                for line in fh:
                    m = re.search(r"(\S*im-fcitx5\.so\S*)", line)
                    if m:
                        p = m.group(1)
                        if p not in mapped:
                            mapped.append(p)
        except OSError as exc:
            out("MODULE-ERROR: %s" % exc)
        out("MODULE:" + ("im-fcitx5.so=" + ",".join(mapped) if mapped else "none"))
        out("GTK_IM_MODULE:" + os.environ.get("GTK_IM_MODULE", "<unset>"))

    if gtk4:
        win.present()
    else:
        win.show_all()

    entry.set_can_focus(True)
    entry.grab_focus()

    # No window manager runs on the test display, so nothing will ever give the
    # window the X input focus on its own: measured, GtkEntry.has_focus() stays
    # False indefinitely under a bare Xvfb and every injected key is dropped
    # before it reaches the toolkit. tests/e2e/xim-harness.c solves the same
    # problem with xcb_set_input_focus(); the GTK 3 equivalent is
    # GdkWindow.focus(), which is what a WM would do on the client's behalf.
    #
    # Called on a timer rather than once: the first call can land before GDK has
    # finished mapping the toplevel and is then silently ineffective (measured
    # -- has_focus() was still False on the next tick and True only after the
    # second attempt). Retrying is not a fudge here; it is the same fact as "no
    # WM will ever do it for us". It also has to keep running for the WHOLE life
    # of the process now that one harness serves a layout group: each case
    # installs a new GtkEntry, which under GTK 3 leaves the window without the
    # X focus again until the next tick re-takes it.
    #
    # GTK 3 needs GdkWindow.focus(), which is what a window manager would do on
    # the client's behalf. GTK 4 has no such call on Gtk.Window (its toplevel
    # is a GdkSurface, and the GdkX11 typelib does not expose the XID -- there
    # is no GdkX11.X11Window in 4.0, only in 3.0), so the runner-side
    # equivalent is used instead: xdotool windowfocus on this window's title,
    # which is an XSetInputFocus by another name. See take_focus().
    if not gtk4:
        def take_focus():
            gw = win.get_window()
            e = cur_entry()
            if gw is not None and not e.has_focus():
                gw.focus(Gdk.CURRENT_TIME)
            return True

        GLib.timeout_add(200, take_focus)
    else:
        import subprocess

        def take_focus():
            if win.is_active():
                return True
            found = subprocess.run(
                ["xdotool", "search", "--name", WINDOW_TITLE],
                capture_output=True, text=True, check=False)
            ids = [x for x in found.stdout.split() if x.strip()]
            if ids:
                subprocess.run(["xdotool", "windowfocus", ids[-1]],
                               check=False)
            return True

        GLib.timeout_add(300, take_focus)

    def focused():
        # "Can a key reach the entry" -- which is what the runner's readiness
        # gate actually needs to know.
        #
        # GTK 3 and GTK 4 disagree here, and the difference is measured rather
        # than assumed: under GTK 4 the toplevel holds the X focus
        # (Gtk.Window.is_active() True) and injected keys DO land in the entry,
        # while GtkEntry.has_focus() stays False. Gating GTK 4 on
        # has_focus() therefore reports "harness never became ready" for a
        # harness that is receiving every key -- the run_d3 message would be a
        # lie. GTK 3 reports both consistently once focus has been taken.
        if gtk4:
            return win.is_active()
        return cur_entry().has_focus()

    def keys_reach_current_entry():
        """focused(), plus "the widget that would receive the key belongs to
        the entry this case installed".

        The extra clause only matters after a case boundary, and only for
        GTK 4. win.is_active() describes the toplevel and does not change when
        make_entry() swaps the child, so on its own it reports "ready"
        immediately -- while keys are still going to the previous GtkEntry,
        which is no longer on screen.

        Measured, GTK 4 under this display: win.get_focus() is never the
        GtkEntry itself, it is the GtkText GtkEntry is built on, so the test
        has to walk up. The parent chain is Text -> Entry -> Window, and it
        follows the swap -- the GtkText's address changes with the entry. Asking
        for the entry directly would have made every case time out.
        """
        if not focused():
            return False
        if not gtk4:
            return True
        node = win.get_focus()
        while node is not None:
            if node is cur_entry():
                return True
            node = node.get_parent()
        return False

    # How long a case may wait for its fresh entry to take the X focus before
    # the harness says so. Not a timing shortcut: the marker is what the runner
    # gates key injection on, so a case that never focuses has to be reported
    # as such rather than left to time out on the runner's own clock with a
    # diff that says nothing about which half failed.
    CASE_FOCUS_TIMEOUT = 15.0

    def ready_check():
        # This timer now runs for the WHOLE life of the process rather than
        # stopping at READY: a reused harness has to keep noticing that the
        # next case's fresh entry has taken the focus.
        if state["pending"] is not None:
            if keys_reach_current_entry():
                cid = state["pending"]
                state["pending"] = None
                # The buffer of a fresh entry is empty; print it so the
                # transcript shows the case starting from nothing rather than
                # inheriting the previous case's text.
                out("TEXT:" + cur_entry().get_text())
                out("CASESTART:" + cid)
            elif time.monotonic() - state["pending_at"] > CASE_FOCUS_TIMEOUT:
                cid = state["pending"]
                state["pending"] = None
                out("CASEFAIL:" + cid)
            return True
        if not state["ready"]:
            if keys_reach_current_entry():
                state["ready"] = True
                report_module()
                out("READY")
                # one dump with an empty buffer, so the runner can tell
                # "nothing arrived" from "the harness died"
                state["printed_text"] = True
                out("TEXT:" + cur_entry().get_text())
        return True

    GLib.timeout_add(100, ready_check)

    # GLib.MainLoop rather than Gtk.main(): Gtk.main() does not exist in the
    # GTK 4 typelib (measured -- AttributeError), and the loop is the same
    # object in both. Gtk.main_quit() is gone for the same reason.
    loop = GLib.MainLoop()

    def on_terminate():
        out("TEXT:" + cur_entry().get_text())
        loop.quit()
        return False

    import signal

    signal.signal(signal.SIGTERM, lambda *_: GLib.idle_add(on_terminate))

    # SIGUSR1 is the case marker, process to process, for the reason spelled out
    # in xim-harness.c's on_case_marker: a synthetic keystroke could be consumed
    # by the engine or forwarded back to the client, and either way a reused
    # harness would compare the previous case's tail against the next case's
    # head. No keystroke crosses the X server here at all.
    #
    # The handler itself does nothing but GLib.idle_add, which is what the
    # SIGTERM handler above already does and is the documented way to hand work
    # to the main loop from a signal context: GLib wakes its own poll from the
    # signal handler, so there is no need for the self-pipe xim-harness.c
    # builds. The case id itself travels through HUNMIN_CASE_ID_FILE because
    # reading a file is not something a signal handler should be doing either.
    def on_case_marker(*_):
        cid = ""
        if case_id_file:
            try:
                with open(case_id_file) as fh:
                    cid = fh.read().strip()
            except OSError as exc:
                out("CASEID-ERROR: %s" % exc)
        make_entry()
        state["pending"] = cid
        state["pending_at"] = time.monotonic()
        return False

    signal.signal(signal.SIGUSR1, lambda *_: GLib.idle_add(on_case_marker))

    loop.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())