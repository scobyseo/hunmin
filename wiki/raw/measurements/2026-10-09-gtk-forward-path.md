---
title: Measurement — the declined key over the GTK IM module, and the two frontends that do not behave alike
created: 2026-10-09
updated: 2026-10-10
type: measurement
status: active
ingested: 2026-10-09
tags: [measurement, fcitx5, e2e, testing, gtk, correction]
sources:
  - ../../../tests/e2e/gtk-harness.py
  - ../../../tests/e2e/run-d3.sh
  - ../../../tests/e2e/cases.tsv
  - ../../../docs/TESTING.md
  - ../../../wiki/raw/measurements/2026-10-09-where-space-is-emitted.md
confidence: high
---

# Measurement — the declined key over the GTK IM module

**What this is.** The continuation of [[2026-10-09-where-space-is-emitted]],
which listed as *not established*: "Non-XIM fcitx5 frontends. GTK, Qt and Wayland
`text-input-v3` … Not measured, and not assumed to behave like XIM." This is the
GTK half.

**The answer, on both GTK 3 and GTK 4:** the declined space reaches the
application. `dkssuds<space>` under dubeolsik leaves a real `GtkEntry` holding
`안녕ㄴ ` — the composed syllables, the flushed `ㄴ`, and one space character.

**And the interesting part is not that it works.** It is that it works by a
different mechanism on each of the two, and that the GTK 4 one is a function
whose entire body is empty. That is only visible if you measure both
separately.

## What was run

Same stack as tier D3, one variable changed: the client.

```
$ ctest --test-dir build -R engine-d3
    engine-d3-x11 ....... Passed   (fcitx5's XIM server -> tests/e2e/xim-harness.c)
    engine-d3-gtk3 ...... Passed   (fcitx5-frontend-gtk3 -> tests/e2e/gtk-harness.py)
    engine-d3-gtk4 ...... Passed   (fcitx5-frontend-gtk4 -> tests/e2e/gtk-harness.py)
```

`run-d3.sh` gained `HUNMIN_D3_CLIENT=gtk`, which changes nothing above the X
server — same private bus, same staged addon, same `FCITX_*` directories, same
`Loaded addon hunmin` readiness gate, same `cases.tsv`, same
`tests/e2e/xtest-type.py` injection. Only the harness and the expectation
change. The client is a real GTK application: the real `libgtk-3.so.0` /
`libgtk-4.so.1`, the real fcitx5 input method module, real `GdkEventKey`s from
the X server via XTEST.

Measured per case, both GTK versions:

```
ok: [dubeolsik/dkssuds ] -> TEXT:안녕ㄴ 
  MODULE:im-fcitx5.so=/usr/lib/x86_64-linux-gnu/gtk-3.0/3.0.0/immodules/im-fcitx5.so
  GTK_IM_MODULE:fcitx5
ok: [sin3-p2/kfcQ]      -> TEXT:각
ok: [sin3-p2/jk1]       -> TEXT:ㅇµ
```

The expectation is derived at run time from `hunmin-sim --trace` exactly as D3
does — tier A stays the single source of truth. For a GTK client the observable
is a buffer rather than an event stream, so `expected_text` folds the trace into
one string: every commit, plus the character of every key the engine declined,
concatenated. `dkssuds ` yields `COMMIT:안 COMMIT:녕 COMMIT:ㄴ` plus
`FORWARD:<space>`, i.e. `TEXT:안녕ㄴ `.

## Why GTK is not XIM, in mechanism

Over XIM the declined key becomes a *synthesised X key event*
(`src/frontend/xim/xim.cpp:664`, `if (!result) xcb_im_forward_event(...)`). A GTK
application never sees that: it speaks `GtkIMContext`, and
`fcitx5-frontend-gtk3`/`-gtk4` are different code with a different fallback.

Measured on the bus, `dbus-monitor` on the private D3 session filtering
`org.fcitx.Fcitx.InputContext1`, for the one `dkssuds ` case under GTK 3:

```
3  member=CommitString        (안, 녕, ㄴ — one per commit, in order)
1  member=ForwardKey   uint32 32   uint32 0   boolean false
```

keysym `32` is `space`. So fcitx5 *does* emit a `ForwardKey` signal over D-Bus,
and the GTK module's callback re-queues it:

```c
/* fcitx5-gtk 5.1.6 gtk3/fcitximcontext.cpp:1299 */
void _fcitx_im_context_forward_key_cb(FcitxGClient *, guint keyval, guint state,
                                      gboolean isRelease, void *user_data) {
    GdkEventKey *event = _create_gdk_event(context, keyval, state, isRelease);
    event->state |= (guint32)IgnoredMask;
    gdk_event_put((GdkEvent *)event);
}
```

`IgnoredMask` is the whole trick: the re-queued event goes back through
`fcitx_im_context_filter_keypress`, hits the `IgnoredMask` branch
(`:591`), and is handed to `fcitx_im_context_filter_keypress_fallback`, which
runs it past an xkb compose state and finally into `context->slave` — a
`gtk_im_context_simple_new()` (`:348`). `GtkIMContextSimple` inserts the
character. `_slave_commit_cb` re-emits it as the context's `commit`, and
`GtkEntry` shows a space.

**GTK 4 does not do this.** Same measurement, same bus, and `ForwardKey`
count **zero** — the case still passes. The reason is in the source:

```c
/* fcitx5-gtk gtk4/fcitximcontext.cpp:1228 */
void _fcitx_im_context_forward_key_cb(FcitxGClient *, guint, guint, gboolean,
                                      void *) {}
```

An empty function. GTK 4 gets the character by a different route: it tracks
events by pointer identity (`pending_events` / `handled_events` hash tables),
and when `fcitx_g_client_process_key_finish` returns false it *re-queues the
original event* with `gdk_display_put_event` (`:537`). The re-delivery hits the
`pending_events` branch (`:486`), which calls
`gtk_im_context_filter_keypress(fcitxcontext->slave, event)` directly — the same
`GtkIMContextSimple` slave, no `ForwardKey` involved.

So the two GTK frontends agree on the outcome and disagree on the path. One
test over both would have recorded "GTK emits the space" and lost this. Both are
ctest cases (`engine-d3-gtk3`, `engine-d3-gtk4`) for that reason.

## The harness could have measured nothing, three ways

Every one of these produced a *green* run at some point during this work, and
each is now a check in the harness or the runner. They are the substance of
this page as much as the answer is.

1. **`GtkEntry.has_focus()` lies about GTK 4.** No window manager runs on the
   test display, so nothing gives the window the X focus — under GTK 4 it stays
   `False` indefinitely (measured, 8 polls over 2.4 s) while `Gtk.Window.is_active()`
   is `True` and every injected key *does* land in the entry. A readiness gate
   written as `has_focus()` reports "harness never became ready" for a harness
   that is receiving every key: a message that is a lie. The gate is now
   `is_active()` under GTK 4, with the discrepancy written at the call site.
   GTK 4 also cannot be focused the GTK 3 way — its toplevel is a `GdkSurface`
   and the `GdkX11` 4.0 typelib has no `X11Window` — so the harness shells out
   to `xdotool windowfocus`, and the corresponding runner check *skips* GTK 4
   rather than pretending otherwise.
2. **`python3` on `PATH` is not a Python that can drive GTK.** The first `python3`
   on this host is a venv without `gi`, so the harness died instantly and the
   runner translated an interpreter problem into a statement about the input
   method. The runner now probes for a `python3` that can `import gi` and
   prints which one it picked.
3. **The greenest false positive: a GTK client that never used fcitx5's GTK
   module.** Run the same case with `GTK_IM_MODULE=xim` instead of `fcitx5` and
   **it passes**: `XMODIFIERS=@im=fcitx` is set by `run-d3.sh` for the XIM server,
   so GTK's `im-xim.so` connects to that same fcitx5 and the buffer fills
   correctly — by the very path this measurement exists to distinguish from the
   GTK one. The run is indistinguishable from a real GTK-frontend measurement by
   its result alone. The harness now prints the `im-fcitx5.so` actually mapped
   into its own `/proc/self/maps`, and `run_case` **refuses** any GTK run where
   that line is absent, with the reason printed. Verified: the `xim` control now
   fails with "fcitx5's GTK IM module is not loaded", and the real runs still
   pass.

## Falsifiability

The claim is "the declined space reaches the buffer". Two mutations were run
against it, both on the real stack.

**Negative control — the expectation without the space.** With the declined-key
character removed from `expected_text`, the case fails, showing the buffer does
carry the space and the check is the thing distinguishing them:

```
FAIL [dubeolsik/dkssuds ]
  expected: TEXT:안녕ㄴ
  actual:   TEXT:안녕ㄴ 
```

**Engine mutation — make the engine consume the space.** A copy of
`libhunmin/data` with `" "` added to dubeolsik's key table, so tier A now
reports `consumed: true` for the space and commits `ㄱ` as a preedit. The
expectation follows the data (`TEXT:안녕ㄴ`, no space) and the case **passes** on
both GTK 3 and GTK 4:

```
ok: [dubeolsik/dkssuds ] -> TEXT:안녕ㄴ
```

This is the load-bearing one. The GTK path is not "a space always appears" — the
buffer content tracks the engine's `consumed` flag, which is what makes the
`안녕ㄴ ` result evidence about the declined key rather than about GTK.

**Empty buffer.** `dkssuds` replaced by a bare space: `TEXT:<space>` on both
GTK versions, and `ForwardKey` count 0 on the bus.

## What is modelled rather than exercised

Recorded on the open, because each of these is a real boundary on what the
number above supports.

- **The client is built through GObject introspection, not compiled against
  `gtk/gtk.h`.** `pkg-config --exists gtk+-3.0 gtk4` fails on this host —
  no headers, and `sudo -n` always fails — but the runtime, both typelibs and
  both fcitx5 modules are installed. A Python client using `Gtk.Entry` is a real
  GTK client: same `libgtk`, same `GtkIMContext` multicontext, same
  `im-fcitx5.so`, same X server. **What differs is the binding, not the
  toolkit** — and the fcitx5 GTK module is *loaded into the GTK process*, so the
  module under test is byte-identical to what a compiled client would load. That
  is also why the runner *skips* (rc 77) rather than fails when the runtime or
  the module is absent.
- **The buffer is the assertion, so history is not.** `GtkEntry` prints its
  whole buffer on every change; `run_case` compares the last line. A client that
  inserted the space twice and deleted it would print the same final line as one
  that inserted it once. This is a property of measuring a buffer rather than an
  event stream, and it is why the engine mutation above was needed as well as
  the negative control.
- **Whether the character arrived as an fcitx5 commit or a fallback insertion is
  not observable from inside a GTK client.** `GtkIMContext` exposes no such
  distinction to a widget. The commit-vs-forward split is established by the
  `dbus-monitor` capture above, not by the buffer.
- **`xdotool windowfocus` is a foreign way to focus a GTK 4 window**, and
  **the client is not a real application**: no input-hint manipulation, no
  candidate window, no password field. A GTK application with different input
  hints takes a different branch in the module
  (`_fcitx_im_context_set_capability`, `fcitximcontext.cpp:1099-1160`).

## Still uncovered

- **Wayland `text-input-v3`.** Nothing here touches it. A GTK 4 application on
  Wayland uses `zwp_text_input_v3`, fcitx5's Wayland frontend is a different
  module with its own `forwardKey` handling, and it is not built, not installed,
  not connected to. **GTK over X11 must not be read as standing in for it.**
  `docs/TESTING.md`'s D4 line stands.
- **Qt.** `fcitx5-frontend-qt5`/`-qt6` are installed and untested; the Qt
  module's fallback path is a different implementation again.
- **Candidate window / F9** — the same gap D3 has, unchanged by this work.
- **The bus capture was one run, one case, per toolkit.** Enough to say "GTK 3
  emits it and GTK 4's handler is empty", because the empty handler is in the
  source; not enough to claim GTK 3 emits exactly one per declined key in
  general.
- **A real application** (gedit, gnome-text-editor, a browser) — installed, not
  driven.

## Where this touched the rest of the tree

`tests/e2e/gtk-harness.py` (the client and the `MODULE:` line),
`tests/e2e/run-d3.sh` (`HUNMIN_D3_CLIENT=gtk`, `expected_text`, the interpreter
probe, the focus/skip preconditions, the module guard), `CMakeLists.txt`
(`engine-d3-gtk3` and `engine-d3-gtk4`, both skipping rc 77), and
`docs/TESTING.md`'s D3 "not covered" list — corrected: GTK is now measured,
Wayland and Qt are not.