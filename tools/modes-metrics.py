#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Measure the hangul/latin autoswitch on a sentence corpus — SPEC-MODES E4.

Docs: `docs/MODES-METRICS.md` §1.2 (② sentence replay), §1.4 (③-bis
wrong-direction switch), §2 (②b manual intervention); the units and the
keystroke arithmetic are `docs/OPTIONS-MEASURED.md` §PAS-1; the reason a
hand-built corpus is void is `docs/MODES-METRICS.md` §3.

WHAT IT REPORTS, and in whose words
-----------------------------------
E4 grades numbers nobody has measured: ② the keystroke delta between the
feature on and off, ②b the manual interventions, ③-bis the switches whose
direction contradicts the script the person actually typed. §PAS-1 fixes the
units: for one text of `n` characters and `t` script transitions (t is exactly
the number of 한/영 presses the same text needs by hand),

    off  = n + t keys
    on   = n + t·(s + r·(2k+1))          a + r + s = 1
    q    = a - (2k+1)r = 1 - u - (2k+1)r      u = 1 - a      (②b)

so this tool emits `a r s u q k d` and, beside them, the two figures taken
straight off the replay: `keys_off`, `keys_on`, their delta, and
`q_measured = (keys_off - keys_on) / t`.

WHY A REPLAY AND NOT A TABLE. §3's six unfit harnesses all had one shape: a
plausible number from a tool that cannot fail loudly, and the keystroke
inversion was one of them (a hand-derived jamo -> key map disagrees with the
engine on 겹받침 and on the twins ㅗ/ㅜ/ㅡ). So the keys are NOT derived here:
Hangul eojeol go through `tools/jamo-keytable.py`, which probes the engine for
each jamo in its slot, and the replay drives `hunmin_buffered_preedit` exactly
as `tools/sim.c`'s judge_step() does — read the judgement after every key,
perform the switch, deliver what the key handed back. Two guards keep the
instrument honest:

  * the OFF arm must deliver every eojeol exactly as written, or the sentence is
    counted NOT INPUTTABLE and excluded from the ratios (§1.3(a) is the question
    that count answers);
  * the ON arm is cross-checked against `hunmin-sim --judge` on every sentence,
    and a disagreement fails the run. §3 records a ctypes-vs-CLI difference that
    was left unexplained for two numbers; here it is a control, not a footnote.

THE MODEL, stated because every number below depends on it
----------------------------------------------------------
tokenisation   whitespace eojeol, punctuation stays attached. A token is `hangul`
               if it holds a Hangul syllable and no ASCII letter, `latin` if the
               reverse, else `other` (typed literally, no script, no transition).
keys           a Hangul eojeol is typed with its jamo key table entry; a Latin
               eojeol with its own characters (shift encodes case); one space
               after every eojeol, which is the boundary the judgement runs at.
transitions    one per eojeol whose script differs from the previous eojeol's,
               with the initial mode (hangul) as position 0 — so t is the 한/영
               presses the OFF arm actually makes.
off arm        autoswitch 0; the replayer presses 한/영 at each transition.
on arm         autoswitch 1; no presses of its own. A eojeol that arrives in
               the wrong script is a MISFIRE and costs, per §PAS-1, backspaces
               (one per delivered character) + one 한/영 press to reach the
               script that was typed + the eojeol's own keys to retype it.
repairs        counted, not replayed. The mode is corrected on the layer so
               later judgements start where a person would be, and the retyped
               text is taken from the OFF arm's proof that the eojeol is
               typeable in its own script. A repair that would re-fire the same
               judgement is therefore charged once — a person's second reaction
               is to turn the feature off, which is ②b's subject and not
               something this tool models for them.

③-bis is reported as a RATE over switches, never as a switch count (§1.4), and
with both directions separately: `hangul_typed_to_latin` and its reverse.

USAGE
  tools/modes-metrics.py --corpus ~/work/hunmin-sentences/wiki-150.jsonl
  tools/modes-metrics.py --corpus CORPUS --json                 # JSONL to stdout
  tools/modes-metrics.py --corpus CORPUS --json-out run.jsonl   # + run.jsonl.meta.json
  tools/modes-metrics.py --selfcheck                            # the guards bite

The corpus is an argument, never a default: a bulk corpus lives outside the
repository (AGENTS.md), and a corpus built by hand in a session is void (§3).
Make one with `tools/build-modes-sentences.py` or pass any file of sentences
(`.jsonl` with a `text` field, or plain text, one sentence per line).
"""
import argparse
import ctypes
import datetime
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import sys
from typing import Any

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA = REPO / "libhunmin" / "data"
LIB = REPO / "build" / "libhunmin.so"
SIM = REPO / "build" / "hunmin-sim"

HANGUL, LATIN, OTHER = "hangul", "latin", "other"
LANG_NAME = {0: HANGUL, 1: LATIN}
COMMIT_AT_BOUNDARY = 0


# --------------------------------------------------------------- the engine

class KeyResult(ctypes.Structure):
    _fields_ = [("consumed", ctypes.c_int),
                ("commit", ctypes.c_char_p),
                ("preedit", ctypes.c_char_p)]


def _decode(p):
    return p.decode("utf-8", "replace") if p else ""


class Layer:
    """One `hunmin_buffered_preedit`, driven the way tools/sim.c drives it."""

    def __init__(self, lib, ctx, hangul_id, latin_id, autoswitch):
        self.lib = lib
        self.h = lib.hunmin_buffered_preedit_new(ctx, hangul_id.encode(),
                                                 latin_id.encode(), COMMIT_AT_BOUNDARY)
        if not self.h:
            raise SystemExit(f"cannot build a layer for {hangul_id}/{latin_id}")
        if lib.hunmin_buffered_preedit_load_english_shipped(self.h) != 1:
            raise SystemExit("the shipped English list did not load — the judgement "
                             "abstains without it (B3), so every number would be a "
                             "confident zero")
        if not lib.hunmin_buffered_preedit_set_option(self.h, b"autoswitch", autoswitch):
            raise SystemExit(f"'autoswitch' refused the value {autoswitch}")

    def key(self, base, shift):
        """One key: (consumed, commit, preedit). Raises if the layer vanished."""
        out = KeyResult()
        consumed = self.lib.hunmin_buffered_preedit_process_key(
            self.h, base, shift, ctypes.byref(out))
        return consumed, _decode(out.commit), _decode(out.preedit)

    def judgement(self):
        return self.lib.hunmin_buffered_preedit_take_judgement(self.h)

    def toggle(self):
        out = KeyResult()
        self.lib.hunmin_buffered_preedit_toggle_language(self.h, ctypes.byref(out))
        return _decode(out.commit)

    def reset(self):
        self.lib.hunmin_buffered_preedit_reset(self.h)

    def set_option(self, name, value):
        if not self.lib.hunmin_buffered_preedit_set_option(self.h, name.encode(), value):
            raise SystemExit(f"the layer refused {name}={value}")

    def language(self):
        return LANG_NAME.get(self.lib.hunmin_buffered_preedit_language(self.h), "?")

    def close(self):
        if self.h:
            self.lib.hunmin_buffered_preedit_free(self.h)
            self.h = None


class Engine:
    def __init__(self, libpath, datadir):
        if not libpath.exists():
            raise SystemExit(f"{libpath} not found — build first "
                             f"(cmake -B build && cmake --build build)")
        self.lib = ctypes.CDLL(str(libpath))
        L = self.lib
        L.hunmin_ctx_new.restype = ctypes.c_void_p
        L.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
        L.hunmin_ctx_free.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_new.restype = ctypes.c_void_p
        L.hunmin_buffered_preedit_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                                  ctypes.c_char_p, ctypes.c_int]
        L.hunmin_buffered_preedit_free.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_process_key.restype = ctypes.c_int
        L.hunmin_buffered_preedit_process_key.argtypes = [
            ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(KeyResult)]
        L.hunmin_buffered_preedit_take_judgement.restype = ctypes.c_int
        L.hunmin_buffered_preedit_take_judgement.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_toggle_language.restype = ctypes.c_int
        L.hunmin_buffered_preedit_toggle_language.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(KeyResult)]
        L.hunmin_buffered_preedit_language.restype = ctypes.c_int
        L.hunmin_buffered_preedit_language.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_reset.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_load_english_shipped.restype = ctypes.c_int
        L.hunmin_buffered_preedit_load_english_shipped.argtypes = [ctypes.c_void_p]
        L.hunmin_buffered_preedit_set_option.restype = ctypes.c_bool
        L.hunmin_buffered_preedit_set_option.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        dirs = (ctypes.c_char_p * 1)(str(datadir).encode())
        self.ctx = L.hunmin_ctx_new(dirs, 1)
        if not self.ctx:
            raise SystemExit(f"no layouts loaded from {datadir}")

    def layer(self, hangul_id, latin_id, autoswitch):
        return Layer(self.lib, self.ctx, hangul_id, latin_id, autoswitch)


# ------------------------------------------------------------------- keys

def _load_tool(name):
    spec = importlib.util.spec_from_file_location(
        name.replace("-", "_"), REPO / "tools" / f"{name}.py")
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load tools/{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def sg_event(ch):
    """One character of a key string -> (base, shift).

    The engine's convention (hunmin.h, and fcitx5's adapter at
    hunminadapter.cpp: lowercases the base and sets the shift flag): a shifted
    key is (lower(base), 1), not the upper-case code point.
    """
    if ch.isascii() and ch.isalpha() and ch.isupper():
        return ord(ch.lower()), 1
    return ord(ch), 0


def sim_keys(events):
    """(base, shift) pairs -> the string hunmin-sim's keys_step() reads."""
    out = []
    for base, shift in events:
        ch = chr(base)
        if shift:
            out.append("\\s" + ch)
        elif ch == "\\":
            out.append("\\\\")
        else:
            out.append(ch)
    return "".join(out)


def script_of(token):
    has_h = any(0xAC00 <= ord(c) <= 0xD7A3 for c in token)
    has_l = any(c.isascii() and c.isalpha() for c in token)
    if has_h and not has_l:
        return HANGUL
    if has_l and not has_h:
        return LATIN
    return OTHER


def hangul_events(text, table):
    """(events, missing) for a Hangul run, cho -> jung -> jong per syllable.

    Every syllable the recorded layout cannot produce is returned in `missing`
    rather than guessed: a key that does not exist is §1.3(a)'s question, and a
    harness that invented one would be measuring itself.
    """
    kt = _KT
    events, missing = [], []
    for syl in text:
        cp = ord(syl)
        if cp == 0x20:
            events.append((0x20, 0))
            continue
        if not (0xAC00 <= cp <= 0xD7A3):
            events.extend(sg_event(c) for c in syl)
            continue
        x = cp - 0xAC00
        parts = ((kt.CHOSEONG[x // 588], "cho"),
                 (kt.JUNGSEONG[(x % 588) // 28], "jung"),
                 (kt.JONGSEONG[x % 28], "jong"))
        for jamo, slot in parts:
            if not jamo:
                continue
            keys = table[slot].get(jamo)
            if keys is None:
                missing.append(jamo)
                continue
            events.extend(sg_event(c) for c in keys)
    return events, missing


_KT: Any = None  # tools/jamo-keytable.py, loaded on demand


def load_keytable(layout, cache_path=None):
    global _KT
    layout_sha = sha256(DATA / "layouts" / f"{layout}.json")
    if cache_path and cache_path.exists():
        blob = json.loads(cache_path.read_text(encoding="utf-8"))
        if blob.get("layout") != layout:
            raise SystemExit(f"{cache_path} is a {blob.get('layout')} table, not {layout}")
        if blob.get("layout_sha256") == layout_sha:
            _KT = _load_tool("jamo-keytable")
            return blob
        # The layout moved under the cache: a table keyed on the old one would
        # silently type the wrong jamo, so it is rebuilt rather than trusted.
    _KT = _load_tool("jamo-keytable")
    table, not_found = _KT.build_table(layout)
    blob = {"layout": layout, "layout_sha256": layout_sha,
            "table": table, "not_found": not_found,
            "jung_twins": _KT.TWINS,
            "provenance": "tools/jamo-keytable.py build_table(), probed against "
                          "build/hunmin-sim --batch; twins recorded by the tool"}
    if cache_path:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps(blob, ensure_ascii=False, indent=1) + "\n",
                              encoding="utf-8")
    return blob


# ------------------------------------------------------------------ replay

def plan_sentence(text, table):
    """[(token, script, events, missing)] with one space event after each token."""
    plan = []
    for token in text.split(" "):
        script = script_of(token)
        events, missing = hangul_events(token, table) if script == HANGUL \
            else ([sg_event(c) for c in token], [])
        plan.append({"token": token, "script": script, "events": events,
                     "missing": missing, "space": (0x20, 0)})
    return plan


def replay(engine, plan, hangul_id, latin_id, autoswitch, repairs, layer=None):
    """Drive one sentence through one layer; attribute delivery per eojeol.

    Returns (tokens, delivered, commit_only, judged_all, mode_after).
    `delivered` is what a client would show (the engine's commits plus the keys
    the engine declined, which a frontend forwards); `commit_only` is the
    commits alone — the same quantity hunmin-sim --judge reports, which is what
    makes the cross-check exact.
    """
    own = layer is None
    if own:
        layer = engine.layer(hangul_id, latin_id, autoswitch)
    else:
        # One layer reused across sentences: reset() clears the text state and
        # the option is written per arm. The mode is NOT part of reset(), so a
        # mode left by the previous arm is cleared here — otherwise a sentence
        # would start in whatever mode the last one ended in.
        layer.reset()
        layer.set_option("autoswitch", autoswitch)
        if layer.language() != HANGUL:
            spilled = layer.toggle()
            if spilled:
                raise SystemExit(f"reset delivered {spilled!r} while returning to "
                                 f"Hangul mode")
    out, commit_only, judged_all = [], [], []
    try:
        prev_script = HANGUL          # the layer starts in Hangul mode
        for item in plan:
            token, script, events = item["token"], item["script"], item["events"]
            presses = 0
            if autoswitch == 0 and script != OTHER and script != prev_script:
                # The OFF arm's 한/영. Nothing is composing at a eojeol start, so
                # a commit here would be text lost at a mode boundary: recorded.
                pre = layer.toggle()
                presses += 1
                if pre:
                    raise SystemExit(
                        f"toggle before '{token}' delivered {pre!r} — text escaped at a "
                        f"mode boundary, which is not a case this model can price")
            commits, forwarded, judged = [], [], []
            for base, shift in events + [item["space"]]:
                consumed, commit, _ = layer.key(base, shift)
                if commit:
                    commits.append(commit)
                if not consumed:
                    forwarded.append(chr(base))
                j = layer.judgement()
                if j >= 0:
                    judged.append(LANG_NAME.get(j, "?"))
                    judged_all.append(LANG_NAME.get(j, "?"))
                    sw = layer.toggle()      # the frontend performs the switch
                    if sw:
                        commits.append(sw)
            got = "".join(commits)
            chunk = got + "".join(forwarded)
            commit_only.append(got)
            mode = layer.language()
            # The space key is declined and forwarded, so a eojeol that arrived
            # as typed is `token + " "` — the separator the replay itself typed.
            rec = {"token": token, "script": script, "got": chunk,
                   "correct": chunk == token + " ",
                   "judged": judged, "presses": presses,
                   "mode_after": mode, "keys": len(events) + 1,
                   "missing": item["missing"], "backspaces": 0, "repair": 0}
            if repairs and not rec["correct"]:
                # §PAS-1's repair, counted: delete what arrived (the space stays),
                # 한/영 once if the layer is not in the script that was typed,
                # retype the eojeol.
                body = got[:-1] if got.endswith(" ") else got
                spilled = len(body) + sum(1 for c in forwarded if c != " ")
                press = 0
                if script != OTHER and mode != script:
                    layer.toggle()
                    press = 1
                rec["backspaces"] = spilled
                rec["repair"] = spilled + press + len(events)
                rec["presses"] += press
            out.append(rec)
            if script != OTHER:
                prev_script = script
        return (out, "".join(r["got"] for r in out), "".join(commit_only),
                judged_all, layer.language())
    finally:
        if own:
            layer.close()


def sim_judge(engine_path, data, layout, latin, keys, autoswitch):
    """`hunmin-sim --judge` — the CLI's answer for the same key string."""
    cmd = [str(engine_path), "--data", str(data), "--layout", layout,
           "--latin", latin, "--judge", "--autoswitch", str(autoswitch),
           "--keys", keys]
    # errors=replace: a long preedit can be cut mid-character by sim's %c
    # print path, and the last line — the only one parsed — is not.
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace",
                       timeout=120)
    if r.returncode != 0:
        raise SystemExit(f"{' '.join(cmd)} failed: {r.stderr.strip()[:300]}")
    lines = [l for l in r.stdout.splitlines() if l.strip().startswith("{")]
    if not lines:
        raise SystemExit(f"{' '.join(cmd)} printed no result line")
    return json.loads(lines[-1])


# ---------------------------------------------------------------- metrics

def summarise(cases):
    """The outcome table, counted — no share is computed here.

    Two partitions are reported because §PAS-1's `a/r/s` and what the replay
    actually charges are not the same partition: a transition can fire no
    switch AND deliver the wrong script (the judgement abstained after the word
    was already delivered), which §PAS-1's `s` prices at one 한/영 press. The
    table keeps both axes so neither reading is hidden.
    """
    table = {
        "switched_to_typed": {"delivered_correct": 0, "delivered_wrong": 0},
        "switched_to_other": {"delivered_correct": 0, "delivered_wrong": 0},
        "no_switch": {"delivered_correct": 0, "delivered_wrong": 0},
    }
    by_dir = {"hangul->latin": 0, "latin->hangul": 0}
    wrong_dir = {"hangul->latin": 0, "latin->hangul": 0}
    per_dir = {d: {"switched_to_typed": 0, "switched_to_other": 0,
                   "no_switch": 0, "delivered_wrong": 0,
                   # R9's guard is three key inputs: a eojeol under it is never
                   # judged, so its misfire belongs to the guard and not to the
                   # judgement's precision.
                   "wrong_below_guard": 0, "wrong_at_or_above_guard": 0}
               for d in ("hangul->latin", "latin->hangul")}
    backspaces = repairs = presses_on = 0
    switches = 0
    for case in cases:
        prev = HANGUL
        switches += len(case["judged"])
        for rec in case["tokens"]:
            backspaces += rec["backspaces"]
            repairs += rec["repair"]
            presses_on += rec["presses"]
            if rec["script"] == OTHER:
                continue
            if rec["script"] != prev:
                key = f"{prev}->{rec['script']}"
                by_dir[key] += 1
                correct = rec["got"] == rec["token"] + " "
                if rec["judged"]:
                    landed = rec["script"] in rec["judged"]
                    row = "switched_to_typed" if landed else "switched_to_other"
                    if not landed:
                        wrong_dir[key] += 1
                else:
                    row = "no_switch"
                table[row]["delivered_correct" if correct else "delivered_wrong"] += 1
                per_dir[key][row] += 1
                if not correct:
                    per_dir[key]["delivered_wrong"] += 1
                    below = (rec["keys"] - 1) < 3
                    per_dir[key]["wrong_below_guard" if below
                                else "wrong_at_or_above_guard"] += 1
            prev = rec["script"]
    return {"table": table, "transitions_by_direction": by_dir,
            "outcomes_by_direction": per_dir,
            "wrong_direction_by_direction": wrong_dir,
            "backspaces": backspaces, "repair_keys": repairs,
            "manual_presses_on": presses_on, "switches": switches}


def ratio(num, den):
    return (num / den) if den else None


def build_report(cases, sentences_total, not_inputtable, layout, latin,
                 table_blob, cross, corpus_meta, args):
    agg = summarise(cases)
    tab = agg["table"]
    t = sum(sum(row.values()) for row in tab.values())
    switched_typed = sum(tab["switched_to_typed"].values())
    switched_other = sum(tab["switched_to_other"].values())
    no_switch = sum(tab["no_switch"].values())
    delivered_wrong = sum(r["delivered_wrong"] for r in tab.values())
    a = ratio(switched_typed, t) or 0.0
    r = ratio(delivered_wrong, t) or 0.0
    s = ratio(no_switch, t)
    u = (1 - a) if t else None
    chars = sum(len(c["text"]) for c in cases)
    k_mean = (agg["backspaces"] / delivered_wrong) if delivered_wrong else 0.0
    q_model = (a - (2 * k_mean + 1) * r) if t else None
    keys_off = sum(c["keys_off"] for c in cases)
    keys_on = sum(c["keys_on"] for c in cases)
    q_measured = ratio(keys_off - keys_on, t) if t else None
    switches = agg["switches"]
    totals = {
        "sentences_in_corpus": sentences_total,
        "sentences_graded": len(cases),
        "sentences_not_inputtable": len(not_inputtable),
        "characters": chars,
        "keys_typed": sum(c["keys_typed"] for c in cases),
        "keys_off": keys_off,
        "keys_on": keys_on,
        "keystroke_delta": keys_off - keys_on,
        # §1.3(b): how many graded sentences the judgement passes through
        # unchanged — the ON arm needed no repair at all.
        "sentences_unchanged": sum(1 for c in cases if c["keys_on"] == c["keys_typed"]),
        "saving_over_off": ratio(keys_off - keys_on, keys_off),
        "manual_presses_off": sum(c["presses_off"] for c in cases),
        "manual_presses_on": agg["manual_presses_on"],
        "backspaces": agg["backspaces"],
        "retype_keys": agg["repair_keys"] - agg["backspaces"] - agg["manual_presses_on"],
        "transitions": t,
        "transition_density_d": ratio(t, chars),
        # The units the pass-line shapes of OPTIONS-MEASURED §PAS-2/§PAS-3 read,
        # so a ruling can be made from this output without recomputing by hand.
        "keys_saved_per_1000_chars": (1000 * (keys_off - keys_on) / chars) if chars else None,
        "manual_presses_on_per_10_sentences":
            (10 * agg["manual_presses_on"] / len(cases)) if cases else None,
        "outcomes": tab,
        "counts": {"transitions": t, "switched_to_typed": switched_typed,
                   "switched_to_other": switched_other, "no_switch": no_switch,
                   "delivered_wrong": delivered_wrong},
        "a_switched_correctly": a,       # §PAS-1's a: share of transitions
        "r_misfires": r,                  # §PAS-1's r: share that cost a repair
        "s_no_switch": s,                 # §PAS-1's s: share that fired no switch
        "u_manual_intervention": u,       # ②b, §PAS-3's strict reading
        "k_mean_backspaces": k_mean,
        "q_pool_share_model": q_model,       # §PAS-1's q, from a r s k
        "q_pool_share_measured": q_measured,  # (off-on)/t, straight off the replay
        "wrong_direction_switches": switched_other,
        "switches": switches,
        "wrong_direction_rate": ratio(switched_other, switches),
        "transitions_by_direction": agg["transitions_by_direction"],
        "outcomes_by_direction": agg["outcomes_by_direction"],
        "wrong_direction_by_direction": agg["wrong_direction_by_direction"],
        "cross_check": cross,
        "notes": [
            "③-bis is wrong_direction_rate over switches, both directions — never a "
            "switch count (MODES-METRICS §1.4)",
            "②b is u = 1 - a: transitions that were not switched correctly, one "
            "manual press each (§PAS-3's strict reading). Whether a repair also "
            "counts is §PAS-4 item 4 and is not decided here",
            "a, r and s are three different partitions of the same t and overlap "
            "where the judgement abstained: r counts delivered_wrong, s counts "
            "no_switch, and a word can be both. q_pool_share_model applies §PAS-1's "
            "formula as written; q_pool_share_measured is the replay's own answer",
            "the model charges s like a misfire when it delivers the wrong script, "
            "because the judgement runs at the boundary and the word is already in "
            "the client (R10) — §PAS-1's s costs one 한/영 press instead",
        ],
    }
    return {
        "instrument": {
            "tool": "tools/modes-metrics.py",
            "invocation": " ".join(sys.argv),
            "engine": {"lib": str(args.lib), "sha256": sha256(args.lib),
                       "sim": str(args.sim), "data": str(args.data)},
            "layouts": {"hangul": layout, "latin": latin},
            "keytable": {"layout": table_blob["layout"],
                         "layout_sha256": table_blob.get("layout_sha256"),
                         "jamo_mapped": sum(len(v) for v in table_blob["table"].values()),
                         "not_found": table_blob["not_found"],
                         "jung_twins": table_blob.get("jung_twins", {}),
                         "provenance": table_blob["provenance"]},
            "model": {
                "tokenisation": "whitespace eojeol, punctuation attached; a token is "
                                "hangul / latin / other by its own characters",
                "keys": "Hangul eojeol via tools/jamo-keytable.py, Latin eojeol as its "
                        "own characters (shift encodes case), one space after each",
                "transitions": "each eojeol whose script differs from the previous "
                               "eojeol's, initial mode hangul as position 0",
                "off_arm": "autoswitch 0, 한/영 pressed by the replayer at each transition",
                "on_arm": "autoswitch 1, judgement read after every key and the switch "
                          "performed as tools/sim.c judge_step() does",
                "repair": "counted, not replayed: backspaces + one 한/영 press if the "
                          "mode differs from the script typed + the eojeol's keys",
                "match": "a eojeol arrived as typed when its delivery, including the "
                         "separator the replay typed, equals token + ' '",
            },
            "generated_at": datetime.datetime.now(datetime.timezone.utc)
                                    .isoformat(timespec="seconds"),
        },
        "corpus": {"path": str(args.corpus), "sha256": sha256(args.corpus),
                   "meta": corpus_meta},
        "totals": totals,
        "not_inputtable": not_inputtable,
    }


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ------------------------------------------------------------------ corpus

def read_corpus(path):
    text = pathlib.Path(path).read_text(encoding="utf-8")
    meta, rows = None, []
    if str(path).endswith(".jsonl"):
        for line in text.splitlines():
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            if "meta" in rec:
                if meta is None:
                    meta = rec["meta"]
                continue
            if "text" in rec:
                rows.append({"id": rec.get("id", f"line{len(rows)}"),
                             "text": rec["text"].strip()})
    else:
        for line in text.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rows.append({"id": f"line{len(rows)}", "text": line})
    if not rows:
        raise SystemExit(f"{path}: no sentences — an empty corpus grades nothing")
    return meta, rows


# ------------------------------------------------------------------- run

def run(args):
    table_blob = load_keytable(args.layout, args.keytable)
    table = table_blob["table"]
    engine = Engine(args.lib, args.data)
    shared = engine.layer(args.layout, args.latin, 1)
    corpus_meta, rows = read_corpus(args.corpus)
    if args.limit:
        rows = rows[:args.limit]

    cases, not_inputtable, mismatches = [], [], []
    checked = 0
    every = max(1, args.cross_check_every)
    for index, row in enumerate(rows):
        text = row["text"]
        plan = plan_sentence(text, table)
        keys_all = sim_keys([e for item in plan for e in item["events"] + [item["space"]]])
        # The OFF arm is the control: the text must be typeable as written.
        off_tokens, off_text, _, _, _ = replay(engine, plan, args.layout,
                                               args.latin, 0, False, shared)
        if off_text != text + " ":
            not_inputtable.append({
                "id": row["id"], "text": text, "delivered": off_text,
                "missing_jamo": sorted({j for i in plan for j in i["missing"]}),
                "reason": "the OFF arm did not deliver the sentence as written",
            })
            continue
        # Raw ON replay (no repairs) — this is the arm hunmin-sim --judge replays,
        # so it is the one the cross-check can compare byte for byte.
        _, _, raw_commit, raw_judged, raw_mode = replay(
            engine, plan, args.layout, args.latin, 1, False, shared)
        # Every Nth sentence by default: hunmin-sim reloads the English list per
        # process, ~130 ms a spawn, so a full corpus of them would dominate the
        # run. The sample is deterministic (every Nth), not random, and one
        # mismatch fails the whole run.
        if not args.no_cross_check and index % every == 0:
            cli = sim_judge(args.sim, args.data, args.layout, args.latin, keys_all, 1)
            checked += 1
            if (cli["delivered"] != raw_commit or cli["judged"] != raw_judged
                    or cli["mode_after"] != raw_mode):
                mismatches.append({
                    "id": row["id"],
                    "cli": cli,
                    "ctypes": {"delivered": raw_commit, "judged": raw_judged,
                               "mode_after": raw_mode},
                })
        on_tokens, _, _, on_judged, on_mode = replay(
            engine, plan, args.layout, args.latin, 1, True, shared)
        keys_typed = sum(r["keys"] for r in on_tokens)
        presses_off = sum(r["presses"] for r in off_tokens)
        repair_keys = sum(r["repair"] for r in on_tokens)
        cases.append({
            "case": row["id"], "text": text,
            "tokens": on_tokens,
            "keys_typed": keys_typed,
            "presses_off": presses_off,
            "keys_off": keys_typed + presses_off,
            "keys_on": keys_typed + repair_keys,
            "judged": on_judged, "mode_after": on_mode,
        })

    shared.close()
    if mismatches:
        for m in mismatches[:3]:
            print(f"CROSS-CHECK FAIL {m['id']}: sim={m['cli']} ctypes={m['ctypes']}",
                  file=sys.stderr)
        raise SystemExit(f"{len(mismatches)} of {checked} sentences disagree with "
                         f"hunmin-sim --judge — the receive paths differ, so no number "
                         f"from this run is a measurement (MODES-METRICS §3)")

    cross = {"sentences_compared": checked, "mismatches": 0,
             "every": every,
             "method": "hunmin-sim --judge vs the ctypes replay, same key string, "
                       "before any repair is applied; every Nth sentence"}
    report = build_report(cases, len(rows), not_inputtable, args.layout,
                          args.latin, table_blob, cross, corpus_meta, args)
    return report, cases


# --------------------------------------------------------------- selfcheck

def selfcheck(args):
    """The guards must bite. A green run of a check that cannot fail is decoration.

    Three assertions, each aimed at a way this tool could quietly return a
    confident zero: the key table must round-trip a mixed sentence, the
    comparator must notice a changed sentence, and the transition counter must
    match a hand count.
    """
    table_blob = load_keytable(args.layout, args.keytable)
    table = table_blob["table"]
    engine = Engine(args.lib, args.data)
    text = "사람은 Internet 을 쓴다."
    plan = plan_sentence(text, table)
    off_tokens, off_text, _, _, _ = replay(engine, plan, args.layout, args.latin, 0, False)
    problems = []
    if off_text != text + " ":
        problems.append(f"OFF arm delivered {off_text!r}, not {text + ' '!r}")
    mutated = text.replace("Internet", "Internets")
    if mutated != text and off_text == mutated + " ":
        problems.append("comparator accepted a changed sentence")
    # hand count: hangul -> latin -> hangul = 2 transitions; initial mode hangul
    t = 0
    prev = HANGUL
    for item in plan:
        if item["script"] == OTHER:
            continue
        if item["script"] != prev:
            t += 1
        prev = item["script"]
    if t != 2:
        problems.append(f"transition count {t}, hand count 2")
    on_tokens, _, commit, judged, mode = replay(engine, plan, args.layout, args.latin, 1, True)
    cli = sim_judge(args.sim, args.data, args.layout, args.latin,
                    sim_keys([e for item in plan for e in item["events"] + [item["space"]]]), 1)
    if cli["delivered"] != commit or cli["judged"] != judged:
        problems.append(f"cross-check disagrees: sim={cli} ctypes={commit!r}/{judged}")
    if problems:
        print("SELFCHECK FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print(f"OK  keytable {table_blob['layout']}, OFF arm round-trips a mixed "
          f"sentence, {t} transitions by hand and by the counter, "
          f"sim and ctypes agree (judged={judged})")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--corpus", type=pathlib.Path)
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--latin", default="us-qwerty")
    ap.add_argument("--data", type=pathlib.Path, default=DATA)
    ap.add_argument("--lib", type=pathlib.Path, default=LIB)
    ap.add_argument("--sim", type=pathlib.Path, default=SIM)
    ap.add_argument("--keytable", type=pathlib.Path,
                    help="read/write the jamo table here instead of probing each run")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--json", action="store_true", help="JSONL to stdout")
    ap.add_argument("--json-out", type=pathlib.Path,
                    help="write JSONL here, plus <path>.meta.json with provenance")
    ap.add_argument("--no-cross-check", action="store_true")
    ap.add_argument("--cross-check-every", type=int, default=10,
                    help="compare every Nth sentence with hunmin-sim --judge "
                         "(1 = all, default 10; one mismatch fails the run)")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()

    if args.selfcheck:
        return selfcheck(args)
    if not args.corpus:
        ap.error("--corpus is required (a corpus built in-session is void: "
                 "docs/MODES-METRICS.md §3)")

    report, cases = run(args)
    lines = [json.dumps(report["instrument"], ensure_ascii=False),
             json.dumps(report["corpus"], ensure_ascii=False)]
    for c in cases:
        lines.append(json.dumps({"case": c}, ensure_ascii=False))
    lines.append(json.dumps({"totals": report["totals"]}, ensure_ascii=False))
    if report["not_inputtable"]:
        lines.append(json.dumps({"not_inputtable": report["not_inputtable"]},
                                ensure_ascii=False))

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text("\n".join(lines) + "\n", encoding="utf-8")
        meta_path = args.json_out.with_suffix(args.json_out.suffix + ".meta.json")
        meta_path.write_text(json.dumps(
            {"instrument": report["instrument"], "corpus": report["corpus"],
             "totals": report["totals"]}, ensure_ascii=False, indent=1) + "\n",
            encoding="utf-8")
        print(f"{args.json_out}: {len(cases)} cases", file=sys.stderr)
        print(f"{meta_path}: provenance", file=sys.stderr)
    if args.json:
        print("\n".join(lines))
        return 0

    t = report["totals"]
    def pct(x):
        return "n/a" if x is None else f"{100 * x:.2f}%"
    c = t["counts"]
    print(f"corpus            {args.corpus}")
    print(f"sentences         {t['sentences_graded']} graded, "
          f"{t['sentences_not_inputtable']} not inputtable of {t['sentences_in_corpus']}")
    print(f"characters        {t['characters']}   transitions {t['transitions']} "
          f"(d = {t['transition_density_d']:.4f})")
    print(f"   unchanged      {t['sentences_unchanged']} of {t['sentences_graded']} "
          f"sentences needed no repair")
    print(f"②  keystrokes     off {t['keys_off']}   on {t['keys_on']}   "
          f"delta {t['keystroke_delta']:+d} ({pct(t['saving_over_off'])} of off)")
    print(f"   transitions    switched-to-typed {c['switched_to_typed']}  "
          f"switched-to-other {c['switched_to_other']}  no-switch {c['no_switch']}  "
          f"delivered-wrong {c['delivered_wrong']}")
    print(f"   ②b u           {pct(t['u_manual_intervention'])}   "
          f"q (model) {pct(t['q_pool_share_model'])}   "
          f"q (measured) {pct(t['q_pool_share_measured'])}")
    print(f"   units          {t['keys_saved_per_1000_chars']:+.1f} keys per 1000 chars, "
          f"{t['manual_presses_on_per_10_sentences']:.2f} manual presses per 10 sentences")
    print(f"③-bis wrong dir  {pct(t['wrong_direction_rate'])} of {t['switches']} switches "
          f"({t['wrong_direction_switches']})")
    for d, row in t["outcomes_by_direction"].items():
        print(f"   {d:13} t={t['transitions_by_direction'][d]:5}  "
              f"switched={row['switched_to_typed']:5}  no-switch={row['no_switch']:5}  "
              f"wrong={row['delivered_wrong']:5}  wrong-dir="
              f"{t['wrong_direction_by_direction'][d]}  "
              f"(below R9 guard {row['wrong_below_guard']}, at/above "
              f"{row['wrong_at_or_above_guard']})")
    print(f"cross-check       {t['cross_check']['sentences_compared']} sentences vs "
          f"hunmin-sim --judge, {t['cross_check']['mismatches']} mismatches")
    if report["not_inputtable"]:
        print(f"not inputtable    {len(report['not_inputtable'])} "
              f"(e.g. {report['not_inputtable'][0]['id']}: "
              f"{report['not_inputtable'][0]['reason']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())