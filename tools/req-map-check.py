#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Check that docs/REQ-MAP.md is a complete, living pointer table.

WHY THIS EXISTS
---------------
docs/REQ-MAP.md is a pointer layer, not a copy: one row per requirement, and
every cell is a `path#anchor` into a document or a symbol in a source file.
Its purpose is to let a fact live in exactly one place, so the duplication the
maintainer wants deleted has somewhere to point. That only works if the
pointers resolve. A map with a dead pointer is worse than no map: it is a
citation that looks authoritative and sends the reader nowhere.

So this check is deliberately narrow and mechanical, and it fails closed:

  1. every `path` in a pointer exists;
  2. every `path#anchor` resolves -- `§<key>` to a heading with that section
     key, a bare identifier to text present in the file;
  3. every ctest id in `verified by` is a real test (see "what registered
     means" below);
  4. every requirement id that a SPEC file's own requirement section carries
     has a row;
  5. every row has all seven columns.

WHAT REGISTERED MEANS
---------------------
A ctest id counts as registered when the given build directory lists it in
`ctest -N`, OR when some source CMakeLists.txt or cmake/*.cmake file declares
it with `add_test(NAME <id>` or its H2 helper spelling
`hunmin_add_test(NAME <id>` (cmake/HunminTestHelpers.cmake, issue #271).
The disjunction is deliberate and measured: the fcitx5
(D1/D2), Android (E1) and trace-build tiers register **only where their SDKs
are present**, so a pure `ctest -N` test would red on a host without fcitx5 for
a row that is true -- the false-positive class docs-lint.py's header says gets
a guard switched off (`docs-lint.py:25-27`). `tools/docs-lint.py` check 6 reads
the source CMakeLists for the same reason (`docs-lint.py:470-487`). The line
still says which of the two answered.

A missing build directory skips ONLY the ctest half, loudly, and still runs
every other check (requirement 2 of the brief).

Run:  tools/req-map-check.py [--map PATH] [--build-dir DIR]
      --map        the map to check (default docs/REQ-MAP.md)
      --build-dir  a configured build tree (default build/)

Exit: 0 all rows ok; 1 a row failed; 2 the map or the SPEC files could not be
read (a check that cannot read what it guards is not a check).
"""
import argparse
import glob
import os
import re
import subprocess
import sys

# The map quotes requirement prose, which contains em dashes; a Windows
# console default (cp949) raises UnicodeEncodeError on the first such byte.
for _stream in (sys.stdout, sys.stderr):
    _reconfigure = getattr(_stream, "reconfigure", None)
    if _reconfigure:
        _reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_MAP = "docs/REQ-MAP.md"

COLUMNS = ("REQ", "requirement", "architecture", "design", "implementation",
           "verified by", "status", "parent")

# `ReqMode8`, `ReqMode7`, `4-7`, `4-3b`, `1.4` -- the section keys this repo's
# requirement-bearing documents actually use. Kept as one regex because both
# the SPEC heading scan and the `§` anchors are read through it.
KEY = re.compile(r"^([A-Za-z]*\d+(?:[.\-][0-9]+[a-z]?)*(?:-[^\s.]+)?)\.?")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
SYMBOL = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)*$")

rows_total = 0
rows_failed = 0
failures = 0
accepted: dict[str, str] = {}
LABEL_SEP = " — "  # the requirement cell is `pointer — label`; only the pointer is read


def fail(req: str, reason: str) -> None:
    global failures
    failures += 1
    print(f"FAIL: {req} -- {reason}")


def norm(text: str) -> str:
    """Lowercase, drop `§`, keep alphanumerics (Hangul included), single-space."""
    return " ".join("".join(c if c.isalnum() else " " for c in text.lower()).split())


# --------------------------------------------------------------------------
# Pointers
# --------------------------------------------------------------------------
def split_row(line: str) -> list[str]:
    return [c.strip().strip("`") for c in line.strip().strip("|").split("|")]


def headings(text: str) -> list[tuple[str, str]]:
    """(section key, normalised heading text) for every heading with a key."""
    out = []
    for line in text.splitlines():
        m = HEADING.match(line)
        if not m:
            continue
        body = re.sub(r"\[[DE]\]\s*$", "", m.group(2)).strip()
        key = KEY.match(body)
        out.append((key.group(1) if key else "", norm(body)))
    return out


def check_pointer(req: str, cell: str, column: str) -> bool:
    """Validate one `path` or `path#anchor`. Returns True when it resolved."""
    token = cell.strip().strip("`").strip()
    if not token:
        return True
    path, _, anchor = token.partition("#")
    full = os.path.normpath(os.path.join(ROOT, path))
    if not os.path.exists(full):
        fail(req, f"{column}: `{path}` does not exist (from `{token}`)")
        return False
    if not anchor:
        return True
    if not os.path.isfile(full):
        fail(req, f"{column}: `{path}` is a directory, so `#{anchor}` cannot anchor in it")
        return False
    try:
        text = open(full, encoding="utf-8", errors="replace").read()
    except OSError as exc:
        fail(req, f"{column}: `{path}` could not be read ({exc})")
        return False

    anchor = anchor.strip().strip("`")
    heads = headings(text)
    if anchor.startswith("§"):
        body = anchor.lstrip("§").strip()
        key = KEY.match(body)
        if key:
            want = key.group(1)
            if any(h[0] == want for h in heads):
                return True
            fail(req, f"{column}: `{path}#§{want}` -- no heading with section key "
                      f"`{want}` in that file (anchors are section keys, never line numbers)")
            return False
        # Title-only anchor: a prefix of some heading.
        want = norm(body)
        if want and any(h[1].startswith(want) for h in heads):
            return True
        fail(req, f"{column}: `{path}#{anchor}` -- no heading in that file starts "
                  f"with `{body}`")
        return False

    if SYMBOL.match(anchor):
        if anchor in text:
            return True
        fail(req, f"{column}: `{path}#{anchor}` -- the symbol is not in that file")
        return False

    want = norm(anchor)  # a heading title, as the document writes it
    if want and any(h[1].startswith(want) for h in heads):
        return True
    fail(req, f"{column}: `{path}#{anchor}` -- neither a symbol in the file nor the "
              f"start of a heading in it")
    return False


def check_cell(req: str, cell: str, column: str) -> bool:
    """`none` is a finding; `; ` joins several pointers; each must resolve."""
    if cell.strip().lower() in ("none", ""):
        return True
    good = True
    for part in cell.split(";"):
        if part.strip():
            good = check_pointer(req, part, column) and good
    return good


# --------------------------------------------------------------------------
# ctest ids
# --------------------------------------------------------------------------
def build_ids(build_dir: str) -> set[str] | None:
    ctf = os.path.join(build_dir, "CTestTestfile.cmake")
    if not os.path.exists(ctf):
        return None
    r = subprocess.run(["ctest", "--test-dir", build_dir, "-N"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return set(re.findall(r"^\s*Test\s+#\d+:\s+(\S+)\s*$", r.stdout, re.M))


def source_registration(test_id: str) -> str:
    """The repo-relative CMakeLists/cmake file declaring the test, or ""."""
    cmake_files = sorted(glob.glob(os.path.join(ROOT, "**", "CMakeLists.txt"),
                                   recursive=True)
                         + glob.glob(os.path.join(ROOT, "cmake", "*.cmake")))
    for cmake in cmake_files:
        rel = os.path.relpath(cmake, ROOT).replace(os.sep, "/")
        if rel.startswith("build") or "/build" in rel or "/.git" in rel:
            continue
        try:
            text = open(cmake, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        if re.search(rf"(?:add_test|hunmin_add_test)\(\s*NAME\s+{re.escape(test_id)}\b", text):
            return rel
        # A test registered from a LOOP has no literal `add_test(NAME <id>`, so
        # the search above cannot see it and the row reads dead on any build
        # where that tier does not register. Measured 2026-10-29 on a Linux
        # build: `add_test(NAME tsf-mock-${name}` (frontends/tsf/test) made
        # FE1/FE4/FE10/TSF9/A2 fail with "neither registered in the build nor
        # declared by any add_test(NAME ...)" -- five true rows reported dead.
        # A loop's literal prefix is the declaration, so match that too; an id
        # that is not a prefix-extension of any registration still fails. Since
        # the #271 split, the loop registrations live in cmake/*.cmake and go
        # through hunmin_add_test, so the prefix scan must cover that file set
        # and that spelling.
        for prefix in re.findall(r"(?:add_test|hunmin_add_test)\(\s*NAME\s+([A-Za-z0-9_.\-]+)\$\{", text):
            if test_id.startswith(prefix):
                return rel
    return ""


def check_verified(req: str, cell: str, ids: set[str] | None) -> bool:
    cell = cell.strip()
    if cell.lower() in ("none", ""):
        return True
    good = True
    for part in (p.strip().strip("`") for p in cell.split(";")):
        if not part:
            continue
        if ids is None:
            continue  # the ctest half is skipped; said once in main()
        if part in ids:
            continue
        src = source_registration(part)
        if src:
            if part not in accepted:
                accepted[part] = src
                print(f"note: `{part}` is declared by {src} and is not registered in "
                      f"this build dir -- accepted (SDK/option-dependent tier)")
            continue
        fail(req, f"verified by: `{part}` is neither registered in the build nor "
                  f"declared by any add_test(NAME ...) in a source CMakeLists")
        good = False
    return good


# --------------------------------------------------------------------------
# The map
# --------------------------------------------------------------------------
def map_rows(path: str) -> tuple[list[list[str]], list[str]]:
    rows: list[list[str]] = []
    problems: list[str] = []
    header_seen = False
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            s = line.strip()
            if not s.startswith("|"):
                if rows:
                    break  # the requirement table is the first table in the file
                continue
            cells = split_row(s)
            if all(c.startswith("---") or set(c) <= set("-: ") for c in cells if c):
                continue  # the markdown rule under the header
            if (len(cells) == len(COLUMNS)
                    and all(cells[i].lower().startswith(COLUMNS[i].lower())
                            for i in range(len(COLUMNS)))):
                header_seen = True
                continue
            if not header_seen:
                problems.append(f"{path}:{n}: a table row appears before the header "
                                f"row ({' | '.join(COLUMNS)}).")
                continue
            rows.append(cells + [f"{path}:{n}"])
    return rows, problems



def check_parents(rows: list[list[str]]) -> None:
    """The requirement tree is only closed if the map says so (issue #136 ruling,
    2026-10-25): every requirement id names one of A1-A13, and every A has at
    least one child. Before this the tree was closed by a prose child index in
    SPEC-OVERVIEW, and nothing failed when a new id arrived without a parent."""
    known = {f"A{i}" for i in range(1, 14)}
    children = {a: 0 for a in known}
    for cells in rows:
        cols = dict(zip(COLUMNS, cells[:-1]))
        rid = cols["REQ"].strip()
        parent = cols.get("parent", "").strip().strip("`")
        if rid in known:
            if parent not in ("-", "none", ""):
                fail(rid, f"parent: an abstract requirement cannot have a parent (`{parent}`)")
            continue
        if parent not in known:
            fail(rid, f"parent: `{parent or '(empty)'}` is not one of A1-A13 -- a "
                      f"requirement with no abstract parent is a requirement nobody rooted")
            continue
        children[parent] += 1
    childless = sorted(a for a, n in children.items() if n == 0)
    if childless:
        fail("tree", f"parent: {', '.join(childless)} name(s) no child requirement -- an "
                     f"abstract requirement that nothing satisfies is the finding that file "
                     f"exists to expose")



def spec_requirement_ids() -> dict[str, list[str]]:
    """{spec file: [ids]} from each SPEC's own requirement section.

    The section is chosen by the document's structure, not a hard-coded letter:
    ids are collected from headings under a top-level heading whose text
    contains "requirement", the scan stops at the next top-level heading, and
    the document's title (its first heading) is never a section.

    That last rule is not decoration. SPEC-P2's title reads "... engine
    requirements ..." and its body carries no requirement ids, so trusting the
    title made P1/P2 look like requirements. SPEC-MODES §1 ("Settled
    requirements (DECIDED)") is the only requirement section in the tree; its
    §2 says of itself that each item "is an implementation parameter, not a
    requirement", so E1..E8 are not collected.
    """
    found: dict[str, list[str]] = {}
    for spec in sorted(glob.glob(os.path.join(ROOT, "docs", "SPEC-*.md"))):
        rel = os.path.relpath(spec, ROOT).replace(os.sep, "/")
        inside = False
        seen_title = False
        ids: list[str] = []
        for line in open(spec, encoding="utf-8"):
            m = HEADING.match(line.rstrip("\n"))
            if not m:
                continue
            level, body = len(m.group(1)), m.group(2)
            if level == 1:
                is_title = not seen_title
                seen_title = True
                inside = (not is_title) and "requirement" in body.lower()
                continue
            if not inside:
                continue
            key = KEY.match(body)
            if key and key.group(1) not in ids:
                ids.append(key.group(1))
        found[rel] = ids
    return found


def main() -> int:
    global rows_total, rows_failed
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=DEFAULT_MAP)
    ap.add_argument("--build-dir", default="build")
    args = ap.parse_args()

    map_path = os.path.join(ROOT, args.map)
    if not os.path.exists(map_path):
        print(f"FAIL: {args.map} does not exist. This check has nothing to read; "
              f"a guard that cannot read what it guards is not a guard.", file=sys.stderr)
        return 2

    rows, problems = map_rows(map_path)
    for p in problems:
        fail("table", p)
    if not rows:
        print(f"FAIL: {args.map} carries no requirement rows (expected a table with "
              f"the columns: {' | '.join(COLUMNS)}).", file=sys.stderr)
        return 2

    ids = build_ids(args.build_dir)
    if ids is None:
        print(f"skip: no configured build dir at {args.build_dir} -- the ctest half "
              f"of this check is SKIPPED; pointers, anchors, columns and SPEC "
              f"coverage are still checked. (cmake -B {args.build_dir})")
    else:
        print(f"note: {len(ids)} tests registered in {args.build_dir}")

    check_parents(rows)
    seen: dict[str, str] = {}
    for cells in rows:
        line = cells[-1].rsplit(":", 1)[-1]
        rows_total += 1
        if len(cells) - 1 < len(COLUMNS):
            rows_failed += 1
            fail(f"line {line}", f"row has {len(cells) - 1} column(s), needs "
                                 f"{len(COLUMNS)} ({' | '.join(COLUMNS)})")
            continue
        cols = dict(zip(COLUMNS, cells[:-1]))
        req = cols["REQ"].strip()
        if not req:
            rows_failed += 1
            fail(f"line {line}", "the REQ column is empty")
            continue
        if req in seen:
            rows_failed += 1
            fail(req, f"already has a row ({seen[req]}); one requirement, one row")
            continue
        seen[req] = f"line {line}"

        before = failures
        good = check_pointer(req, cols["requirement"].split(LABEL_SEP)[0], "requirement")
        for column in ("architecture", "design", "implementation"):
            good = check_cell(req, cols[column], column) and good
        good = check_verified(req, cols["verified by"], ids) and good
        if not good:
            rows_failed += 1
        elif failures == before:
            print(f"ok  : {req} -- {cols['status'] or 'no status'}  [line {line}]")

    total_spec_ids = 0
    spec_ids = spec_requirement_ids()
    for spec, spec_list in spec_ids.items():
        total_spec_ids += len(spec_list)
        for rid in spec_list:
            if rid not in seen:
                fail(rid, f"`{spec}` carries this requirement id in a heading and "
                          f"{args.map} has no row for it")
    if total_spec_ids == 0:
        print("FAIL: no requirement id was read from any docs/SPEC-*.md. Either the "
              "SPEC section structure changed or this scan is broken; a scan that "
              "finds nothing must not read as green.", file=sys.stderr)
        return 2
    print("note: %d requirement id(s) read from docs/SPEC-*.md (%s)"
          % (total_spec_ids, ", ".join(f"{k}: {len(v)}" for k, v in spec_ids.items())))

    print()
    if failures:
        print(f"{failures} problem(s); {rows_failed} of {rows_total} row(s) failed. "
              f"The map is the pointer layer -- a dead pointer is the failure mode "
              f"it exists to prevent.")
        return 1
    print(f"all {rows_total} row(s) resolve; every SPEC requirement id has a row")
    return 0


if __name__ == "__main__":
    sys.exit(main())
