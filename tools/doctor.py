#!/usr/bin/env python3
"""hunmin-doctor v0: schema + semantic validation of libhunmin/data.

Exit != 0 on any error. Warnings are advisory. Contract: docs/SPEC-SCHEMA.md.
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import jamo

ID_RE = re.compile(r"^[a-z0-9][a-z0-9.-]*$")
MODS = {"plain", "shift", "ctrl", "alt"}
ENGINES = {"std", "h3", "romaja"}
# Which half of the two-mode IME a layout belongs to. The engine mirrors these
# two strings (hm_layout_mode / hunmin_ctx_layout_mode); a third value here
# would be one the engine has never heard of.
LAYOUT_MODES = {"hangul", "latin"}
REQUIRED_TOP = {"schemaVersion", "id", "name", "engine", "compose", "keys"}


def jamo_slot(ch):
    cp = ord(ch)
    if 0x1100 <= cp <= 0x115E:
        return "L"
    if (0x1161 <= cp <= 0x11A7 or 0xA960 <= cp <= 0xA97C
            or 0xD7B0 <= cp <= 0xD7C6):
        return "V"
    if 0x11A8 <= cp <= 0x11FF or 0xD7CB <= cp <= 0xD7FB:
        return "T"
    return None


class V:
    def __init__(self, data):
        self.errors, self.warnings, self.data = [], [], data

    def err(self, msg):
        self.errors.append(f"{self.data}: {msg}")

    def j(self, x, where):
        if len(x) != 1 or jamo_slot(x) is None:
            self.err(f"{where}: {x!r} is not a single non-filler jamo")
            return None
        return x


def check_value(v, where, vfile):
    if v is None:
        return
    if isinstance(v, str):
        if v.startswith("="):
            if len(v) < 2:
                vfile.err(f"{where}: empty literal")
            return
        vfile.j(v, where)
        return
    if isinstance(v, dict):
        if "seq" in v:
            if not isinstance(v["seq"], list) or not v["seq"]:
                vfile.err(f"{where}: seq must be non-empty array")
            else:
                for i, item in enumerate(v["seq"]):
                    if not isinstance(item, str) or not item:
                        vfile.err(f"{where}: seq[{i}] must be non-empty string")
            return
        if "tier" in v or "noop" in v:
            return
        if "g" in v:
            if set(v) != {"g"}:
                vfile.err(f"{where}: g marker must wrap exactly one value")
            else:
                check_value(v["g"], where + ".g", vfile)
            return
        if "case" in v:
            c = v["case"]
            if not isinstance(c, dict) or set(c) != {"if", "then", "else"}:
                vfile.err(f"{where}: case needs if/then/else"); return
            expr = c["if"]
            if not isinstance(expr, str):
                vfile.err(f"{where}: case.if must be string"); return
            try:
                ast = jamo.parse_expr(expr)
                bad = {a[1] for a in _ids(ast)} - set(
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")
                if bad:
                    vfile.err(f"{where}: case.if bad token {bad}")
            except jamo.ExprError as e:
                vfile.err(f"{where}: case.if parse error: {e}")
            check_value(c["then"], where + ".then", vfile)
            check_value(c["else"], where + ".else", vfile)
            return
        if "j" in v:
            vfile.j(v["j"], where)
            if not isinstance(v.get("compose", True), bool):
                vfile.err(f"{where}: compose must be bool")
            extra = set(v) - {"j", "compose", "variant"}
            if extra:
                vfile.err(f"{where}: unknown value keys {extra}")
            return
        if "amb" in v:
            amb = v["amb"]
            if not (isinstance(amb, list) and len(amb) == 2):
                vfile.err(f"{where}: amb must be 2 jamo")
                return
            s0, s1 = (vfile.j(a, where) for a in amb)
            if s0 and s1:
                if s0 == s1:
                    vfile.err(f"{where}: amb parts share slot {s0}")
                if v.get("first") not in amb:
                    vfile.err(f"{where}: first must be one of amb parts")
            if set(v) - {"amb", "first"}:
                vfile.err(f"{where}: unknown value keys")
            return
        vfile.err(f"{where}: unrecognised value object {sorted(v)}")
        return
    vfile.err(f"{where}: value must be string/object/null")


def check_layout(path: Path, aliases: dict, compose_ids: set, vfile: V):
    try:
        d = json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.data = path.name
        vfile.err(f"invalid JSON: {e}")
        return
    vfile.data = path.name
    missing = REQUIRED_TOP - set(d)
    if missing:
        vfile.err(f"missing fields {sorted(missing)}")
        return
    if d["schemaVersion"] not in (1, 2):
        vfile.err(f"unsupported schemaVersion {d['schemaVersion']}")
    if not ID_RE.match(d["id"]):
        vfile.err(f"bad id {d['id']!r}")
    if path.stem != d["id"]:
        vfile.err(f"filename {path.name} != id {d['id']}")
    for a in d.get("aliases", []):
        if a in aliases:
            vfile.err(f"alias {a!r} collides with {aliases[a]}")
        aliases[a] = d["id"]
    if d["engine"] not in ENGINES:
        vfile.err(f"bad engine {d['engine']!r}")
    # `mode` decides which half of the two-mode settings list a layout appears
    # in (issue #12). The ENGINE defaults an absent or misspelt value to
    # "hangul" so a typo costs one mis-filed row rather than the layout; this
    # is what makes the typo loud instead. Required, because "which list does
    # this belong in" has exactly one right answer and it is the file's.
    if "mode" not in d:
        vfile.err("missing 'mode' (\"hangul\" | \"latin\")")
    elif d["mode"] not in LAYOUT_MODES:
        vfile.err(f"bad mode {d['mode']!r}; expected one of {sorted(LAYOUT_MODES)}")
    for c in d["compose"]:
        if c not in compose_ids:
            vfile.err(f"compose {c!r} not found in data/compose")
    if "ko" not in d.get("name", {}) or "en" not in d.get("name", {}):
        vfile.warnings.append("name should carry en and ko")
    if not isinstance(d["keys"], dict) or not d["keys"]:
        vfile.err("keys empty or wrong type")
        return
    for k, mods in d["keys"].items():
        if k == "\n":
            krepr = "enter"
        else:
            krepr = k
        if not (len(k) == 1 or k in {"space", "enter", "backspace"}):
            vfile.err(f"key {krepr!r}: not a char id or named special")
        if not isinstance(mods, dict) or not (set(mods) <= MODS) or not mods:
            vfile.err(f"key {krepr!r}: mods must be non-empty subset of {sorted(MODS)}")
            continue
        for mod, val in mods.items():
            check_value(val, f"key {krepr!r}.{mod}", vfile)


def check_compose(path: Path, vfile: V, ids: set):
    try:
        d = json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.data = path.name
        vfile.err(f"invalid JSON: {e}")
        return
    vfile.data = path.name
    ids.add(d.get("id"))
    if d.get("schemaVersion") not in (1, 2):
        vfile.err("bad schemaVersion")
    seen = set()
    for it in d.get("items", []):
        if set(it) != {"a", "b", "r"}:
            vfile.err(f"bad item {it}")
            continue
        for f in "abr":
            vfile.j(it[f], f"item {f}")
        key = (it["a"], it["b"])
        if key in seen:
            vfile.err(f"duplicate pair {key}")
        seen.add(key)


def _ids(ast):
    if ast[0] == "id":
        yield ast
    for c in ast[1:]:
        if isinstance(c, tuple):
            yield from _ids(c)


def check_hanja(path: Path, vfile: V):
    try:
        d = json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.data = path.name
        vfile.err(f"invalid JSON: {e}")
        return
    vfile.data = path.name
    if d.get("schema") != "hunmin.hanja/2":
        vfile.err(f"bad schema {d.get('schema')!r}")
    if d.get("version") != 2:
        vfile.err(f"bad version {d.get('version')!r}")
    entries = d.get("entries")
    if not isinstance(entries, dict):
        vfile.err("entries must be dict")
        return
    seen = set()
    for reading, hanja_list in entries.items():
        if reading in seen:
            vfile.err(f"duplicate reading {reading!r}")
        seen.add(reading)
        if not isinstance(hanja_list, list):
            vfile.err(f"reading {reading!r}: value must be list")
            continue
        for i, h in enumerate(hanja_list):
            # v2: each candidate is {"v": <hanja>, "ko"/"en": <gloss>,
            # optional "jp"/"cn": true}
            if not isinstance(h, dict) or not isinstance(h.get("v"), str):
                vfile.err(f"reading {reading!r}[{i}]: must be an object with str 'v'")
                continue
            hv = h["v"]
            if len(hv) != 1:
                vfile.err(f"reading {reading!r}[{i}]: 'v' must be single char")
            for gloss_key in ("ko", "en"):
                if gloss_key in h and not isinstance(h[gloss_key], str):
                    vfile.err(f"reading {reading!r}[{i}]: '{gloss_key}' must be str")
            for tag_key in ("jp", "cn"):
                if tag_key in h and h[tag_key] is not True:
                    vfile.err(f"reading {reading!r}[{i}]: '{tag_key}' must be true")
            cp = ord(hv) if len(hv) == 1 else None
            if cp is not None and not (0x3400 <= cp <= 0x4DBF or 0x4E00 <= cp <= 0x9FFF or 0xF900 <= cp <= 0xFAFF or 0x20000 <= cp <= 0x2A6DF or 0x2A700 <= cp <= 0x2B73F or 0x2B740 <= cp <= 0x2B81F or 0x2B820 <= cp <= 0x2CEAF):
                vfile.warnings.append(f"reading {reading!r}[{i}]: {hv!r} not in CJK Unified Ideographs blocks")


def check_idioms(path: Path, vfile: V):
    try:
        d = json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.data = path.name
        vfile.err(f"invalid JSON: {e}")
        return
    vfile.data = path.name
    if d.get("schema") != "hunmin.idioms/1":
        vfile.err(f"bad schema {d.get('schema')!r}")
    if d.get("version") != 1:
        vfile.err(f"bad version {d.get('version')!r}")
    entries = d.get("entries")
    if not isinstance(entries, list):
        vfile.err("entries must be list")
        return
    seen_hanja = set()
    seen_reading = set()
    for i, e in enumerate(entries):
        if not isinstance(e, dict):
            vfile.err(f"entry[{i}]: must be object")
            continue
        required = {"hanja", "reading", "meaning"}
        missing = required - set(e)
        if missing:
            vfile.err(f"entry[{i}]: missing fields {sorted(missing)}")
            continue
        hanja = e["hanja"]
        reading = e["reading"]
        meaning = e["meaning"]
        if not isinstance(hanja, str) or len(hanja) != 4:
            vfile.err(f"entry[{i}]: hanja must be 4 chars")
        if not isinstance(reading, str) or not reading:
            vfile.err(f"entry[{i}]: reading must be non-empty string")
        if not isinstance(meaning, str) or not meaning:
            vfile.err(f"entry[{i}]: meaning must be non-empty string")
        if hanja in seen_hanja:
            vfile.warnings.append(f"entry[{i}]: duplicate hanja {hanja!r} (multiple readings)")
        seen_hanja.add(hanja)
        if reading in seen_reading:
            vfile.warnings.append(f"entry[{i}]: duplicate reading {reading!r}")
        seen_reading.add(reading)


def check_krdict(path: Path, vfile: V):
    try:
        d = json.loads(path.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.data = path.name
        vfile.err(f"invalid JSON: {e}")
        return
    vfile.data = path.name
    if d.get("schema") != "hunmin.krdict/1":
        vfile.err(f"bad schema {d.get('schema')!r}")
    if d.get("version") != 1:
        vfile.err(f"bad version {d.get('version')!r}")
    entries = d.get("entries")
    if not isinstance(entries, list):
        vfile.err("entries must be list")
        return
    seen = set()
    for i, e in enumerate(entries):
        if not isinstance(e, dict):
            vfile.err(f"entry[{i}]: must be object")
            continue
        required = {"ko", "hanja", "meanings", "pos"}
        missing = required - set(e)
        if missing:
            vfile.err(f"entry[{i}]: missing fields {sorted(missing)}")
            continue
        ko = e["ko"]
        hanja = e["hanja"]
        meanings = e["meanings"]
        pos = e["pos"]
        if not isinstance(ko, str) or not ko:
            vfile.err(f"entry[{i}]: ko must be non-empty string")
        if not isinstance(hanja, str) or not hanja:
            vfile.err(f"entry[{i}]: hanja must be non-empty string")
        if not isinstance(meanings, list) or not meanings:
            vfile.err(f"entry[{i}]: meanings must be non-empty list")
        elif not all(isinstance(m, str) and m for m in meanings):
            vfile.err(f"entry[{i}]: all meanings must be non-empty strings")
        if not isinstance(pos, str) or not pos:
            vfile.err(f"entry[{i}]: pos must be non-empty string")
        if "level" in e and (not isinstance(e["level"], str) or not e["level"]):
            vfile.err(f"entry[{i}]: level must be non-empty string if present")
        if "category" in e and (not isinstance(e["category"], str) or not e["category"]):
            vfile.err(f"entry[{i}]: category must be non-empty string if present")
        # Duplicate ko is EXPECTED and load-bearing: krdict re-uses the same
        # headword for a different hanja (동음이의어) or a different pos
        # (품사 중복 등재, e.g. 가가호호 명사/부사), and upstream order is
        # reproduced rather than de-duplicated. 873 of the 2,461 repeated ko
        # values repeat the SAME hanja with a different pos. The only case
        # that is a defect is a ko+hanja+pos triple that appears twice --
        # same word, same spelling, same part of speech, twice.
        trip = (ko, hanja, pos)
        if trip in seen:
            vfile.warnings.append(
                f"entry[{i}]: duplicate ko/hanja/pos {ko!r} {hanja!r} {pos!r} "
                f"-- same word, spelling and part of speech twice"
            )
        seen.add(trip)


# ------------------------------------------------------------------ modes/
# docs/SPEC-SCHEMA.md "English word list": a JSON array of lowercase words,
# sorted, `[a-z]` with word-internal `'` or `-`. The lookup POLICY is stated in
# that same section -- the loader folds ASCII A-Z before comparing and never
# strips an apostrophe or a hyphen -- and this check is what keeps the FILE
# honest about it, because a word the lookup would still match (uppercase,
# duplicated, out of order) passes every behaviour test and only the schema
# check can notice.
#
# The declared shape and this pattern are one rule read twice, deliberately:
# tools/en-us2data.py writes both, and check_modes fails when they disagree.
MODES_WORD_RE = re.compile(r"^[a-z]+(?:['-][a-z]+)*$")
MODES_META_TYPES = {
    "what": str,
    "generator": str,
    "source": dict,
    "method": dict,
    "counts": dict,
    "licence": dict,
    "does_not_represent": list,
}
MODES_SOURCE_KEYS = {"name", "dic", "dic_sha256", "aff", "aff_sha256",
                     "declared_version", "upstream"}
MODES_METHOD_KEYS = {"expansion", "lowercased", "word_shape"}
MODES_COUNTS_KEYS = {"stems", "expanded_forms", "shipped", "dropped"}


def check_modes(directory: Path, vfile: V):
    """data/modes/: the english word list behind ReqMode8's english_valid."""
    vfile.data = "en-US.json"
    listing = directory / "en-US.json"
    try:
        root = json.loads(listing.read_text(encoding="utf-8"))
    except Exception as e:
        vfile.err(f"en-US.json: invalid JSON: {e}")
        return
    if isinstance(root, dict) and isinstance(root.get("words"), list):
        root = root["words"]
    if not isinstance(root, list):
        vfile.err('en-US.json: root must be an array of words (or an object with "words")')
        return
    prev = None
    for i, w in enumerate(root):
        if not isinstance(w, str) or not w:
            vfile.err(f"word[{i}]: must be a non-empty string")
            prev = None
            continue
        if not MODES_WORD_RE.match(w):
            vfile.err(f"word[{i}]: {w!r} is outside the schema's word shape "
                      f"{MODES_WORD_RE.pattern}")
        if prev is not None and w <= prev:
            vfile.err(f"word[{i}]: {w!r} does not follow {prev!r} -- the list is "
                      f"the lookup's sort contract, so it must be sorted and unique")
        prev = w

    vfile.data = "meta.json"
    try:
        meta = json.loads((directory / "meta.json").read_text(encoding="utf-8"))
    except Exception as e:
        vfile.err(f"missing or invalid: {e}")
        return
    if not isinstance(meta, dict):
        vfile.err("must be an object")
        return
    for key, kind in MODES_META_TYPES.items():
        if key not in meta:
            vfile.err(f"missing {key!r}")
        elif not isinstance(meta[key], kind):
            vfile.err(f"{key!r} must be {kind.__name__}")
    for group, keys in (("source", MODES_SOURCE_KEYS),
                        ("method", MODES_METHOD_KEYS),
                        ("counts", MODES_COUNTS_KEYS)):
        obj = meta.get(group)
        if not isinstance(obj, dict):
            continue
        for key in sorted(keys - set(obj)):
            vfile.err(f"missing {group}.{key!r}")
    method_obj = meta.get("method")
    method = method_obj if isinstance(method_obj, dict) else {}
    if method.get("lowercased") is not True:
        vfile.err("method.lowercased must be true -- the artefact is "
                  "lowercase and the lookup folds ASCII to it")
    shape = method.get("word_shape")
    if isinstance(shape, str) and shape != MODES_WORD_RE.pattern:
        vfile.err(f"method.word_shape {shape!r} is not the shape this "
                  f"check enforces ({MODES_WORD_RE.pattern!r})")
    counts_obj = meta.get("counts")
    counts = counts_obj if isinstance(counts_obj, dict) else {}
    shipped = counts.get("shipped")
    if isinstance(shipped, int) and shipped != len(root):
        vfile.err(f"counts.shipped {shipped} but en-US.json holds {len(root)} words")
    if not (directory / "LICENSE.md").is_file():
        vfile.data = "LICENSE.md"
        vfile.err("missing -- the SCOWL notice is the attribution the licence "
                  "requires beside any redistribution")

def main():
    data = Path(sys.argv[1] if len(sys.argv) > 1 else "libhunmin/data")
    vfile = V(data)
    compose_ids = set()
    for p in sorted((data / "compose").glob("*.json")):
        check_compose(p, vfile, compose_ids)
    aliases = {}
    for p in sorted((data / "layouts").glob("*.json")):
        check_layout(p, aliases, compose_ids, vfile)
    for p in sorted((data / "hanja").glob("*.json")):
        check_hanja(p, vfile)
    for p in sorted((data / "idioms").glob("*.json")):
        check_idioms(p, vfile)
    for p in sorted((data / "krdict").glob("*.json")):
        check_krdict(p, vfile)
    modes = data / "modes"
    if (modes / "en-US.json").is_file():
        check_modes(modes, vfile)
    else:
        # B3 (#190) lets the dictionary ship as its own package, so an absent
        # word list is a warning and not a red gate; what must not happen is a
        # silent skip, which is why the summary line counts it.
        vfile.warnings.append("modes/: no english word list in this tree "
                              "(B3 allows it to ship separately); nothing linted")
    for w in vfile.warnings:
        print(f"warn: {w}", file=sys.stderr)
    for e in vfile.errors:
        print(f"error: {e}", file=sys.stderr)
    print(f"hunmin-doctor: {len(vfile.errors)} errors, {len(vfile.warnings)} warnings")
    sys.exit(1 if vfile.errors else 0)


if __name__ == "__main__":
    main()
