# Tier D3 — root cause and fix: the harness never spoke XIM to fcitx5

**Status: FIXED. `engine-d3-x11` passes all 3 cases and the full `ctest` suite
passes (ctest is green; 140 as of the P5 merge).**

This is the record of why D3 spent a long time looking nearly green while
measuring nothing, and of the four separate defects that were stacked behind
the first one. Each was found only after the one before it was removed.

## The first and decisive defect: two different XIM implementations

`tests/e2e/xim-harness.c` called `XOpenIM()`/`XCreateIC()` and linked only
`X11::X11`, so those resolved inside **libX11's own built-in XIM**. fcitx5's
XIM server is **libxcb-imdkit**. Client and server were different XIM
implementations for the entire life of tier D3. Measured three ways:

**1. The harness binary never loads dkit.**

```
$ ldd build/xim-harness | grep -i imdkit      # (no output)
$ ldd build/xim-harness | grep X11
        libX11.so.6 => /usr/lib/x86_64-linux-gnu/libX11.so.6
```

`LD_DEBUG=libs` on the harness confirms only `libX11.so.6` is searched.
The server side *did* use dkit: fcitx5's `libxim.so` links it.

**2. libX11's XIM succeeds with no server present at all.** A probe calling
`XOpenIM` on a bare Xvfb, with `XIM_SERVERS` absent and no fcitx5 running:

```
XIM_SERVERS atom = 0
XIM_SERVERS: unreadable
literal '@server=fcitx' atom = 233, owner = 0x0
XOpenIM (libX11 built-in) -> 0x60a73ebd7570     <- non-NULL
  styles offered = 2
```

It returns a usable IM and offers styles **with nothing to talk to**.
`XMODIFIERS=@im=nonexistent-xyz` behaves identically — the name is never
checked, because it never resolves a server.

**3. That fallback is libX11's local IM.** libX11 exports the whole local-IM
implementation:

```
$ nm -D libX11.so.6 | grep ' T _XimLocal' | head
  _XimLocalCreateIC  _XimLocalFilter  _XimLocalOpenIM  _XimLocalUtf8LookupString ...
```

So the harness accepted every key itself. That is what produced the
`COMMIT:k` / `COMMIT:f` output that was read as "the harness handles keys
itself, so the raw-key symptom says nothing" — correct, and never connected to
*why* it handled them.

### Why nothing reached the server

With the client on libX11's local IM, no XIM protocol traffic is generated, so
no `XIM_CONNECT` and no `CREATE_IC`. Instrumenting the server through dkit's
own `DebugLog` showed only initialization and property chatter:

```
[dkit] XIM basic init: 1
[dkit] XIM Reset property. 1791017498
```

dkit logs every opcode in `_xcb_im_handle_message` (`-- XIM_CONNECT`,
`-- XIM_CREATE_IC`, …, `imdkit.c:585-690`), so a working handshake would be
unmissable. `xcb_im_filter_event` was wrapped to log every event fcitx5 feeds
it; over a full case run it saw **7 events, none a ClientMessage**:

```
2x event type=31   (ColormapNotify)
2x event type=28   (SelectionClear)
2x event type=90 / 89  (XGE)
```

Hence `fcitx5-diagnose`'s `Group [x11::96] has 0 InputContext(s)`: no IC was
ever created, because nothing ever asked for one. The `XIM Key Event x 0` that
led to the earlier diagnosis was a *symptom of zero contexts*, not a filter
failure.

## The fix

The harness must use dkit's **client** API so client and server agree on one
implementation. `libxcb-imdkit` exports the client half (`xcb_xim_create`,
`xcb_xim_open`, `xcb_xim_create_ic`, `xcb_xim_filter_event`,
`xcb_xim_forward_event`, `xcb_xim_set_ic_focus`, …), so `xim-harness.c` is
rewritten against `xcb_xim_*` and linked against `libxcb-imdkit`.

Two API details worth keeping:

- **The dev headers are not shipped** on Debian/Ubuntu and there is no
  `xcb-imdkit.pc`, so `CMakeLists.txt` finds `libxcb-imdkit.so.1` by full path
  (only the SONAME is installed — there is no `.so` development symlink) and
  the harness declares the small API slice it uses. The struct member order of
  `xcb_xim_im_callback` is part of the ABI and must not be reordered.
- **`setbuf(stdout, NULL)` is load-bearing.** stdout is a redirect that
  `run-d3.sh` greps for `READY`, so it is fully buffered by default and `READY`
  would sit in the buffer until exit. Without it the symptom is a misleading
  "harness never became ready" even though the handshake succeeded.

The server trace now shows the complete protocol, and `fcitx5-diagnose` reports
`focus:1`:

```
-- XIM_CONNECT → XIM_OPEN → XIM_QUERY_EXTENSION → XIM_ENCODING_NEGOTIATION
  → XIM_CREATE_IC → XIM_SET_IC_FOCUS → XIM_FORWARD_EVENT → XIM_SYNC_REPLY
```

## Three more defects, exposed only after the first was fixed

Rewriting the harness made D3 honest, and honest immediately showed three
separate bugs that had been hiding behind the false one. The first is not a
bug but a decision, recorded here because it was nearly shipped as a test.

### 1. F9's Hanja window is D2's job, not D3's

A F9 case was tried here (a `pre`-key column in `tests/e2e/cases.tsv`, so the
expectation could be stated outside tier A) and removed: it passed no matter
what the adapter did. See the measurements and the general rule under
"What remains open" below. `cases.tsv` keeps two columns only — a case whose
expectation cannot come from `hunmin-sim --trace` does not belong in D3.

### 2. `fcitx5-remote -s hunmin` silently did nothing

It returns **rc=0** — success — and the active IM stays `keyboard-us`:

```
a1 -s rc=0 out=[] -o rc=0
a1 -n=[keyboard-us] -m(hunmin)=[hunmin] -q=[Default] state=[2]
```

`-m hunmin` resolves fine and the context is active (`state=2`), so the switch
*looks* like it worked. `Instance::setCurrentInputMethod` (instance.cpp:2073)
looks the name up in the current group's item list and returns silently if
`canTrigger()` is false or no context is focused. What actually selects the
engine is `DefaultIM` in the profile, so the `-s` call is **removed** rather
than retried — retrying it ten times changes nothing.

### 3. The profile's item order decides the active IM

With `DefaultIM=hunmin` written correctly and read correctly, the active IM
was still `keyboard-us`. The cause is `InputMethodGroup`
(inputmethodgroup.cpp:100):

```cpp
if (d->inputMethodList_.size() > 1) {
    d->defaultInputMethod_ = d->inputMethodList_[1].name();   // the SECOND item
}
```

The **second** listed item wins. The original profile had `hunmin` first, so
`keyboard-us` was selected. Reordering to keyboard-us-then-hunmin fixed it.
This is why two of the three cases started passing at the same moment.

### 4. The layout never reached the engine

The last failure was `dkssuds` producing `ㅎ|곈` instead of `안|녕` — wrong
text, not lost keys, which is what made it worth chasing rather than another
plumbing bug. `HunminAdapter::layoutFor()` calls `InputMethodGroup::layoutFor()`,
which reads the **per-item `Layout=` field in the profile**. But `start_fcitx`
wrote it to `data/fcitx5/conf/hunmin.conf` — an fcitx5 *addon* config file
that the addon never reads. fcitx5 does not pass a layout to addons, and the
addon's own `hunmin.conf` has no `Layout` key at all.

With the layout empty the engine composes with its default, `sin3-p2`.
Verified against tier A:

```
$ hunmin-sim --layout sin3-p2 --keys dkssuds --trace
{"in":{"ch":"k"},"commit":"ㅎ",...}
{"in":{"ch":"u"},"commit":"곈",...}
```

That is exactly the `ㅎ|곈` D3 reported. **So the two `sin3-p2` cases had been
passing for the wrong reason** — right answer, by luck of the default layout.
`Layout=` is now written into the profile item where `layoutFor()` reads it.

## The readiness gate was also wrong

The gate waited only for `@server=fcitx`, but the `xim` addon loads at t+0.02s
and `hunmin` at t+1.08s. Keys injected in that window are forwarded to the
server and dropped, and the case fails with an empty commit stream. The gate
now also requires `Loaded addon hunmin`.

Note what the gate may **not** use: `fcitx5-remote -n`. It reports the
*focused* context's input method, which is empty until the harness connects, so
gating on it deadlocks — measured returning state `0` and an empty name for
100 s while `fcitx5-diagnose` reported a live `focus:1` InputContext.

## Ruled out by measurement, not by argument

| Candidate | Result |
|---|---|
| The XIM filter cannot see XTEST keys | `xcb_im_filter_event` hit for injected keys; they arrive, they are just not XIM messages |
| `client->connect_id` unset / `byte_order == '?'` drop | unreachable — no message arrives to be dropped. This was the previous hypothesis |
| Locale negotiation | whole D3 re-run under `ko_KR.utf8`: identical, zero opcodes |
| Xephyr nesting | same result on bare Xvfb. Nesting is not a fix and not the cause |
| fcitx5 not owning the selection | `XIM_SERVERS` on root = exactly `['@server=fcitx']`, owner `0x400001` — correct and standard |
| dkit sync mode | already excluded earlier by symbol interposition |

## A measurement trap worth recording

`FCITX_LOG_RULE` **is not read by fcitx5 at all** — grepping the source for it
returns nothing. The log rule is the `--verbose <rule>` command-line flag
(instance.cpp:1305). Every Debug-level line this investigation tried to read
was silently filtered for that reason, which sent one round of debugging after
a phantom "the XIM addon is not logging" dead end. With
`--verbose "xim=5,xim_key=5,hunmin=5"` the server side became fully visible and
`XIM Key Event: 2 k ... detail: 45` confirmed keys were arriving correctly all
along.

Note also that `XIM Key Event` is category **`xim_key`** (`xim.cpp:54`), not
`xim` — enabling only `xim` misses the key trace while the rest of the addon
logs normally.

## Instrumenting dkit without rebuilding it

`tests/e2e/dkit-logshim.c` is the `LD_PRELOAD` shim used throughout. It hooks
`xcb_im_create` to install a `DebugLog` handler and interposes
`xcb_im_filter_event` to log events. Four non-obvious constraints, each hit and
measured, are documented in that file's header:

- `xcb_im_create` takes **12** arguments; a shim declaring two forwards two and
  the real function segfaults at `xcb_im_create+0xa9`.
- No `dlopen` — the loader lock is already held (`AddonManager::load ->
  dlopen(libxim)`).
- No `RTLD_DEFAULT`/`RTLD_NEXT` — dkit is loaded `RTLD_LOCAL`, so it is not in
  the global scope; and `dladdr` on the shim's own function returns the shim.
- The load base is `dlpi_addr`; `/proc/self/maps` gives the r-x segment,
  ~0x25000 too high, and no dynsym entry resolves.

## What remains open

Nothing in the D3 handshake. `XIM_CONNECT`, `XIM_OPEN`, `XIM_CREATE_IC`,
`XIM_SET_IC_FOCUS`, `XIM_FORWARD_EVENT` and `XIM_SYNC_REPLY` are all observed
in the server trace, `fcitx5-diagnose` reports a focused InputContext, and the
commit stream matches tier A for all three cases.

### Why the candidate window stays out of D3 — measured, not assumed

An F9 Hanja-window case *was* added here and then removed, because it could not
fail. Two mutations of `frontends/fcitx5/src/hunminadapter.cpp` — deleting
`keyEvent.filterAndAccept()` in the F9 branch, and adding `flush()` at the top
of `openCandidates()` — both left D3 green, while both turned tier D2 red:

| mutation | D3 | D2 (`engine-d2-ui`) |
|---|---|---|
| `filterAndAccept()` removed (F9 passed to the client) | pass ❌ | **fail** ✅ |
| `flush()` in `openCandidates()` (preedit committed early) | pass ❌ | **fail** ✅ |

The cause is what this harness can observe. Instrumenting its event loop to
log every raw key it receives shows the F9 keycode arriving at the client on
every run, consumed or not:

```
RAWKEY detail=33  RAWKEY detail=32  RAWKEY detail=75   <- F9 (XKeysymToKeycode)
RAWKEY detail=45  RAWKEY detail=41  RAWKEY detail=54  RAWKEY detail=50
```

Consumption happens in the fcitx5 addon, downstream of the X event the harness
has already received, and the harness does not decode keysyms — `xim-harness.c`
says so in its own header ("Nothing here decodes a keysym") and `on_commit()`
discards the `keysym`/`nKeySym` arguments it is handed. So "F9 was consumed"
is not expressible in the commit stream, and the preedit-survival half fails
differently: `flush()` empties the preedit, `openCandidates()` then returns
false, F9 goes unconsumed, and the following `kfcQ` recomposes `푸|각` from an
empty buffer — the same stream by coincidence.

That is the general shape: **any D3 case whose distinguishing property is
invisible to a client that only sees commits will pass regardless.** D3's
precondition is that the case is expressible in `hunmin-sim`, which is why
`cases.tsv` states expectations only from `--trace` and carries no override
column.

The F9 flow is verified in tier D2 instead (`testhunminui.cpp`), in-process,
where consumption is the return value of `sendKeyEvent`, the candidate list is
read off `InputPanel::candidateList()`, and selecting a candidate is a direct
`list->candidate(0).select(ic)`. Both mutations above are caught there. The
D2 assertions are the right tier for this; D3 would have restated them in a
place that cannot fail.

### What D3 structurally cannot observe: consumption

It is tempting to close the remaining gap by asserting on `nKeySym` — the
pass-through keysyms `on_commit()` is handed and currently discards. Measured,
that does not work either, and the reason is worth keeping because it closes
the question rather than deferring it.

`nKeySym` is **0 for every commit, always** — for a syllable the engine
composed, and equally for a key the engine declined:

```
# dkssud  (all consumed)
[probe] on_commit nKeySym=0 str='안' keysyms=[]
[probe] on_commit nKeySym=0 str='녕' keysyms=[]
# dkssud1 ('1' declined -> flushed and passed to the client)
[probe] on_commit nKeySym=0 str='안' keysyms=[]
[probe] on_commit nKeySym=0 str='녕' keysyms=[]
```

A declined key produces no `commit_string` callback carrying it at all; it
simply never becomes text. So there is no keysym array to assert on, and an
assertion written against it would pass for the same reason the F9 case did.

### Why `nKeySym` is 0: traced through fcitx5 and dkit to the wire

Worth chasing, because "always 0" could have been an fcitx5 defect or a harness
bug, and the two have different fixes. It is neither: it is the XIM protocol
split into two messages, and fcitx5 always sends the one that carries no keysym.

The client takes a different branch per flag, in dkit's own source
(`xcb-imdkit-1.0.9/src/clientprotocolhandler.c`, `_xcb_xim_handle_commit`):

```c
if (flag & XCB_XIM_LOOKUP_KEYSYM) {        /* COMMIT_BOTH */
    ... commit_string(..., &frame.keysym, 1, ...);   /* nKeySym = 1 */
} else {                                   /* COMMIT_CHARS */
    ... commit_string(..., NULL, 0, ...);            /* nKeySym = 0 */
}
```

and the server picks between them from the same bit (`imdkit.c:870`):

```c
void xcb_im_commit_string(..., uint32_t flag, const char *str, uint32_t length,
                          uint32_t keysym) {
    if (!(flag & XCB_XIM_LOOKUP_KEYSYM) && (flag & XCB_XIM_LOOKUP_CHARS)) {
        ... xcb_im_commit_chars_fr_t ...     /* no keysym field */
    } else {
        ... xcb_im_commit_both_fr_t ... frame.keysym = keysym;
    }
}
```

fcitx5's XIM module sends the chars-only message unconditionally
(`src/frontend/xim/xim.cpp:421`):

```c
xcb_im_commit_string(server_->im(), xic_, XCB_XIM_LOOKUP_CHARS, commit,
                     length, 0);
```

Measured on the wire, from the harness's own `on_commit()`:

```
flag=0x3  CHARS=1  KEYSYM=0  nKeySym=0
```

`0x3` is `SYNCHRONOUS|LOOKUP_CHARS` (`ximproto.h:94-96`: `SYNCHRONOUS = 1<<0`,
`CHARS = 1<<1`, `KEYSYM = 1<<2`) — the server-side `use_sync_mode` ORs in
`SYNCHRONOUS` inside `xcb_im_commit_string`, so the flag fcitx5 passes as
`LOOKUP_CHARS` arrives as `0x3`. `KEYSYM` is clear, so the client takes the
`NULL, 0` branch.

So `nKeySym` is 0 because **no XIM implementation sends a keysym for a composed
string**, and a declined key never becomes a committed string to carry one.
There is nothing to fix and nothing to assert: the field is structurally empty
on this path for any server, not just for ours. A harness that wanted it would
have to assert on `flag` (which does distinguish the two messages), but that
asserts fcitx5's choice, not our engine's behaviour.

One measurement note: the first probe printed `flag=0x3 CHARS=0 KEYSYM=0`,
because the bit values were guessed as `0x10`/`0x20` instead of read from
`ximproto.h`. The contradiction is what sent this to the header — guessing the
constants would have produced a confident wrong explanation of the same data.

The raw X event is no help either, and this is the sharper result. Logging
every `XCB_KEY_PRESS` the harness receives:

```
# dkssud1 — '1' is keycode 10 on the us layout
[rawkey] detail=40  detail=45  detail=39  detail=39  detail=30  detail=40  detail=10
# dkssuds — every key consumed
[rawkey] detail=40  detail=45  detail=39  detail=39  detail=30  detail=40  detail=39
```

All seven keycodes arrive in both runs, including the declined one. The X
server delivers `KeyPress` to the client window before fcitx5's addon has
looked at it, and the addon filters in the *fcitx5* layer, downstream of what
this harness can see. Consumption is not observable from an XIM client at all
— not in the commit stream, not in `nKeySym`, not in the raw event.

That is the complete answer, and it is a stronger one than "assert on
`nKeySym`": there is nothing left to assert on. Consumption is only visible to
the addon, which is why `sendKeyEvent`'s return value in tier D2 is the sole
place it is asserted, and why `tests/e2e/cases.tsv` can only carry keys the
engine takes.
