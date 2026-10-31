r"""Dev-host round driver: type a key sequence into Notepad or Edge through the
registered TIP and read the document text back (issue #126).

Both hosts here are the modern TSF class (the native composition path). The
sandbox lane for the same host class is tools/tsf-edge-round.ps1; this script is
the dev-host half, where the installed image is the one the user runs.

  python3 tools/tsf-devhost-round.py --host notepad --layout dubeolsik --keys 'dkssud ' --expect '안녕 '
  python3 tools/tsf-devhost-round.py --host edge    --layout dubeolsik --keys 'dkssud ' --expect '안녕 '

What it does, per host:
  * Notepad: launches it with HUNMIN_TSF_DEBUG_LOG=1 and the log dir pointed at
    the run dir, clears the restored document, types one key at a time with a
    foreground guard, and reads the text back with Ctrl+A / Ctrl+C.
  * Edge: launches a generated page whose textarea value is mirrored into
    document.title (TXTV[<encoded>]ENDT), types the same way, and reads the
    title -- no UIA or CDP client, and the readback is what the page received.
  * Win+Space cycles the input list until the TIP logs `applied hangul=`.

The store layout is set to --layout for the round and restored afterwards (the
TIP reads HKCU\Software\Hunmin\TSF only in Activate, so a running host keeps its
old layout until the profile is re-activated -- which the Win+Space cycle does).

Reading the run: the TIP log in the run dir is the delivery trace. A native
round shows one `composition start`, one `composition end text=` at the boundary
key, and no `interim:`/`flush:` rows; `legacy detection:` means the host
terminated the composition mid-word and the interim path took over.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import json
import os
import subprocess
import sys
import time
import winreg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
STORE = r"Software\Hunmin\TSF"
VALUE = "HangulLayout"

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]


class _INPUTunion(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("padding", ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wt.DWORD), ("u", _INPUTunion)]


def send_vk(vk, up=False):
    inp = INPUT(type=1)
    inp.u.ki.wVk = vk
    inp.u.ki.dwFlags = 0x0002 if up else 0
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))
    time.sleep(0.02)


def tap(vk):
    send_vk(vk)
    send_vk(vk, up=True)


def chord(vk_mod, vk):
    send_vk(vk_mod)
    send_vk(vk)
    send_vk(vk, up=True)
    send_vk(vk_mod, up=True)


def type_char(ch):
    scan = user32.VkKeyScanW(ctypes.c_wchar(ch))
    vk, shift = scan & 0xFF, (scan >> 8) & 1
    if vk == 0xFFFF or vk == 0:
        raise RuntimeError("no virtual key for %r" % ch)
    if shift:
        send_vk(0x10)
    send_vk(vk)
    send_vk(vk, up=True)
    if shift:
        send_vk(0x10, up=True)


LANG_VK = 0x15  # VK_HANGEUL: the physical Han/Eng key on Korean hardware


def split_tokens(keys):
    """--keys is characters plus {token}s.

    Tokens exist because the Han/Eng key is not typed, it is PRESSED AND HELD
    (E5, issue #159): `{L}` is a single press (the toggle), `{L+xy}` holds it
    while typing xy (the compound press). Everything else stays one character
    per key so the ordinary golden rounds read as before.
    """
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
    if token == "L":
        tap(LANG_VK)
        return
    if token.startswith("L+"):
        send_vk(LANG_VK)
        try:
            for ch in token[2:]:
                type_char(ch)
        finally:
            send_vk(LANG_VK, up=True)
        return
    raise SystemExit("unknown token {%s}" % token)


def enumerate_windows():
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        cls = ctypes.create_unicode_buffer(256)
        title = ctypes.create_unicode_buffer(1024)
        user32.GetClassNameW(hwnd, cls, 256)
        user32.GetWindowTextW(hwnd, title, 1024)
        out.append((hwnd, cls.value, title.value))
        return True

    user32.EnumWindows(cb, 0)
    return out


def clipboard_text():
    CF_UNICODETEXT = 13
    user32.GetClipboardData.restype = ctypes.c_void_p
    user32.GetClipboardData.argtypes = [wt.UINT]
    kernel32.GlobalLock.restype = ctypes.c_void_p
    kernel32.GlobalLock.argtypes = [ctypes.c_void_p]
    kernel32.GlobalUnlock.argtypes = [ctypes.c_void_p]
    user32.OpenClipboard(None)
    try:
        handle = user32.GetClipboardData(CF_UNICODETEXT)
        if not handle:
            return None
        ptr = kernel32.GlobalLock(handle)
        if not ptr:
            return None
        try:
            return ctypes.c_wchar_p(ptr).value
        finally:
            kernel32.GlobalUnlock(handle)
    finally:
        user32.CloseClipboard()


def find_window(predicate, tries=120, pause=0.5):
    for _ in range(tries):
        for hwnd, cls, title in enumerate_windows():
            if predicate(cls, title):
                return hwnd, title
        time.sleep(pause)
    return None, None


def wait_for_log(path, needle, tries=6, pause=2.5):
    for _ in range(tries):
        if os.path.exists(path):
            blob = open(path, encoding="utf-8", errors="replace").read()
            if needle in blob:
                return True
        time.sleep(pause)
    return False


def activate(hwnd, log_path, needle, cycles=6):
    """Cycle the input list until the TIP logs an activation."""
    for i in range(cycles):
        if os.path.exists(log_path) and needle in open(log_path, encoding="utf-8", errors="replace").read():
            return i
        if user32.GetForegroundWindow() != hwnd:
            user32.SetForegroundWindow(hwnd)
            time.sleep(0.4)
        send_vk(0x5B)          # VK_LWIN
        tap(0x20)              # VK_SPACE
        send_vk(0x5B, up=True)
        time.sleep(2.5)
    return None


def focus(hwnd):
    """Bring the host window to the front and verify it took.

    SetForegroundWindow is refused for a process that does not own the
    foreground, so an ALT tap first satisfies the foreground lock; the
    verification is what makes a refused call visible instead of a round that
    types into somebody else's window.
    """
    for _ in range(5):
        if user32.GetForegroundWindow() == hwnd:
            return True
        tap(0x12)                # VK_MENU
        user32.ShowWindow(hwnd, 9)   # SW_RESTORE
        user32.SetForegroundWindow(hwnd)
        time.sleep(1.0)
    return user32.GetForegroundWindow() == hwnd


def read_document():
    chord(0x11, 0x41)            # Ctrl+A
    time.sleep(0.3)
    chord(0x11, 0x43)            # Ctrl+C
    time.sleep(0.8)
    return clipboard_text()


def close_stale_notepad(tries=8, pause=0.4):
    """Close every Notepad window before launching the round's own.

    Win11 Notepad is a PACKAGED app: `notepad.exe` hands off to the AppX
    process, and a process that is ALREADY running keeps the environment it
    was started with. A leftover instance from an earlier attempt therefore
    holds no HUNMIN_TSF_LOG_DIR/HUNMIN_TSF_DEBUG_LOG, the TIP writes no log in
    the run dir, and the activation wait fails as "profile not activated;
    applied rows: []" — measured twice (2026-10-28) before the cause was seen.
    Close the stale windows first, and say so when one refuses to go (a dirty
    document opens a save prompt and the round cannot proceed).
    """
    def notepad_windows():
        found = []

        @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
        def cb(hwnd, lparam):
            cls = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(hwnd, cls, 256)
            if cls.value == "Notepad":
                found.append(hwnd)
            return True

        user32.EnumWindows(cb, 0)
        return found

    stale = notepad_windows()
    if not stale:
        return 0
    for hwnd in stale:
        user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
    for _ in range(tries):
        time.sleep(pause)
        if not notepad_windows():
            return len(stale)
    raise RuntimeError(
        "a stale Notepad window refused to close (a dirty document opens a save prompt); "
        "the round cannot launch a fresh process that carries the log env vars")


def run_notepad(run_dir, log_path, keys, hwnd_hint=None):
    env = dict(os.environ, HUNMIN_TSF_DEBUG_LOG="1", HUNMIN_TSF_LOG_DIR=run_dir)
    close_stale_notepad()
    subprocess.Popen(["notepad.exe"], env=env,
                     creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
    # Win11 Notepad is a packaged app: the process we spawn hands the window to
    # another one, so the window is found by class, and the document is read
    # back through the clipboard rather than over UIA by pid.
    hwnd, title = find_window(lambda cls, _t: cls == "Notepad")
    if not hwnd:
        raise RuntimeError("no Notepad window")
    focus(hwnd)
    return hwnd, title


def run_edge(run_dir, log_path, keys, page):
    env = dict(os.environ, HUNMIN_TSF_DEBUG_LOG="1", HUNMIN_TSF_LOG_DIR=run_dir)
    profile = os.path.join(run_dir, "edge-profile")
    subprocess.Popen([EDGE, "--new-window", "file:///" + page.replace("\\", "/"),
                      "--no-first-run", "--no-default-browser-check",
                      "--user-data-dir=" + profile],
                     env=env, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
    hwnd, title = find_window(lambda cls, t: cls.startswith("Chrome_WidgetWin") and "TXTV[" in t, tries=180)
    if not hwnd:
        raise RuntimeError("no Edge window running the probe page")
    focus(hwnd)
    return hwnd, title


def doc_from_title(title):
    if "TXTV[" not in title:
        return None
    body = title.split("TXTV[", 1)[1].split("]ENDT", 1)[0]
    from urllib.parse import unquote
    return unquote(body)


PAGE = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>TXTV[]ENDT</title>
<style>textarea{width:600px;height:120px;font-size:22px}</style>
</head>
<body>
<textarea id="t" autofocus></textarea>
<script>
  const t = document.getElementById('t');
  t.focus();
  const sync = () => { document.title = 'TXTV[' + encodeURIComponent(t.value) + ']ENDT'; };
  t.addEventListener('input', sync);
  t.addEventListener('compositionupdate', sync);
  t.addEventListener('compositionend', sync);
  setInterval(sync, 100);
</script>
</body>
</html>
"""


def set_store_layout(layout):
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, STORE, 0, winreg.KEY_READ | winreg.KEY_WRITE) as k:
        previous = winreg.QueryValueEx(k, VALUE)[0]
        winreg.SetValueEx(k, VALUE, 0, winreg.REG_SZ, layout)
    return previous


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", choices=("notepad", "edge"), default="edge")
    ap.add_argument("--keys", default="dkssud ")
    ap.add_argument("--layout", default="dubeolsik")
    ap.add_argument("--expect", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--settle", type=float, default=3.0,
                    help="seconds between the host gaining the foreground and the first key")
    ap.add_argument("--keep-open", action="store_true")
    args = ap.parse_args()

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = args.out or os.path.join(ROOT, "build-tsf-devhost", "%s-%s" % (args.host, stamp))
    os.makedirs(run_dir, exist_ok=True)
    log_path = os.path.join(run_dir, "hunmin-tsf-debug.log")
    if os.path.exists(log_path):
        os.remove(log_path)

    page = os.path.join(run_dir, "target.html")
    with open(page, "w", encoding="utf-8") as f:
        f.write(PAGE)

    previous = set_store_layout(args.layout)
    result = {"host": args.host, "keys": args.keys, "layout": args.layout, "run_dir": run_dir}
    try:
        if args.host == "notepad":
            hwnd, title = run_notepad(run_dir, log_path, args.keys)
        else:
            hwnd, title = run_edge(run_dir, log_path, args.keys, page)

        cycles = activate(hwnd, log_path, "applied hangul=%s" % args.layout)
        if cycles is None:
            applied = [l for l in open(log_path, encoding="utf-8", errors="replace").read().splitlines()
                       if "applied hangul=" in l] if os.path.exists(log_path) else []
            raise RuntimeError("profile not activated; applied rows: %r" % applied)
        result["activation_cycles"] = cycles
        # Win+Space can move focus to the input-list UI; re-assert the host.
        if not focus(hwnd):
            raise RuntimeError("host window lost the foreground")

        time.sleep(args.settle)

        if args.host == "notepad":
            # Win11 Notepad restores the previous session's text.
            chord(0x11, 0x41)
            time.sleep(0.3)
            tap(0x2E)            # VK_DELETE
            time.sleep(0.8)

        typed = []
        for kind, ch in split_tokens(args.keys):
            if user32.GetForegroundWindow() != hwnd and not focus(hwnd):
                result["focus_lost_before"] = ch
                break
            if kind == "token":
                run_token(ch)
                typed.append("{%s}" % ch)
                time.sleep(0.35)
                continue
            type_char(ch)
            typed.append(ch)
            time.sleep(0.35)
        result["typed"] = "".join(typed)
        time.sleep(1.5)

        if args.host == "notepad":
            text = read_document()
        else:
            _hwnd, title = find_window(lambda cls, t: cls.startswith("Chrome_WidgetWin") and "TXTV[" in t, tries=4)
            text = doc_from_title(title or "")
        result["text"] = text
        if args.expect is not None:
            result["expect"] = args.expect
            result["pass"] = 1 if text == args.expect else 0
    finally:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, STORE, 0, winreg.KEY_READ | winreg.KEY_WRITE) as k:
            winreg.SetValueEx(k, VALUE, 0, winreg.REG_SZ, previous)
        with open(os.path.join(run_dir, "result.json"), "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=2)
        if os.path.exists(log_path):
            rows = [l for l in open(log_path, encoding="utf-8", errors="replace").read().splitlines()
                    if any(t in l for t in ("applied hangul", "legacy", "composition", "consumed",
                                            "interim:", "flush:", "boundary",
                                            "langkey", "langchord"))]
            result["trace"] = rows
            with open(os.path.join(run_dir, "result.json"), "w", encoding="utf-8") as f:
                json.dump(result, f, ensure_ascii=False, indent=2)

    print(json.dumps({k: result.get(k) for k in ("host", "layout", "keys", "typed", "text", "expect", "pass")},
                     ensure_ascii=False))
    print("run:", run_dir)
    return 0 if result.get("pass", 1) == 1 else 1


if __name__ == "__main__":
    sys.exit(main())
