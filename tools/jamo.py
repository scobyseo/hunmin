"""jamo.py — single source of Hangul-domain knowledge for hunmin tooling.

Binding rule: kbd2data/ist2data/doctor/ohi2compose import from here
(docs/CODE_RULES.md rule 2). Slot ranges mirror libhangul's predicates
(hangulctype.c) so generated data stays parity-compatible — including
libhangul's convention of treating U+A960-A97C as extended CHOSEONG.
"""
import re

# ---- slot ranges (libhangul hangul_is_choseong/jungseong/jongseong) ----
CHO_RANGES = ((0x1100, 0x115F), (0xA960, 0xA97C))
JUNG_RANGES = ((0x1161, 0x11A7), (0xD7B0, 0xD7C6))
JONG_RANGES = ((0x11A8, 0x11FF), (0xD7CB, 0xD7FB))
FILLERS = (0x115F, 0x1160)

def in_ranges(cp, ranges):
    return any(a <= cp <= b for a, b in ranges)

def slot(cp):
    if in_ranges(cp, CHO_RANGES): return "L"
    if in_ranges(cp, JUNG_RANGES): return "V"
    if in_ranges(cp, JONG_RANGES): return "T"
    return None

def is_jamo(cp):
    return slot(cp) is not None and cp not in FILLERS

# ---- US-QWERTY shift pairs (base -> shifted), shared by converters ----
US_SHIFT = {"`": "~", "1": "!", "2": "@", "3": "#", "4": "$", "5": "%",
            "6": "^", "7": "&", "8": "*", "9": "(", "0": ")", "-": "_",
            "=": "+", "[": "{", "]": "}", "\\": "|", ";": ":", "'": '"',
            ",": "<", ".": ">", "/": "?"}
US_UNSHIFT = {v: k for k, v in US_SHIFT.items()}

def key_of(codepoint):
    """ist/libhangul `at`/key code -> (base_id, mod) or None."""
    if not 0x20 <= codepoint <= 0x7E:
        return None
    ch = chr(codepoint)
    if "A" <= ch <= "Z":
        return ch.lower(), "shift"
    if ch in US_UNSHIFT:
        return US_UNSHIFT[ch], "shift"
    return ch, "plain"

# ---- modern cho index -> jong codepoint (libhangul choseong_to_jongseong, basic 19) ----
CHO2JONG = {0x1100: 0x11A8, 0x1101: 0x11A9, 0x1102: 0x11AB, 0x1103: 0x11AE,
            0x1105: 0x11AF, 0x1106: 0x11B7, 0x1107: 0x11B8, 0x1109: 0x11BA,
            0x110A: 0x11BB, 0x110B: 0x11BC, 0x110C: 0x11BD, 0x110E: 0x11BE,
            0x110F: 0x11BF, 0x1110: 0x11C0, 0x1111: 0x11C1, 0x1112: 0x11C2,
            0x1104: 0, 0x1108: 0, 0x110D: 0, }

# ---- Naraeseot unit-ID convention (docs/SPEC-P2.md): id = codepoint - 0x1081 ----
UNIT_ID_BASE = 0x1081

# Slot-dependent exceptions where the BASE convention does not land in the
# slot's ranges: unit IDs are letter-shared (one id per jamo letter), but each
# slot compares against its own codepoint. Only ids verified against the ist
# rows that use them are enabled; every other id stays as written (always
# false at runtime) and is recorded as inert-ids by ist2data (fail-fast).
#   D: 0x8A = ㅇ (z+ㅇ symbol-tier guard, sin3-p2{,-yethangul})
#   F: 2024 윗글 받침 ㄸ/ㅃ/ㅉ — shift-G `F==24` (G's jong=ㄷ), shift-E `F==86`
#      (E's jong=ㅂ), shift-V `F==161` (V's jong=ㅈ)
UNIT_ID_CHO = {0x8A: UNIT_ID_BASE + 0x8A}          # ㅇ as choseong
UNIT_ID_JONG = {24: 0x11AE, 86: 0x11B8, 161: 0x11BD}  # ㄷ, ㅂ, ㅈ as jongseong

# ---- expression mini-language (mirror of libhunmin/src/expr.{h,c}) ----
# or := and ('||' and)* | and := unary ('&&' unary)* | unary := '!' unary | cmp
# cmp := atom [cmpop atom] | atom := IDENT | NUMBER | '(' or ')'
_IDENT = r"[A-Za-z]"
_NUM = r"0[xX][0-9A-Fa-f]+|\d+"
_TOKRE = re.compile(rf"(?P<tok>{_NUM}|{_IDENT}|\(|\)|==|!=|>=|<=|&&|\|\||[!<>])")

class ExprError(Exception):
    pass

def tokenize(expr):
    toks, i = [], 0
    while i < len(expr):
        if expr[i].isspace():
            i += 1; continue
        m = _TOKRE.match(expr, i)
        if not m:
            raise ExprError(f"bad char {expr[i]!r} at {i}")
        toks.append(m.group("tok"))
        i = m.end()
    return toks

def parse_expr(expr):
    """Returns nested tuples: ('id',c) ('num',v) ('not',a) ('and',a,b) ... """
    toks = tokenize(expr)
    pos = 0
    def peek(): return toks[pos] if pos < len(toks) else None
    def take(): 
        nonlocal pos
        t = toks[pos]; pos += 1; return t
    def atom():
        t = take()
        if t == "(":
            e = or_()
            if peek() != ")": raise ExprError("expected )")
            take()
            return e
        if re.fullmatch(_IDENT, t): return ("id", t)
        if re.fullmatch(_NUM, t): return ("num", int(t, 0))
        raise ExprError(f"unexpected {t!r}")
    def cmp_():
        a = atom()
        if peek() in ("==", "!=", ">=", "<=", "<", ">"):
            op = take()
            return (op, a, atom())
        return a
    def unary():
        if peek() == "!":
            take()
            return ("!", unary())
        return cmp_()
    def and_():
        a = unary()
        while peek() == "&&":
            take()
            a = ("&&", a, unary())
        return a
    def or_():
        a = and_()
        while peek() == "||":
            take()
            a = ("||", a, or_())
        return a
    e = or_()
    if pos != len(toks):
        raise ExprError(f"trailing {toks[pos:]}")
    return e

PR = {"||": 1, "&&": 2, "!": 3, "==": 4, "!=": 4, ">=": 4, "<=": 4,
      "<": 4, ">": 4}

def unparse(ast):
    return _u(ast, 0)

def _u(a, minp):
    k = a[0]
    if k == "id":
        return a[1]
    if k == "num":
        return f"0x{a[1]:X}" if a[1] > 0xFF else str(a[1])
    if k == "!":
        s = f"!{_u(a[1], PR['!'])}"
    elif k in ("&&", "||"):
        s = f"{_u(a[1], PR[k])}{k}{_u(a[2], PR[k] + 1)}"
    else:
        s = f"{_u(a[1], PR[k])} {k} {_u(a[2], PR[k])}"
    return f"({s})" if PR.get(k, 9) < minp else s

def walk_num_comparisons(ast):
    """yield (parent_index_placeholder, ident, op, num) for id OP num nodes."""
    if ast[0] in ("==", "!=", ">=", "<=", "<", ">") and ast[2][0] == "num":
        if ast[1][0] == "id":
            yield ast[1][1], ast[0], ast[2][1]
    for child in ast[1:]:
        if isinstance(child, tuple):
            yield from walk_num_comparisons(child)

def map_nums(ast, fn):
    """Return new AST applying fn(ident, op, num)->num|None to id-OP-num nodes."""
    k = ast[0]
    if k in ("==", "!=", ">=", "<=", "<", ">") and ast[1][0] == "id" and ast[2][0] == "num":
        new = fn(ast[1][1], k, ast[2][1])
        if new is None:
            return ast
        return (k, ast[1], ("num", new))
    return (k,) + tuple(map_nums(c, fn) if isinstance(c, tuple) else c for c in ast[1:])

def or_nodes(*asts):
    r = asts[0]
    for a in asts[1:]:
        r = ("||", r, a)
    return r
