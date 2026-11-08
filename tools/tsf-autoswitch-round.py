"""Dev-host measurement round for the automatic switch (issue #216).

What this measures, and why it is not tools/tsf-devhost-round.py: that tool
answers "did the right TEXT land". Issue #216 asks how the text is DELIVERED
when `Autoswitch` is ON and English-mode keys go through the engine instead of
the host's passthrough -- the caret after every key, what one undo step
removes, and what a selection does while the engine is the one writing. Those
are per-key numbers, so the host has to report its state after every key
without the reading itself disturbing the composition (a Ctrl+A/Shift+End
readback is a key of passage: it flushes the word being measured).

The host is a Chromium textarea: a TSF-native host (docs/TSF-INTERIM-INPUT-
SPIKE.md §3-F measured it staying on the native delivery), whose own state --
value, selectionStart, selectionEnd -- is readable from inside the page on
every key. The page mirrors that state into the window title, and the driver
polls it after each key; no keystroke is spent on reading.

  python3 tools/tsf-autoswitch-round.py --switch on  --keys '{L}English ' --out build-tsf-round/on-english
  python3 tools/tsf-autoswitch-round.py --switch off --keys '{L}English ' --out build-tsf-round/off-english

--keys is the same mini-language as tsf-devhost-round.py: characters, plus
{L} for the Han/Eng key (see docs/SPEC-MODES.md §E5) -- and, on top of it,
{UNDO} and {SELALL} so one round can measure the keyboard's own edit keys:

  '{L}English {UNDO}{UNDO}{UNDO}'        the undo units, one row per step
  '{L}English {SELALL}x '                type over a selection

The TIP's policy comes from the environment the host is launched with
(HUNMIN_TSF_AUTOSWITCH, HUNMIN_TSF_COMMIT_UNIT) and from HUNMIN_DATA_DIR, so
the two rounds differ in exactly one variable. The TIP log in the run dir is
the delivery evidence: a native round has `composition start`/`composition
end` and NO `legacy detection` row -- with the interim delivery the switch is
deliberately off (docs/DESIGN-FRONTEND.md Part A §2.1) and the round would be
measuring nothing.
"""
import argparse
import datetime
import importlib.util
import json
import os
import subprocess
import sys
import time
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parents[1]

ROOT = str(_COMMON_ROOT)

_spec = importlib.util.spec_from_file_location("devhost", os.path.join(ROOT, "tools", "tsf-devhost-round.py"))
assert _spec and _spec.loader, "tools/tsf-devhost-round.py is the Win32 plumbing this round reuses"
devhost = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(devhost)

EDGE = devhost.EDGE

PAGE = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>TXTJ__TOKEN__{}ENDT</title>
<style>html,body{margin:0;padding:0}textarea{width:96vw;height:80vh;font-size:22px;font-family:Consolas,monospace}</style>
</head>
<body>
<textarea id="t" autofocus spellcheck="false"></textarea>
<script>
const t = document.getElementById('t');
const TOKEN = '__TOKEN__';
t.focus();
let keys = 0;
/* Every sample carries the whole state, so a poll that missed one key still
 * reads a consistent row: `k` counts keydowns, and the driver records the row
 * whose `k` advanced past the key it just sent. */
function sync() {
  const s = {k: keys, v: t.value, s: t.selectionStart, e: t.selectionEnd,
             f: document.activeElement === t};
  document.title = 'TXTJ' + TOKEN + JSON.stringify(s) + 'ENDT';
}
t.addEventListener('keydown', () => { keys++; sync(); }, true);
t.addEventListener('keyup', sync, true);
t.addEventListener('input', sync);
t.addEventListener('compositionupdate', sync);
t.addEventListener('compositionend', sync);
t.addEventListener('select', sync);
document.addEventListener('selectionchange', sync);
t.addEventListener('mousedown', () => { t.focus(); sync(); });
setInterval(sync, 30);
</script>
</body>
</html>
"""


def title_state(title, token):
    marker = "TXTJ" + token
    if not title or marker not in title or "ENDT" not in title:
        return None
    body = title.split(marker, 1)[1].split("ENDT", 1)[0]
    try:
        return json.loads(body)
    except ValueError:
        return None


def read_state(token, tries=12, pause=0.06):
    marker = "TXTJ" + token
    for _ in range(tries):
        _hwnd, title = devhost.find_window(
            lambda cls, t: cls.startswith("Chrome_WidgetWin") and marker in t, tries=1, pause=0)
        st = title_state(title, token)
        if st is not None:
            return st
        time.sleep(pause)
    return None


def wait_advanced(token, last_k, timeout=3.0):
    """The first state whose keydown counter advanced past `last_k`."""
    deadline = time.time() + timeout
    best = None
    while time.time() < deadline:
        st = read_state(token)
        if st is not None:
            best = st
            if st["k"] > last_k:
                return st
        time.sleep(0.05)
    return best


def close_stale_browsers():
    """Close only the browsers an earlier round of THIS tool started.

    Each round runs Edge on its own user-data-dir under build-tsf-round/, so
    the command line names the round: a round that failed before its finally
    block leaves a window behind, and the next round's title search would find
    THAT window (measured 2026-11-02 -- the second run typed into the first
    run's page and its log stayed empty). The user's own Edge is untouched.
    """
    ps = ("Get-CimInstance Win32_Process -Filter \"Name='msedge.exe'\" | "
          "Where-Object { $_.CommandLine -like '*build-tsf-round*' } | "
          "ForEach-Object { Stop-Process -Id $_.ProcessId -Force }")
    subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True)


def split_actions(keys):
    """Characters, plus the {token}s -- {L}, {L+xy} (devhost) and the two edit
    keys this round needs. Kept separate from devhost.split_tokens so adding a
    token here cannot change what the sandbox lane means."""
    out = []
    i = 0
    while i < len(keys):
        if keys[i] == "{":
            j = keys.find("}", i)
            if j < 0:
                raise SystemExit("unterminated token in --keys")
            out.append(("token", keys[i + 1:j]))
            i = j + 1
            continue
        out.append(("char", keys[i]))
        i += 1
    return out


def run_token(token):
    if token in ("UNDO", "REDO", "SELALL"):
        vk = {"UNDO": 0x5A, "REDO": 0x59, "SELALL": 0x41}[token]
        devhost.chord(0x11, vk)
        return
    devhost.run_token(token)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--switch", choices=("on", "off"), required=True)
    ap.add_argument("--keys", required=True)
    ap.add_argument("--layout", default="sin3-p2-basic")
    ap.add_argument("--settle", type=float, default=3.0)
    ap.add_argument("--between", type=float, default=0.45,
                    help="seconds to let one key settle before the state is polled")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = args.out or os.path.join(ROOT, "build-tsf-round", "%s-%s" % (args.switch, stamp))
    run_dir = os.path.abspath(run_dir)
    os.makedirs(run_dir, exist_ok=True)
    log_path = os.path.join(run_dir, "hunmin-tsf-debug.log")
    if os.path.exists(log_path):
        os.remove(log_path)

    token = "%x" % (int(time.time() * 1000) & 0xFFFFFFF)
    page = os.path.join(run_dir, "target.html")
    with open(page, "w", encoding="utf-8") as f:
        f.write(PAGE.replace("__TOKEN__", token))

    env = dict(os.environ,
               HUNMIN_TSF_DEBUG_LOG="1",
               HUNMIN_TSF_LOG_DIR=run_dir,
               HUNMIN_TSF_AUTOSWITCH="1" if args.switch == "on" else "0",
               HUNMIN_DATA_DIR=os.path.join(ROOT, "libhunmin", "data"))
    profile = os.path.join(run_dir, "edge-profile")
    result = {"switch": args.switch, "keys": args.keys, "layout": args.layout, "run_dir": run_dir}

    close_stale_browsers()
    previous = devhost.set_store_layout(args.layout)
    rows = []
    browser = None
    try:
        browser = subprocess.Popen([EDGE, "--new-window", "file:///" + page.replace("\\", "/"),
                          "--no-first-run", "--no-default-browser-check",
                          "--user-data-dir=" + profile],
                         env=env, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
        marker = "TXTJ" + token
        hwnd, title = devhost.find_window(
            lambda cls, t: cls.startswith("Chrome_WidgetWin") and marker in t, tries=180)
        if not hwnd:
            raise RuntimeError("no Edge window running the probe page")
        devhost.focus(hwnd)
        cycles = devhost.activate(hwnd, log_path, "applied hangul=%s" % args.layout)
        if cycles is None:
            applied = [l for l in open(log_path, encoding="utf-8", errors="replace").read().splitlines()
                       if "applied hangul=" in l] if os.path.exists(log_path) else []
            raise RuntimeError("profile not activated; applied rows: %r" % applied)
        result["activation_cycles"] = cycles
        if not devhost.focus(hwnd):
            raise RuntimeError("host window lost the foreground")
        time.sleep(args.settle)

        st = read_state(token)
        rows.append({"step": 0, "action": "START", "state": st})
        last_k = st["k"] if st else 0
        for kind, tok in split_actions(args.keys):
            if devhost.user32.GetForegroundWindow() != hwnd and not devhost.focus(hwnd):
                result["focus_lost_before"] = tok
                break
            if kind == "token":
                run_token(tok)
                label = "{%s}" % tok
            else:
                devhost.type_char(tok)
                label = tok
            time.sleep(args.between)
            st = wait_advanced(token, last_k)
            if st is not None:
                last_k = max(last_k, st["k"])
            rows.append({"step": len(rows), "action": label, "state": st})
        result["rows"] = rows
        result["text"] = rows[-1]["state"]["v"] if rows and rows[-1]["state"] else None
        rows_after_end = read_state(token)
        result["text_settled"] = rows_after_end["v"] if rows_after_end else None
    finally:
        if browser is not None:
            subprocess.run(["taskkill", "/PID", str(browser.pid), "/T", "/F"], capture_output=True)
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, devhost.STORE, 0, winreg.KEY_READ | winreg.KEY_WRITE) as k:
            winreg.SetValueEx(k, devhost.VALUE, 0, winreg.REG_SZ, previous)
        if os.path.exists(log_path):
            lines = open(log_path, encoding="utf-8", errors="replace").read().splitlines()
            result["delivery"] = [l for l in lines if any(t in l for t in (
                "applied hangul", "legacy", "composition start", "composition end",
                "interim:", "flush:", "boundary", "consumed,", "judge", "judgement"))]
            result["legacy_rows"] = sum(1 for l in lines if "legacy detection" in l)
            result["composition_starts"] = sum(1 for l in lines if "composition start" in l)
            result["composition_ends"] = sum(1 for l in lines if "composition end" in l)
        with open(os.path.join(run_dir, "round.json"), "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=2)

    for r in rows:
        st = r["state"] or {}
        print("step=%-3d %-10s len=%-3s sel=%-8s value=%r" % (
            r["step"], r["action"],
            len(st.get("v", "")) if st else "?",
            "%s,%s" % (st.get("s"), st.get("e")) if st else "?",
            st.get("v") if st else None))
    print(json.dumps({k: result.get(k) for k in (
        "switch", "keys", "text", "text_settled", "legacy_rows",
        "composition_starts", "composition_ends")}, ensure_ascii=False))
    print("run:", run_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())