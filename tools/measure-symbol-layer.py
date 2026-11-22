#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Measure the symbol-layer options for issue #298 without changing shipped data.

The probe answers tree facts the design analysis needs:
  * which modifier rows the shipped layouts carry, and which ASCII punctuation
    their literal rows already reach;
  * whether `tools/doctor.py` and the engine agree about a third `symbol` row
    and the reserved `alt` row;
  * whether the current option surface accepts a symbol-layer option;
  * whether a separate symbol page can be expressed as an ordinary layout file
    with no engine change;
  * what the current mode-toggle and external-text paths deliver after a
    buffered Hangul word.

It writes scratch copies under the build directory, never under libhunmin/data.
The output is a measurement, not a ruling; the maintainer chooses the design.

Usage:
  python3 tools/measure-symbol-layer.py --build-dir build
  python3 tools/measure-symbol-layer.py --build-dir build --out-dir build/symbol-layer

With --out-dir the tool writes measurements.json and meta.json beside it.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import shutil
import string
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMMIT_AT_BOUNDARY = 0


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_lib(path: Path) -> ctypes.CDLL:
    if not path.is_file():
        raise SystemExit(f"missing engine library: {path}; build it first")
    lib = ctypes.CDLL(str(path))
    lib.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
    lib.hunmin_ctx_new.restype = ctypes.c_void_p
    lib.hunmin_ctx_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_ctx_layout_count.argtypes = [ctypes.c_void_p]
    lib.hunmin_ctx_layout_count.restype = ctypes.c_int
    lib.hunmin_ctx_layout_id.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.hunmin_ctx_layout_id.restype = ctypes.c_char_p
    lib.hunmin_ctx_layout_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.hunmin_ctx_layout_mode.restype = ctypes.c_char_p
    lib.hunmin_ic_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.hunmin_ic_new.restype = ctypes.c_void_p
    lib.hunmin_ic_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_ic_process_key.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
    lib.hunmin_ic_process_key.restype = ctypes.c_bool
    lib.hunmin_ic_commit.argtypes = [ctypes.c_void_p]
    lib.hunmin_ic_commit.restype = ctypes.c_char_p
    lib.hunmin_ic_preedit.argtypes = [ctypes.c_void_p]
    lib.hunmin_ic_preedit.restype = ctypes.c_char_p
    lib.hunmin_ic_set_option.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    lib.hunmin_ic_set_option.restype = ctypes.c_bool
    lib.hunmin_buffered_preedit_new.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    lib.hunmin_buffered_preedit_new.restype = ctypes.c_void_p
    lib.hunmin_buffered_preedit_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_buffered_preedit_process_key.argtypes = [
        ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    lib.hunmin_buffered_preedit_process_key.restype = ctypes.c_int
    lib.hunmin_buffered_preedit_delivered.argtypes = [ctypes.c_void_p]
    lib.hunmin_buffered_preedit_delivered.restype = ctypes.c_char_p
    lib.hunmin_buffered_preedit_display.argtypes = [ctypes.c_void_p]
    lib.hunmin_buffered_preedit_display.restype = ctypes.c_char_p
    lib.hunmin_buffered_preedit_toggle_language.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    lib.hunmin_buffered_preedit_toggle_language.restype = ctypes.c_bool
    lib.hunmin_buffered_preedit_external_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.hunmin_buffered_preedit_external_text.restype = ctypes.c_char_p
    lib.hunmin_buffered_preedit_set_option.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    lib.hunmin_buffered_preedit_set_option.restype = ctypes.c_bool
    return lib


def new_ctx(lib: ctypes.CDLL, data: Path) -> ctypes.c_void_p:
    arr = (ctypes.c_char_p * 1)(str(data).encode())
    ctx = lib.hunmin_ctx_new(arr, 1)
    if not ctx:
        raise SystemExit(f"hunmin_ctx_new found no layout in {data}")
    return ctx


def ic_key(lib: ctypes.CDLL, ctx: ctypes.c_void_p, layout: str, base: str, shift: int = 0) -> dict:
    ic = lib.hunmin_ic_new(ctx, layout.encode())
    if not ic:
        raise SystemExit(f"hunmin_ic_new failed for {layout}")
    used = lib.hunmin_ic_process_key(ic, ord(base), shift)
    out = {
        "consumed": bool(used),
        "commit": lib.hunmin_ic_commit(ic).decode(),
        "preedit": lib.hunmin_ic_preedit(ic).decode(),
    }
    lib.hunmin_ic_free(ic)
    return out


def layer_events(lib: ctypes.CDLL, ctx: ctypes.c_void_p, hangul: str, latin: str | None,
                 keys: list[tuple[str, int]], toggle: bool = False,
                 external: str | None = None) -> list[dict]:
    layer = lib.hunmin_buffered_preedit_new(
        ctx, hangul.encode(), latin.encode() if latin else None, COMMIT_AT_BOUNDARY)
    if not layer:
        raise SystemExit(f"hunmin_buffered_preedit_new failed for {hangul}/{latin}")
    events = []
    for base, shift in keys:
        used = lib.hunmin_buffered_preedit_process_key(layer, ord(base), shift, None)
        events.append({
            "event": "key", "base": base, "shift": shift, "consumed": bool(used),
            "delivered": lib.hunmin_buffered_preedit_delivered(layer).decode(),
            "display": lib.hunmin_buffered_preedit_display(layer).decode(),
        })
    if toggle:
        switched = lib.hunmin_buffered_preedit_toggle_language(layer, None)
        events.append({
            "event": "toggle_language", "switched": bool(switched),
            "delivered": lib.hunmin_buffered_preedit_delivered(layer).decode(),
            "display": lib.hunmin_buffered_preedit_display(layer).decode(),
        })
        for base, shift in [("1", 1)]:
            used = lib.hunmin_buffered_preedit_process_key(layer, ord(base), shift, None)
            events.append({
                "event": "key", "base": base, "shift": shift, "consumed": bool(used),
                "delivered": lib.hunmin_buffered_preedit_delivered(layer).decode(),
                "display": lib.hunmin_buffered_preedit_display(layer).decode(),
            })
        switched_back = lib.hunmin_buffered_preedit_toggle_language(layer, None)
        events.append({
            "event": "toggle_language_back", "switched": bool(switched_back),
            "delivered": lib.hunmin_buffered_preedit_delivered(layer).decode(),
            "display": lib.hunmin_buffered_preedit_display(layer).decode(),
        })
    if external is not None:
        out = lib.hunmin_buffered_preedit_external_text(layer, external.encode()).decode()
        events.append({
            "event": "external_text", "text": external, "delivered": out,
            "display": lib.hunmin_buffered_preedit_display(layer).decode(),
        })
    lib.hunmin_buffered_preedit_free(layer)
    return events


def literal_outputs(layout: dict) -> set[str]:
    out = set()
    for mods in layout["keys"].values():
        for value in mods.values():
            if isinstance(value, str) and value.startswith("=") and len(value) >= 2:
                out.add(value[1:])
    return out


def census(data: Path) -> list[dict]:
    rows = []
    punct = set(string.punctuation)
    for path in sorted((data / "layouts").glob("*.json")):
        layout = json.loads(path.read_text(encoding="utf-8"))
        mods: dict[str, int] = {}
        for key_mods in layout["keys"].values():
            for mod in key_mods:
                mods[mod] = mods.get(mod, 0) + 1
        outputs = literal_outputs(layout)
        rows.append({
            "id": layout["id"],
            "keys": len(layout["keys"]),
            "modifier_counts": dict(sorted(mods.items())),
            "literal_outputs": len(outputs),
            "ascii_punctuation_reachable": len(outputs & punct),
            "ascii_punctuation_missing": "".join(sorted(punct - outputs)),
        })
    return rows


def scratch_data(data: Path, scratch: Path, name: str) -> Path:
    target = scratch / name
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)
    shutil.copytree(data / "layouts", target / "layouts")
    shutil.copytree(data / "compose", target / "compose")
    return target


def doctor_errors(data: Path) -> tuple[int, list[str]]:
    proc = subprocess.run([sys.executable, str(ROOT / "tools/doctor.py"), str(data)],
                          capture_output=True, text=True)
    errors = [line for line in proc.stderr.splitlines() if line.startswith("error:")]
    return proc.returncode, errors


def modifier_probe(lib: ctypes.CDLL, data: Path, scratch: Path, mod: str) -> dict:
    target = scratch_data(data, scratch, f"modifier-{mod}")
    path = target / "layouts/dubeolsik.json"
    layout = json.loads(path.read_text(encoding="utf-8"))
    layout["keys"]["a"][mod] = "=@"
    path.write_text(json.dumps(layout, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    rc, errors = doctor_errors(target)
    ctx = new_ctx(lib, target)
    probe = ic_key(lib, ctx, "dubeolsik", "a")
    lib.hunmin_ctx_free(ctx)
    return {"modifier": mod, "doctor_returncode": rc, "doctor_errors": errors,
            "engine_key_a": probe}


def option_probe(lib: ctypes.CDLL, ctx: ctypes.c_void_p) -> list[dict]:
    ic = lib.hunmin_ic_new(ctx, b"dubeolsik")
    layer = lib.hunmin_buffered_preedit_new(ctx, b"dubeolsik", b"us-qwerty", COMMIT_AT_BOUNDARY)
    if not ic or not layer:
        raise SystemExit("option probe could not build ic/layer")
    rows = []
    for name in ("symbol", "symbol_layer", "alt"):
        rows.append({
            "option": name,
            "ic_set_option": bool(lib.hunmin_ic_set_option(ic, name.encode(), 1)),
            "layer_set_option": bool(lib.hunmin_buffered_preedit_set_option(layer, name.encode(), 1)),
        })
    lib.hunmin_buffered_preedit_free(layer)
    lib.hunmin_ic_free(ic)
    return rows


def page_layout_probe(lib: ctypes.CDLL, data: Path, scratch: Path) -> dict:
    target = scratch_data(data, scratch, "page-layout")
    symbols = string.punctuation
    bases = [chr(ord("a") + i) for i in range(26)] + [str(i) for i in range(6)]
    keys = {base: {"plain": "=" + ch} for base, ch in zip(bases, symbols)}
    layout = {
        "schemaVersion": 1,
        "id": "android-symbols",
        "name": {"en": "Android symbols (scratch probe)", "ko": "기호 (probe)"},
        "engine": "std",
        "mode": "latin",
        "compose": ["modern"],
        "source": "scratch probe for issue #298; not a shipped artefact",
        "keys": keys,
    }
    (target / "layouts/android-symbols.json").write_text(
        json.dumps(layout, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    rc, errors = doctor_errors(target)
    ctx = new_ctx(lib, target)
    count = lib.hunmin_ctx_layout_count(ctx)
    mode = None
    for i in range(count):
        if lib.hunmin_ctx_layout_id(ctx, i).decode() == "android-symbols":
            mode = lib.hunmin_ctx_layout_mode(ctx, i).decode()
    probe = ic_key(lib, ctx, "android-symbols", "a")
    lib.hunmin_ctx_free(ctx)
    return {
        "doctor_returncode": rc,
        "doctor_errors": errors,
        "layout_count": count,
        "mode": mode,
        "engine_key_a": probe,
        "symbol_count": len(symbols),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data", type=Path, default=ROOT / "libhunmin/data")
    ap.add_argument("--build-dir", type=Path, default=ROOT / "build")
    ap.add_argument("--lib", type=Path, default=None,
                    help="engine library; default <build-dir>/libhunmin.so")
    ap.add_argument("--scratch-dir", type=Path, default=None,
                    help="scratch copies; default <build-dir>/symbol-layer-probe")
    ap.add_argument("--out-dir", type=Path, default=None,
                    help="write measurements.json and meta.json here")
    args = ap.parse_args()
    data = args.data.resolve()
    build = args.build_dir.resolve()
    lib_path = (args.lib or build / "libhunmin.so").resolve()
    scratch = (args.scratch_dir or build / "symbol-layer-probe").resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    lib = load_lib(lib_path)
    ctx = new_ctx(lib, data)

    measurements = {
        "layout_census": census(data),
        "modifier_probes": [modifier_probe(lib, data, scratch, mod) for mod in ("symbol", "alt")],
        "option_probes": option_probe(lib, ctx),
        "page_layout_probe": page_layout_probe(lib, data, scratch),
        "hangul_word_toggle_symbol_toggle_back": layer_events(
            lib, ctx, "dubeolsik", "us-qwerty",
            [("d", 0), ("k", 0), ("s", 0), ("s", 0), ("u", 0), ("d", 0)], toggle=True),
        "hangul_word_then_external_bang": layer_events(
            lib, ctx, "dubeolsik", "us-qwerty",
            [("d", 0), ("k", 0), ("s", 0), ("s", 0), ("u", 0), ("d", 0)], external="!"),
        "abi_symbol_count": len([line for line in (ROOT / "libhunmin/tests/abi/symbols.txt")
                                 .read_text(encoding="utf-8").splitlines() if line.strip()]),
    }
    lib.hunmin_ctx_free(ctx)

    meta = {
        "tool": "tools/measure-symbol-layer.py",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "issue": "https://github.com/scobyseo/hunmin/issues/298",
        "sources": {
            "data_dir": str(data),
            "engine_library": str(lib_path),
            "sha256": {
                "libhunmin/data/layouts/dubeolsik.json": sha256(data / "layouts/dubeolsik.json"),
                "libhunmin/data/layouts/us-qwerty.json": sha256(data / "layouts/us-qwerty.json"),
                "libhunmin/tests/abi/symbols.txt": sha256(ROOT / "libhunmin/tests/abi/symbols.txt"),
                "engine_library": sha256(lib_path),
            },
        },
        "method": "ctypes probes against the built engine; doctor.py on scratch copies; "
                  "no shipped data file is modified",
        "excluded": "Android gesture timing, accessibility, landscape geometry, non-ASCII "
                    "symbol coverage, corpus frequency, and any maintainer ruling",
        "does_not_represent": "a chosen design or a shipped symbol table; the scratch "
                              "android-symbols layout is a feasibility probe only",
    }
    blob = {"meta": meta, "measurements": measurements}
    text = json.dumps(blob, ensure_ascii=False, indent=2) + "\n"
    if args.out_dir:
        out = args.out_dir.resolve()
        out.mkdir(parents=True, exist_ok=True)
        (out / "measurements.json").write_text(text, encoding="utf-8")
        (out / "meta.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
                                       encoding="utf-8")
        print(f"wrote {out / 'measurements.json'} and {out / 'meta.json'}")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
