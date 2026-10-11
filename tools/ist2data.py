#!/usr/bin/env python3
"""ist2data.py — lift a 날개셋 ist XML into hunmin data (docs/SPEC-P2.md).

Policies (docs/CODE_RULES.md rule 1, fail-fast):
  * expressions are parsed to an AST (tools/jamo.py), rewritten, unparsed
    canonically; no blind string replacement
  * virtual units (JUNG 501-504..., JONG 601-659, CHO 401-414) resolve through
    the file's own VirtualUnitTable; unknown names never leak as literals
  * every dropped branch is classified (gated-hH | virtual-unresolved | ...)
    into layout `x-notes`; an unexplained drop is a hard error
  * unknown unit ids in conditions stay numeric (branch inert) + noted

Usage: ist2data.py --ist FILE --out DIR --name ID [--alias A]
                   [--compose-extra id1,id2]
"""
import argparse
import functools
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import jamo
from jamo import ExprError, parse_expr, unparse

CHO = {"G": 0x1100, "GG": 0x1101, "N": 0x1102, "D": 0x1103, "DD": 0x1104,
       "R": 0x1105, "M": 0x1106, "B": 0x1107, "BB": 0x1108, "S": 0x1109,
       "SS": 0x110A, "Q": 0x110B, "J": 0x110C, "JJ": 0x110D, "C": 0x110E,
       "K": 0x110F, "T": 0x1110, "P": 0x1111, "H": 0x1112}
JUNG = {"A": 0x1161, "AE": 0x1162, "YA": 0x1163, "YAE": 0x1164, "EO": 0x1165,
        "E": 0x1166, "YEO": 0x1167, "YE": 0x1168, "O": 0x1169, "WA": 0x116A,
        "WAE": 0x116B, "OI": 0x116C, "YO": 0x116D, "U": 0x116E, "UEO": 0x116F,
        "WE": 0x1170, "WI": 0x1171, "YU": 0x1172, "EU": 0x1173, "EUI": 0x1174,
        "I": 0x1175, "F": 0x119E, "FI": 0x11A1, "FF": 0x11A2}
JONG = {"G": 0x11A8, "GG": 0x11A9, "GS": 0x11AA, "N": 0x11AB, "NJ": 0x11AC,
        "NH": 0x11AD, "D": 0x11AE, "R": 0x11AF, "RG": 0x11B0, "RM": 0x11B1,
        "RB": 0x11B2, "RS": 0x11B3, "RT": 0x11B4, "RP": 0x11B5, "RH": 0x11B6,
        "M": 0x11B7, "B": 0x11B8, "BS": 0x11B9, "S": 0x11BA,
        "SS": 0x11BB, "Q": 0x11BC, "J": 0x11BD, "C": 0x11BE, "K": 0x11BF,
        "T": 0x11C0, "P": 0x11C1, "H": 0x11C2}

# canonical right-side vowels (VirtualUnitTable 501-504 → these)
VIRTUAL = {501: JUNG["O"], 502: JUNG["U"], 503: JUNG["EU"], 504: JUNG["F"]}

GATED_RE = re.compile(r"(?<![A-Za-z])[hH](?![A-Za-z])")
ASSIGN_RE = re.compile(r"^\(\s*([A-Za-z])\s*=\s*([^,()]+)\s*,\s*(.*)\)$")


class Ctx:
    def __init__(self):
        self.notes = {"dropped": {}, "inert-ids": set(), "stripped": set()}
        self.errors = []

    def drop(self, reason, where, detail):
        self.notes["dropped"].setdefault(reason, []).append(f"{where}: {detail}")


def split_ternary(s):
    """top-level `cond ? then : else`; returns None when no ternary."""
    depth = 0
    qi = None
    for i, c in enumerate(s):
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif c == "?" and depth == 0:
            qi = i
            break
    if qi is None:
        return None
    depth = 0
    pend = 0
    for i in range(qi + 1, len(s)):
        c = s[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif c == "?" and depth == 0:
            pend += 1
        elif c == ":" and depth == 0:
            if pend == 0:
                return s[:qi].strip(), s[qi + 1:i].strip(), s[i + 1:].strip()
            pend -= 1
    return None


class IstFile:
    def __init__(self, path: Path, ctx: Ctx):
        txt = path.read_text(encoding="utf-8")
        txt = re.sub(r"&#x0*([0-9A-F]{1,2});",
                     lambda m: "" if int(m.group(1), 16) < 0x20
                     and int(m.group(1), 16) not in (9, 10, 13) else m.group(0),
                     txt)
        self.root = ET.fromstring(txt)
        self.path = path
        scheme = self.root.find("InputSchemeSetting")
        self.table = scheme.find("KeyTable") if scheme is not None else None
        gen = self.root.find("GeneratorSetting")
        # VirtualUnitTable: {(unit, id): resolved cp or None}
        self.vu = {}  # (unit, id) -> cp, 0 (=none), or None (unmapped name)
        vu_t = gen.find("VirtualUnitTable") if gen is not None else None
        if vu_t is not None:
            for v in vu_t.findall("VirtualUnit"):
                unit = v.get("unit")
                from_str = v.get("from")
                to = v.get("to")
                if unit is not None and from_str is not None and to is not None:
                    ident = int(from_str, 0)
                    self.vu[(unit, ident)] = self._name_cp(to, ctx)
        # 2024 윗글 받침 ㄸ/ㅃ/ㅉ clones: their UnitMix rows target DD/BB/JJ
        # (받침 유닛은 초성형으로만 존재), and resolving the virtual to that target
        # keeps the emit distinguishable from a plain ㄷ/ㅂ/ㅈ emit — 날개셋
        # keys composition by unit id, we key it by codepoint.
        for ident, to in ((653, "DD"), (656, "BB"), (659, "JJ")):
            if ("JONG", ident) in self.vu and CHO[to]:
                self.vu[("JONG", ident)] = CHO[to]
        # jung virtual boundary set for range rewrites (from this file's data)
        self.virt_jung = {cp for (u, _i), cp in self.vu.items() if u == "JUNG" and cp}
        self.virt_jung_ids = {i for (u, i), cp in self.vu.items() if u == "JUNG" and cp}

    @staticmethod
    def _name_cp(tok, ctx):
        if tok == "0":
            return 0
        neg = tok.startswith("_")
        nm = tok.strip("_")
        tbls = ([JONG, CHO, JUNG] if neg else [CHO, JUNG, JONG])
        for t in tbls:
            if nm in t:
                return t[nm]
        return None

    def compose(self, unit, tok, ctx):
        if tok is None:
            return None
        if tok.isdigit():
            for u in (unit, "JUNG", "CHO", "JONG"):
                cp = self.vu.get((u, int(tok, 10)))
                if cp:
                    return cp
            return None
        cp = self._name_cp(tok, ctx)
        if cp is None:
            return None
        if unit == "JONG" and 0x1100 <= cp <= 0x1112:
            # ㄸ/ㅃ/ㅉ (CHO2JONG==0) have no jongseong form: the 받침 unit
            # DD/BB/JJ stays choseong-typed (engine stores it in the cho slot).
            return jamo.CHO2JONG.get(cp) or cp
        return cp


def rewrite_ast(ast, ist, ctx, ident_of_num_ok):
    # virtual-vowel ids handled below (J / ==code rewrites) — never inert
    VNUMS = {0x1F3: None, 0x1F4: None, 0x1F5: None, 0x1F6: None,
             0x1F7: None, 0x1F8: None}

    def fn(ident, op, num):
        if ident not in ("D", "E", "F"):
            return None
        tbl = (jamo.UNIT_ID_CHO if ident == "D" else
               jamo.UNIT_ID_JONG if ident == "F" else None)
        if tbl is not None and op == "==" and num in tbl:
            return tbl[num]
        # anything else compares against slot codepoints at runtime and can
        # never match a unit id: keep the comparison, account for it
        if not (ident == "E" and num in VNUMS):
            ctx.notes["inert-ids"].add(f"{ident}{op}{hex(num)}")
        return None
    ast = jamo.map_nums(ast, fn)
    del ident_of_num_ok
    VIRT = ("id", "J")        # "current jung is a right-side variant" (engine J)
    REALV = ("&&", ("id", "E"), ("!", ("id", "J")))

    def ecmp_num(n):
        if not (isinstance(n, tuple) and n[0] in ("==", "!=", ">=", "<=", "<", ">")):
            return None
        a, b = n[1], n[2]
        if a == ("id", "E") and b[0] == "num" and b[1] in VNUMS:
            return b[1]
        return None

    def rw(node):
        if not isinstance(node, tuple):
            return node
        if node[0] == "&&" and (ecmp_num(node[1]) is not None or ecmp_num(node[2]) is not None):
            return rw(VIRT)          # whole range test = "current jung is a right-side variant"
        kids = tuple(rw(c) if isinstance(c, tuple) else c for c in node[1:])
        node = (node[0],) + kids
        v = ecmp_num(node)
        if v is not None:
            op = node[0]
            if op == "==" and v in (0x1F5, 0x1F6, 0x1F7, 0x1F8):
                code = {0x1F5: 0x1169, 0x1F6: 0x116E, 0x1F7: 0x1173, 0x1F8: 0x119E}[v]
                return ("&&", ("==", ("id", "E"), ("num", code)), ("id", "J"))
            if op == "==" and v in ist.virt_jung_ids:
                return ("id", "J")
            return node
            if op in (">=", ">") and v in (0x1F3, 0x1F4, 0x1F5):
                return rw(VIRT)
            if op in ("<", "<=") and v in (0x1F4, 0x1F5):
                return rw(REALV)     # "real (non-variant) vowel present"
        return node
    return rw(ast)


def _mark(v):
    """attach G-flag marker to a leaf value (strings / j / tier / noop / case)."""
    if isinstance(v, dict) and "case" in v:
        v["case"]["then"] = _mark(v["case"]["then"])
        v["case"]["else"] = _mark(v["case"]["else"])
        return v
    if isinstance(v, (str, dict)) and v not in (None, {}):
        return {"g": v}
    return v


def value_json(v, ist, ctx, where):
    v = v.strip()
    m = ASSIGN_RE.match(v)
    if m:
        var, val, rest = m.groups()
        inner = value_json(rest, ist, ctx, where)
        if var == "G" and val.strip() == "1" and inner is not None:
            # G flag: state-3 composition gate (docs/SPEC-P2.md automata section)
            return _mark(inner)
        ctx.notes["stripped"].add(f"{var}=…")
        return inner
    if v == "0":
        return {"noop": True}
    t = split_ternary(v)
    if t:
        cond, th, el = t
        gated = bool(GATED_RE.search(re.sub(r"0x[0-9A-Fa-f]+", "", cond)))
        jt, je = value_json(th, ist, ctx, where + "?"), value_json(el, ist, ctx, where + ":")
        if jt is None and je is None:
            return None
        if jt is None or je is None:
            kept, lost = (jt, el) if je is None else (je, th)
            ctx.drop("gated-hH" if gated else "virtual-unresolved",
                     where, f"cond={cond!r} branch dropped: {lost!r}")
            return kept
        return {"case": {"if": cond_str(cond, ist, ctx), "then": jt, "else": je}}
    if v.startswith("C1|"):
        try:
            return {"tier": int(v[3:], 0)}
        except ValueError:
            ctx.errors.append(f"{where}: bad C1 ref {v!r}")
            return None
    m = re.fullmatch(r"H3\|(.+)", v)
    if not m:
        if re.fullmatch(r"0x[0-9A-Fa-f]+", v):
            return "=" + chr(int(v, 16))
        ctx.errors.append(f"{where}: unparsable value {v!r}")
        return None
    tok = m.group(1)
    if tok.startswith("0x"):
        n = int(tok, 16)
        probes = [("JONG", n >> 32), ("JUNG", n >> 16), ("CHO", n >> 16),
                  ("JONG", n), ("JUNG", n), ("CHO", n)]
        for unit, hi in probes:
            if hi and (unit, hi) in ist.vu:
                cp = ist.vu[(unit, hi)]
                if cp == 0:
                    return {"noop": True}      # VU maps to "0" = emit nothing
                if cp:
                    # 501-504 right-side vowels: plain codepoint + variant flag
                    # (chain-composition gate, docs/SPEC-P2.md; patim V/F test)
                    if unit == "JUNG" and hi in VIRTUAL:
                        return {"j": chr(cp), "variant": True}
                    return chr(cp)
                ctx.errors.append(f"{where}: H3|{tok} VU {unit}:{hi} target name unmapped")
                return None
        if jamo.is_jamo(n):
            return chr(n)
        ctx.errors.append(f"{where}: H3 numeric {tok} not a jamo or known virtual")
        return None
    neg = tok.startswith("_")
    nm = tok.strip("_")
    order = (JONG, CHO, JUNG) if neg else (CHO, JUNG, JONG)
    for t in order:
        if nm in t:
            return chr(t[nm])
    ctx.errors.append(f"{where}: mnemonic {tok!r} unmapped")
    return None


def cond_str(cond, ist, ctx):
    try:
        ast = parse_expr(cond)
    except ExprError as e:
        ctx.errors.append(f"cond {cond!r}: {e}")
        return cond
    ast = rewrite_ast(ast, ist, ctx, [True])
    return unparse(ast)


def convert(ist_path, name, out_dir, alias, extra_compose):
    ctx = Ctx()
    ist = IstFile(ist_path, ctx)
    if ist.table is None:
        ctx.errors.append(f"{ist_path.name}: missing KeyTable")
    table = ist.table
    flags = dict((k, int(v)) for k, v in
                 re.findall(r"([A-Za-z])\s*=\s*(\d+)", table.get("before", "") if table is not None else ""))
    keys = {}
    if table is not None:
        for k in table.findall("Key"):
            at_str = k.get("at")
            if at_str is None:
                continue
            base = jamo.key_of(int(at_str, 16))
            if not base:
                continue
            kid, mod = base
            raw = k.get("value")
            if raw is not None:
                v = value_json(raw, ist, ctx, f"{kid}+{mod}")
                if v is not None:
                    keys.setdefault(kid, {})[mod] = v
        gflag_seen = any("(G=" in (k.get("value") or "") for k in table.findall("Key"))
    else:
        gflag_seen = False
    compose_rows = []
    gen = ist.root.find("GeneratorSetting")
    um = gen.find("UnitMixTable") if gen is not None else None
    for it in (um.findall("UnitMix") if um is not None else []):
        u = it.get("unit")
        a = ist.compose(u, it.get("a"), ctx)
        b = ist.compose(u, it.get("b"), ctx)
        r = ist.compose(u, it.get("to"), ctx)
        if a and b and r:
            compose_rows.append((chr(a), chr(b), chr(r)))
        else:
            ctx.drop("unitmix-unresolved", f"{ist_path.name} UnitMix",
                     f"unit={u} a={it.get('a')!r} b={it.get('b')!r} "
                     f"to={it.get('to')!r}")
    seen, citems = set(), []
    for a, b, r in sorted(set(compose_rows)):
        if (a, b) in seen:
            continue
        seen.add((a, b))
        citems.append({"a": a, "b": b, "r": r})
    dump = lambda o: json.dumps(o, ensure_ascii=False, indent=2) + "\n"
    (out_dir / "layouts").mkdir(parents=True, exist_ok=True)
    (out_dir / "compose").mkdir(parents=True, exist_ok=True)
    layout = {
        "schemaVersion": 2, "id": name,
        "name": {"en": "Shinsebeol P2 (basic)" if "basic" in ist_path.name else
                 "Shinsebeol P2 + 기호 확장" if "symbols" in ist_path.name else
                 "Shinsebeol P2 옛한글 조합 + 기호 확장",
                 "ko": table.get("name", name) if table is not None else name},
        "engine": "h3", "aliases": [alias],
        # 신세벌식 composes Hangul, so it belongs to the 한글 half of the
        # two-mode IME (docs/SPEC-SCHEMA.md, maintainer 2026-10-11).
        "mode": "hangul",
        "source": f"pat.im ist: {ist_path.name} (normative; lifted by tools/ist2data.py)",
        "compose": ([f"{name}-mix"] if citems else []) + list(extra_compose),
        "keys": {k: keys[k] for k in sorted(keys)},
    }
    if gflag_seen:
        layout["jongonlyFresh"] = True
    if flags:
        layout["flags"] = flags
    total_drops = sum(len(v) for v in ctx.notes["dropped"].values())
    if total_drops or ctx.notes["inert-ids"]:
        layout["x-notes"] = {k: (v if not isinstance(v, set) else sorted(v))
                             for k, v in ctx.notes.items() if v}
    if ctx.errors:
        for e in ctx.errors:
            print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(f"ist2data: {len(ctx.errors)} hard errors — refusing to emit {name}")
    (out_dir / "layouts" / f"{name}.json").write_text(dump(layout), encoding="utf-8")
    if citems:
        (out_dir / "compose" / f"{name}-mix.json").write_text(dump(
            {"schemaVersion": 1, "id": f"{name}-mix",
             "source": f"UnitMixTable of {ist_path.name}", "items": citems}),
            encoding="utf-8")
    for reason, lst in ctx.notes["dropped"].items():
        print(f"note[{reason}]: {len(lst)} branches", file=sys.stderr)
    print(f"converted {name}: {len(keys)} keys, {len(citems)} mix rows, flags={flags}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--ist", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--name", required=True)
    ap.add_argument("--alias", default="3sin-p2")
    ap.add_argument("--compose-extra", default="")
    a = ap.parse_args()
    convert(a.ist, a.name, a.out, a.alias,
            [c for c in a.compose_extra.split(",") if c])
