# hunmin — notes for fcitx5 addon authors

Hard-won knowledge from building our `hunmin` addon and getting tier-D3
(stock fcitx5 + Xvfb e2e) to run. Verified against the fcitx5 5.1.19
source tree in `fcitx5-5.1.19/` (a Debian source dump, gitignored) and by
observation on a running instance.

This file is for whoever writes the next addon for this repo, or ports one.
It records the things that are not in fcitx5's own docs and that cost us
real time. Line references are 5.1.19 unless noted.

---

## 1. Two kinds of "name" — do not conflate them

| Name | Meaning | Where it comes from |
|---|---|---|
| **addon name** | the shared library / addon dir | filename stem of `addon/<name>.conf`, `Library=` field |
| **unique name** | an input *method* | the `.conf` file **stem** under `inputmethod/` |
| **display name** | human label | `Name=` inside `inputmethod/<unique>.conf` |

`hunmin` happens to be all three, which hides the distinction while you
develop. `fcitx5-hangul` is an addon named `hangul`; its IM is
`fcitx-hangul`. `fcitx5-remote -m <imname>` takes the **unique name** and
returns the **addon** name. `Instance::preloadInputMethod` looks an entry up
by `entry->addon()`.

Our `inputmethod/hunmin.conf` must contain `Addon=hunmin` — that field is
the bridge, and it is the only thing that makes a stray `.conf` file
discoverable at all.

## 2. StandardPaths: the suffix rules are asymmetric

From `src/lib/fcitx-utils/standardpath.cpp`. This bit us hard, so it is
worth memorising:

| Env var | Type | Suffix behaviour |
|---|---|---|
| `FCITX_CONFIG_HOME` | `PkgConfig` | used **verbatim**; `profile` lives directly here |
| `FCITX_ADDON_DIRS` | library paths | used **verbatim** (`.so` dirs) |
| `FCITX_DATA_DIRS` | `PkgData` | **appends `/fcitx5` to every entry** |
| `FCITX_DATA_HOME` | `PkgData` | **appends `/fcitx5`** |
| `XDG_CONFIG_HOME` | `Config` | used verbatim; addon per-addon config is `<XDG_CONFIG_HOME>/fcitx5/conf/` |
| `XDG_DATA_HOME` | `Data` | used verbatim |

So `FCITX_DATA_DIRS=/usr/share` gives `/usr/share/fcitx5`. Passing
`/usr/share/fcitx5` silently yields `/usr/share/fcitx5/fcitx5` and your
`inputmethod/*.conf` is never found. `FCITX_CONFIG_HOME` is *not*
suffixed — it is the one that looks like it should be but isn't.

When `FCITX_CONFIG_HOME` is unset, `PkgConfig` falls back to
`$XDG_CONFIG_HOME/fcitx5` (`standardpath.cpp:112-115`).

There is an escape hatch: `SKIP_FCITX_PATH`, `SKIP_FCITX_SYSTEM_PATH`,
`SKIP_FCITX_USER_PATH`.

## 3. A context is not active because you said so

The single most important thing for headless/e2e work. To get keys routed
to your engine you need **all three**:

1. the IM present in the active **group** (`profile`),
2. `ActiveByDefault=True` in `[Behavior]`,
3. `ResetStateWhenFocusIn` set to a policy that is **not** the default.

`Instance`'s `InputContextFocusIn` watcher (`instance.cpp:1060`) only calls
`activate()`/`deactivate()` *inside* that policy check, and
`resetStateWhenFocusIn` defaults to `PropertyPropagatePolicy::No`
(`globalconfig.cpp:204`). With the default, **nothing happens no matter
what `ActiveByDefault` says** and the client keeps its own keymap. This
looks exactly like a broken addon and is not.

`ActiveByDefault` is a real no-op on its own. We shipped it for a while
believing it was sufficient; it was not.

## 4. `FCITX_LOG_RULE` tags are per source file, not per concept

`FCITX_LOG_RULE=default=5,xim=5` gives you very little. Tags are the
**source file basename**: `instance`, `inputmethodmanager`, `globalconfig`,
`xim_key` (note: not `xim`), `keyboard`, `core_instance`, `addonmanager`,
`focusgroup`. Turning on `instance` + `inputmethodmanager` +
`globalconfig` is what actually explains a misconfigured group.

The useful lines: `Group Item X in group Y is not valid. Removed.`,
`Group Y is empty. Removed.`, `No valid input method group in
configuration. Building a default one`, and `Loaded addon <name>`.

Note the default group is built with `keyboard-us` and an **empty**
`DefaultInputMethod`. If you see that message, your `profile` is wrong —
almost always the path or a missing `inputmethod/*.conf`.

## 5. `fcitx5-remote` argument traps

```
-o            activate the application            (takes NO argument)
-c            inactivate
-s <imname>   switch to this input method         (this is the one you want)
-n            print current IM unique name        (empty == no active IM)
<none>        print state: 0=closed 1=inactive 2=active
-m <imname>   IM unique name -> addon name
```

`fcitx5-remote -o hunmin` does **not** select `hunmin`; it activates the
application and drops the argument. The symptom is a silent no-op followed
by every key passing through raw. Use `-s` then `-o`.

Also: `-n` empty is the fastest single check for "the context never became
active". Prefer it over reading logs.

## 6. On-demand addons load late

Our addon conf sets `OnDemand=True`. `Instance::initialize` loads addons,
then `InputMethodManager::load`, then schedules a **1-second** timer that
preloads the group's first and default IM (`instance.cpp:~1400`). So
`Loaded addon hunmin` appearing ~1s after startup is normal, not a bug.
If you wait for it, wait up to ~2s.

## 7. Frontend modules do not pick the IM

`xim.conf` has **no** `DefaultInputMethod`; the XIM module never reads one
(0 hits for the identifier in `src/frontend/xim/xim.cpp`). The XIM frontend
only calls `ic->focusIn()` and forwards keys. IM selection is entirely the
profile's `DefaultIM` plus the focus-in policy from §3. A `xim.conf` with
that key is cargo cult; we had one and it did nothing.

## 8. Running fcitx5 under Xvfb (e2e)

Two traps, both fatal and both easy to misdiagnose.

**Detach from the host desktop session.** On a Wayland workstation
fcitx5 prefers `$WAYLAND_DISPLAY` over `$DISPLAY` and quietly attaches to
the real compositor. `DISPLAY=:99` is ignored, XIM never registers, keys
reach the client raw. Unset `WAYLAND_DISPLAY`, `XDG_SESSION_TYPE`,
`XDG_CURRENT_DESKTOP`, `XDG_SESSION_DESKTOP`, `DESKTOP_SESSION`. You can
confirm which display it picked: `fcitx5.log` says
`Connecting to X11 display, display name::N`.

**Shadow the portal services on your private bus.** fcitx5 core queries
`org.freedesktop.portal.Desktop` at startup. On a `dbus-run-session` bus,
activating it spawns `xdg-desktop-portal` and `ksecretd`, and dbus-daemon
hands them the activating process's **stdout/stderr**. Those daemons are
long-lived, so they hold your harness's output pipe open after the script
exits. Under ctest this is a hang that never even applies the `TIMEOUT`
property — the parent is gone, the pipe is not. Fix: give the bus a
datadir that shadows those `.service` files with `Exec=/bin/true`, and
disable the GUI addons (`classicui` runs its own `PortalSettingMonitor`,
`kimpanel`, `virtualkeyboard`, `notifications`, `notificationitem`) via
`[Addon] DisabledAddons=` in the global config.

Diagnose it by walking `/proc/*/fd` and finding who still holds the log.
That is faster and more reliable than reading fcitx5 logs.

## 8b. Debugging an e2e harness: what actually cost us time

Each of these produced a wrong conclusion before the real cause was found.
They are listed because the failure mode is silence, not an error.

**Redirect to a file, never to a pipe.** `run-d3.sh … | grep …` and
`| cat` make the harness hang: the pipe reader is the thing that dies or
blocks, and `dbus-run-session` + its children inherit it. `> /tmp/log 2>&1`
returns in ~6 s where the piped form never returns. If you must watch
progress, tail the file.

**Check for leftovers before believing any result.** A debugging session
that exits badly leaves `Xvfb :95` and `fcitx5 --verbose` running, and
`/tmp/.X95-lock` then makes the harness skip that display number and pick
another — so one run's failure looks like a completely different bug on the
next run. Same test, same code, two symptoms. Before investigating:
`pgrep -a -f 'Xvfb|fcitx5|xim-harness'` and `ls /tmp/.X*-lock`.

**`kill` on a shell that `exec`s leaves orphans.** `run-d3.sh` re-execs
itself through `dbus-run-session`; killing the pid you started does not
reach the grandchildren. They hold fds, and the next run inherits a
polluted display table. `pkill -f run-d3.sh` is the reliable cleanup.

**A nested `bash -c` does not inherit what you think.** `DISP=":99"` in
the outer shell does not become `DISPLAY` in an inner `bash -c` — you must
pass it explicitly and `export DISPLAY="$DISP"` inside. We burned a long
stretch chasing "fcitx5 attaches to :0" that was really our own probe
script losing a variable across an `exec dbus-run-session` boundary.

**ctest's `TIMEOUT` is not a safety net.** If the child holds the output
pipe, ctest blocks before the timeout machinery can apply, and you get a
silent hang instead of a failure. Always check "did it terminate" yourself.

**`fcitx5-remote -n` before reading any log.** Empty means no active IM and
everything downstream is meaningless. Cheap, and it eliminates half the
hypotheses in one command.

**Read the source, don't infer the API.** Three of our wrong conclusions
(`-o` vs `-s`, a nonexistent `xim.conf` key, `FCITX_DATA_DIRS` semantics)
came from assuming a plausible API instead of grepping 5.1.19. Each grep
took a minute; each wrong guess cost ten.

## 8c. What the official wiki says (and where it is imprecise)

From <https://fcitx-im.org/wiki/Fcitx_5> → For Developers, read 2026-10-02.
Three pages matter: *Develop an simple input method*, *Q&A for developer*,
*Debug fcitx5*. Our own findings in §1–§8 came from the 5.1.19 source; these
are the upstream guidance, kept separate so we do not mistake it for source
truth. Where the two disagree, the source wins — noted inline.

### Confirmed by the wiki, matches our code

- **On-disk layout**: `share/fcitx5/addon/<addon>.conf`,
  `share/fcitx5/inputmethod/<im>.conf`, `lib/fcitx5/<library>.so`. Note
  `<library>` need not equal `<addon>`.
- **`Library=`** in the addon conf names the shared object. fcitx dlopens it
  from the addon directory — which is why D3 copies the `.so` next to the
  conf.
- **An IM conf needs `Addon=<addon name>`** to bind to its addon. Same fact as
  §1, stated as a requirement here.
- **`InputContextProperty` is the supported per-context state mechanism**,
  registered via `InputContextManager::registerProperty` with a globally unique
  name, obtained with `FactoryFor<>`. The wiki's advice — use a human-readable
  name if another addon might need the state — is why ours is `"hunminState"`
  rather than a UUID.
- **Use one preedit, not two.** `client preedit` (in the app) vs the input
  panel's own; check `capabilityFlags` to see which is supported. That is
  exactly our `setClientPreedit` / `setPreedit` branch.
- **Call `filterAndAccept()` for keys you handle; let key releases through.**
  Also pass modifier chords to the app so app hotkeys keep working mid
  composition.
- **Config hooks**: `getConfig`/`setConfig`/`getConfigForInputMethod`/
  `setConfigForInputMethod`/`reloadConfig`, with `readAsIni` /
  `safeSaveAsIni` from/to `$XDG_CONFIG_HOME`. It also says to call
  `reloadConfig()` **in the constructor** — we do.
- **Reusing another addon** goes through `FCITX_ADDON_DEPENDENCY_LOADER` plus
  a `find_package` line, so the addon loads on first use without a link-time
  dependency. Relevant to P3 hanja if it wants quickphrase or clipboard.
- **Testing**: "Fcitx provides a small testing framework, that allows you to
  simulate key event... refer to fcitx-utils/testing.h... see also
  testquickphrase.cpp". That is precisely the `testfrontend` +
  `setupTestingEnvironmentPath` pattern our D1/D2 use.

### The wiki is imprecise here — do not rely on it

- **"Key property is in a normalized form... which removes Shift modifier for
  certain cases... upper case A produced by Shift+A and Capslocked A will be
  the same after normalization."** True as far as it goes, but it hides the
  trap. `Key::normalize()` (`key.cpp:493`) rebuilds `states_` down to
  `Ctrl_Alt_Shift | Super | Mod3` — which **drops CapsLock entirely**. So
  `keyEvent.key().states()` can never tell you CapsLock is on. We read
  `rawKey()` for it. The wiki never says this, and our own comment
  ("fcitx delivers the shifted letter") is only true of `rawKey()`, not of
  `key()`. Read the normalization source before trusting any key-state claim.
- **`PREFIX ""`** in the tutorial's CMake is how `quwei.so` avoids becoming
  `libquwei.so`. Our `set_target_properties` sets `OUTPUT_NAME hunmin` and
  relies on `add_fcitx5_addon` instead; the effect is the same and D3 copies
  the file under the name the conf's `Library=` expects.

### Q&A answers that should shape our roadmap

- **Own UI? Officially NO.** Under Wayland only the IM server may create
  popup surfaces, so a third-party UI cannot be made to work reliably, and a
  replacement kimpanel cannot hand the D-Bus name back. The advised path for a
  bespoke UI is: leave `inputPanel()` empty and talk over your own D-Bus name.
  → we should not plan a custom UI addon.
- **Global options**: "some global config option such as previous page key,
  next page key, it may be used" — but a default good for every engine does not
  exist, which is why our own `Layout` option lives in our conf.
- **Surrounding text: use for information, do not modify.** The wiki is
  emphatic — many applications simply do not work with it (XIM clients
  xterm/wine, SDL, terminals; buggy in non-system widgets, browsers, embedded
  terminals), and relying on it is worse than working without it. Japanese uses
  it for reconvert; Hangul for temporary Hanja mode only.
  → **This constrains P3 hanja.** Our F9 flow must not depend on reading or
  rewriting surrounding text. Worth recording as a design constraint now,
  before P3 starts.
- **Multi-line text**: allowed in candidate lists, **not** in preedit or aux.
  Relevant when hanja candidates get long.

### Debug fcitx5 (upstream)

From the same wiki. Two techniques we should be using and are not:

- **Nested X server so focus-out never fires.**
  `Xephyr :1 & DISPLAY=:1 openbox & DISPLAY=:1 xterm & DISPLAY=:1 fcitx5 -r`.
  Switching windows makes the IM reset on focus-out, which destroys any bug
  that needs a key *sequence* to reproduce. A nested server suppresses that.
  **This is a candidate cause for our D3 red**: the harness focuses and
  unfocuses around key injection, so a composition spanning two keys may be
  reset between them. Worth trying before digging further into XIM.
- **Disabling the frontend that talks to your terminal**, so gdb and typing
  coexist: `gdb --args fcitx5 --disable=dbusfrontend -r`, or start the
  terminal with `XMODIFIERS=@im=none` so it cannot reconnect to fcitx5's XIM.
- `setupTestingEnvironment` is the documented entry point for the test
  addons, with `testunicode` as the reference — we use `testfrontend` and
  D1/D2 mirror it, but `testunicode` is the example the wiki points at.
- Log categories exist so you can enable *only* the log you want; that is the
  mechanism behind §4's per-source-file tag rule.

Not yet verified against our setup: Xephyr is not in our ENVIRONMENT.md
inventory, and D3 currently uses Xvfb without a window manager, so the
focus-out path may or may not be implicated. Stated as a lead, not a finding.

## 9. Key injection: XTEST, not uinput

Xvfb has no evdev backend, and `/dev/uinput` is root-only in CI. So on a
headless X stack, inject at the X server with **XTEST** (libXtst). The
whole stack below X is still real: XKB → fcitx5 XIM → addon → engine →
client buffer.

Two details worth keeping:

- XTEST does **not** infer Shift. Uppercase needs an explicit Shift_L
  press/release pair, or you get lowercase.
- The engine must consume every key in a test case. If any key falls
  through to the client, the COMMIT stream stops being comparable to the
  expected stream and the test lies.

## 10. Two ways to be sure, before believing a log

- `fcitx5-remote -n` — is there an active IM at all?
- `fcitx5-remote` (no args) — `0` means closed/inactive; if this stays `0`
  nothing else you read will matter.

## 11. Status of our D3

`engine-d3-x11` currently **fails**, deliberately visible rather than
skipped. The commit stream comes back as raw keys —
`FAIL [dubeolsik/dkssuds] expected: 안|녕 actual: d|k|s|s|u|d|s` — which says
the adapter never received them, upstream of libhunmin.

### 11a. The addon was not being loaded at all (fixed)

`run-d3.sh` staged the addon where fcitx5 does not look.
`StandardPathsPrivate::defaultPaths()` (fcitx-utils/standardpath.cpp:259)
splits `FCITX_DATA_DIRS` on `:` and uses each entry **verbatim**; `packageName`
is appended only to the *fallback* value, never to an environment one. So an
entry must already be the `fcitx5/` directory itself:

| staged | fcitx5 searched | loaded? |
|---|---|---|
| `$STAGE/pkgdata/fcitx5/addon` | `$STAGE/pkgdata/addon` | no |
| `$STAGE/pkgdata/addon` | `$STAGE/pkgdata/addon` | yes |

Every stock addon loaded and hunmin alone did not, which is what made this
look like a filter problem. Verified by keeping the stage: `Loaded addon
hunmin` goes from absent to present, 18 → 19 addons, and the staged profile
gains `Items/0 = hunmin`.

The same note had it backwards — it claimed fcitx5 *appends* `/fcitx5` to
`FCITX_DATA_DIRS` entries, which is how the wrong path got written in the
first place.

### 11b. ~~The XIM server never receives the client's CREATE_IC~~ — SOLVED

⚠️ **Superseded by `docs/D3-ROOT-CAUSE.md`. Read that instead.**

The heading above was wrong in its premise. It assumed the client was talking
XIM and that dkit was dropping `CREATE_IC`. Neither was true: the D3 harness
never spoke XIM to fcitx5 at all.

The one-line cause: `xim-harness.c` called `XOpenIM()`/`XCreateIC()` while
linking only `X11::X11`, so the client used **libX11's built-in XIM** while the
server is **libxcb-imdkit**. libX11's XIM answers with its own *local* IM when
no server is reachable (`_XimLocalOpenIM` and friends), so `XOpenIM` returned
non-NULL and `XCreateIC` succeeded even on a bare Xvfb with no fcitx5 running,
and the harness decoded every key itself. No XIM traffic was generated, so no
`XIM_CONNECT` and no `CREATE_IC` — hence `0 InputContext(s)`.

The harness is now written against dkit's `xcb_xim_*` client API and linked
against `libxcb-imdkit`. `engine-d3-x11` passes all 3 cases.

What this file got right and should keep:

- fcitx5's XIM addon links **libxcb-imdkit**, not D-Bus (`ldd`, `readelf -d`).
- The server registers correctly: `XIM_SERVERS(ATOM) = @server=fcitx` on the
  root window, selection owner `0x400001`. Standard and correct.
- **Keys do reach `xcb_im_filter_event`.** That measurement was accurate; only
  the interpretation was wrong. Injected keys are not XIM messages, and the
  filter was never the problem.
- `XIM Key Event x 0` is a *symptom of zero contexts*, not a filter failure.

### 11c. Wrong turns in this diagnosis, kept so they are not repeated

| Claim | Refuted by |
|---|---|
| "the harness has no focus, so Xephyr is the lead" | standalone probe: `focused=... ours=... match=1`, key delivered. `xwininfo` prints the focused window in its `Focus` section; I read a tree annotation. Xephyr was installed on the strength of this and changed nothing |
| "the readiness gate can never pass" | `XIM_SERVERS(ATOM) = @server=fcitx` is set on the root window |
| "the XIM filter does not see XTEST events" | `xcb_im_filter_event` hit 4 times for 2 keys |
| "locale: `LC_CTYPE=C.UTF-8` is absent from diagnose's *All locales*" | true, but re-running D3 under `ko_KR.utf8` changes nothing |
| breaking on `xcb_xim_create_ic` / `xcb_xim_*` | fcitx5 calls `xcb_im_*` and dkit exports `xcb_im_*`; `xcb_im_create_ic` is not exported at all, so the breakpoint stayed pending and silently never hit |

### 11c-bis. Four more, from the fix that made D3 honest

Rewriting the harness surfaced these immediately. All four cost a round each.

| Claim or approach | What actually happens |
|---|---|
| `fcitx5-remote -s hunmin` selects the engine | returns **rc=0** and the active IM stays `keyboard-us`. `Instance::setCurrentInputMethod` returns silently when nothing is focused. Retrying 10x changes nothing — it is removed |
| `DefaultIM=hunmin` in the profile selects the engine | the **second** listed item wins: `InputMethodGroup` sets `defaultInputMethod_ = inputMethodList_[1].name()` (inputmethodgroup.cpp:100). Item order is load-bearing |
| writing `Layout=` to `data/fcitx5/conf/hunmin.conf` configures the layout | the addon reads the **profile's** per-item `Layout=` (`HunminEngine::layoutFor` → `InputMethodGroup::layoutFor`). fcitx5 never hands an addon a layout; the addon's own `hunmin.conf` has no such key |
| `FCITX_LOG_RULE=<rule>` enables debug logging | **fcitx5 never reads that variable** — grep the source, zero hits. The rule is `--verbose <rule>` (instance.cpp:1305). Every Debug line looked absent for this reason |

And one that wastes a whole debugging round on its own: `XIM Key Event` is
category **`xim_key`** (`xim.cpp:54`), not `xim`. Enabling only `xim=5` logs the
addon normally and silently misses the key trace.

A subtler one, worth stating as a rule: two D3 cases passed **for the wrong
reason**. With the layout empty the engine composes with its default
`sin3-p2`, which happened to be what those two cases expect, so they were green
while `dkssuds` produced `ㅎ|곈`. A pass is not evidence that the mechanism
under it is the one you intended — check that the fixture can fail.

### 11c-ter. One addon, two input methods — and why the layout picker stays global

`hunmin-romaja.conf` registers a **second** input method (`Hunmin (Romaja)`)
against the same `hunmin` addon. This is the normal fcitx5 shape, not a
workaround: an input method is a `.conf` under `share/fcitx5/inputmethod/`
naming an addon, so two registrations for one addon are how one engine offers
two configurations.

The subtlety is where the *layout* is selected, and it is the §11c-bis trap
again. The per-item `Layout=` in the profile is what `layoutFor()` reads, so
each IM item selects its layout — but the addon's configtool exposes **one**
global `Layout=` dropdown, injected with every layout the core offers. There is
no per-IM picker, and adding one would be wrong: two IMs sharing one
`HunminConfig` instance cannot each hold a different value for the same key.
The profile item is the only per-IM override, and it is the only place that
should carry one.

The list itself must come from the core. `HunminEngine::refreshLayoutList()`
walks `hunmin_ctx_layout_count/id/name()` and hands the rows to
`LayoutAnnotation::dumpDescription`, which writes the `Enum/n` + `EnumI18n/n`
rows that `fcitx5-configtool` turns into a QComboBox. Hardcoding the ids in
`engine.h` would work and would also be a second place to edit per layout,
violating AGENTS.md rule 1 — enforced now as invariant I11, with D2 asserting
the rows behaviourally against the shipped data.

**Ordering caveat, and what it costs in the field.** `refreshLayoutList()`
must run *before* the first `getConfig()`, because that is when
`dumpDescription()` runs. It is called in the constructor after `ctx_` exists,
and again in `reloadConfig()` — the data directory can change under a reload,
and a stale annotation would offer layouts the engine can no longer open. D2
cannot cover the reload path: it is an addon virtual that only fcitx5 itself
calls, over D-Bus, and a testfrontend instance never does that.

Which means the picker is populated **once per fcitx5 process**, and installing
an updated package does not refresh it. Measured 2026-10-03: after installing
the deb that adds `hunmin-romaja.conf` and the layout rows, configtool still
listed only `Hunmin` and showed the layout as free text `sin3-p2` with no
rows — because fcitx5 was already running and had read the old
`.conf` files and the old annotation at startup. `systemctl --user restart
fcitx5` cleared both symptoms at once.

So this is the expected behaviour of an already-running instance, not a
packaging defect — but it is indistinguishable from one to a user, and it
will recur on every `dpkg -i`. Anything that changes the set of layouts or
input methods needs a restart to be visible; that belongs in the deb's
postinst, and it is the one gap in this feature with a real cost.

### 11d. Use fcitx5-diagnose before reading fcitx5 source

The D-Bus context dump answers "is there an input context at all" in one
command. Hours went into reading `xim.cpp`, `xcbmodule.cpp` and
`standardpath.cpp` to reach the same fact. The upstream page
<https://fcitx-im.org/wiki/Fcitx5-diagnose> documents the dump's shape
(`Group [...] has N InputContext(s)`) and the sections that matter for XIM:
XMODIFIERS, `XIM_SERVERS` on the root window, and locale.

Two things it settled that source-reading had not:

- `Group [x11::96] has 0 InputContext(s)` — the whole "is the XIM connected"
  question, in one line, before any source was read.
- `IC [...] program: frontend:xim cap:4000000000 focus:1` — that the IC **does**
  have focus once the harness speaks dkit, which is what distinguishes "no
  context" from "context present but the active IM is keyboard-us".

Caveats when using it here: it reads `/usr/share/fcitx5/addon`, so it will
never list our staged `hunmin` addon — that is expected, not a finding. It
hangs without a D-Bus session (`dbus-run-session -- fcitx5-diagnose`), and it
needs the same `FCITX_*` environment as the instance under test or the dump
describes a different process.

Do **not** gate a test on `fcitx5-remote -n`: it reports the *focused* context's
input method, which is empty until a client connects. Measured here returning
state `0` for 100 s while diagnose showed `focus:1`.
