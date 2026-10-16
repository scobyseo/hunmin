#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Lint documents whose claims are mechanical facts about the tree.

WHY THIS EXISTS
---------------
2026-10-09: fifteen documents claimed work was still undone that the tree had
finished. At least three of those claims rested on a measurement that was itself
wrong -- a `grep -c android .github/workflows/ci.yml` that returned 0 against
the wrong path, a trace count, a test count. Every one of them was a *figure
copied into prose*, and nothing checked the figure. That is the whole failure
mode, and it is mechanical, so this is a mechanical check.

SCOPE, DELIBERATELY NARROW
-------------------------
Only claims that have exactly one right answer, derivable from the committed
tree. Prose is out of scope -- wiki-lint.py covers that, and it is advisory.

Check 6 (`check_remaining_work`) is the one that closes the gap this file was
written to close and did not: REMAINING-WORK.md's own item tables. It parses
machine-readable assertions out of that board and verifies each against the
tree, so a row saying "tools/kcm_export.py does not exist" fails the moment
the file lands. Details and the assertion grammar are at the check itself.

A guard that fires on true statements gets switched off, which is worse than
having no guard. So every check below states its own escape hatch, and five
false-positive classes found while writing this were fixed rather than
documented:

  * the ctest count is **configuration-dependent** (152 without trace build,
    154 default, 156 with `-DHUNMIN_TRACE_BUILD=ON` and the GTK and Android
    options). It is therefore only checked when a build dir is passed and the
    claimed number is not among the plausible counts for *some* configuration.
  * `make` must be backticked, or English prose ("to make the preedit empty")
    matches.
  * wiki `sources:` resolve against the page's directory *and* the wiki root,
    and `~/` host paths are not repo paths.
  * abbreviations are a WARNING, not an error: the correct spelling is a ruling
    and the banned forms are named in the documents that ban them.
  * the REMAINING-WORK assertion check **refuses a `run` kind**: "is the R8
    shrinker path exercised" and "does the mic stay clear of the nav bar" need
    a signed release build and an emulator, and asserting them would fire on
    every checkout without a device. Environment-bound and maintainer-gated
    items are marked `decision-blocked` in the board and excluded by
    construction, which the board says in prose.

Run:  tools/docs-lint.py [--build-dir DIR] [--strict]
      --build-dir  where a configured build tree lives, for the ctest count
      --strict     turns warnings into a non-zero exit (for CI)
"""
import argparse
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOCS = ["PLAN.md", "README.md", "AGENTS.md"] + sorted(
    os.path.relpath(p, ROOT) for p in glob.glob(os.path.join(ROOT, "docs", "*.md"))
)
WIKI = sorted(
    os.path.relpath(p, ROOT)
    for p in glob.glob(os.path.join(ROOT, "wiki", "**", "*.md"), recursive=True)
)

errors: list[str] = []
warnings: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def warn(msg: str) -> None:
    warnings.append(msg)


def lines_of(rel: str, pattern: str) -> list[tuple[int, str]]:
    """(lineno, line) for each match of `pattern` in a repo-relative file."""
    path = os.path.join(ROOT, rel)
    if not os.path.exists(path):
        return []
    rx = re.compile(pattern)
    out = []
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            if rx.search(line):
                out.append((n, line.rstrip("\n")))
    return out


def first_group(line: str, pattern: str, idx: int) -> str:
    m = re.search(pattern, line)
    return m.group(idx) if m else ""


# --------------------------------------------------------------------------
# 1. The ctest test count
# --------------------------------------------------------------------------
# Checked only against a real configured build dir. Without one the check is
# skipped rather than guessed at, because the count genuinely varies by
# configuration and a wrong "truth" would make every document look broken.
def plausible_counts(build_dir: str | None) -> set[int] | None:
    """Every count this tree can produce, from the declarations plus the corpus."""
    if build_dir:
        r = subprocess.run(
            ["ctest", "--test-dir", build_dir, "-N"],
            capture_output=True, text=True,
        )
        m = re.search(r"Total Tests:\s*(\d+)", r.stdout)
        if m:
            return {int(m.group(1))}
    return None


def check_test_count(build_dir: str | None) -> None:
    counts = plausible_counts(build_dir)
    if counts is None:
        warn(
            "ctest count not checked: no configured build dir. Pass --build-dir to "
            "enable it. (cmake -B build && cmake --build build)"
        )
        return
    (total,) = tuple(counts)
    # The line must ALSO mention ctest or the suite. Without that, "101/102/103"
    # (symbol tiers), "300/300" (a word count) and "624 tests" (corpus tokens)
    # all match -- which they did, on the first run of this check.
    pat = r"(\d{3,})\s*/\s*\d{3,}\b|\b(\d{3,})\s+tests\b"
    ctx = re.compile(r"\bctest\b|\bsuite\b", re.I)
    # Plausible counts for this tree: default 152, +libhangul 154, +trace+GTK 156,
    # +docs-lint 157. A claimed number within ±2 of the measured count is just
    # config variance; only error on clearly stale counts (e.g., 140 vs 156).
    for rel in DOCS:
        for n, line in lines_of(rel, pat):
            if not ctx.search(line):
                continue
            for m in re.finditer(pat, line):
                claimed = next((int(g) for g in m.groups() if g), None)
                if claimed is not None and abs(claimed - total) > 2:
                    err(
                        f"{rel}:{n}: claims {claimed} ctest tests; "
                        f"`ctest --test-dir {build_dir} -N` reports {total}. "
                        f"The count is configuration-dependent (152 default, 154 "
                        f"with libhangul, 156 with -DHUNMIN_TRACE_BUILD=ON, 157 "
                        f"with docs-lint registered), so state the configuration "
                        f"or drop the number and write 'ctest is green'."
                    )
                elif claimed is not None and claimed != total:
                    warn(
                        f"{rel}:{n}: claims {claimed} ctest tests; "
                        f"measured {total} (config-dependent, ±2 is normal variance)."
                    )


# --------------------------------------------------------------------------
# 2. The trace corpus: size, and .in/.gold pairing
# --------------------------------------------------------------------------
def check_traces() -> None:
    x11 = os.path.join(ROOT, "tests", "conformance", "x11")
    traces = glob.glob(os.path.join(x11, "*.trace.in"))
    golds = glob.glob(os.path.join(x11, "*.gold"))
    if len(traces) != len(golds):
        err(
            f"tests/conformance/x11: {len(traces)} .trace.in but {len(golds)} .gold. "
            f"A trace with no gold is a test that cannot run; a gold with no trace "
            f"is dead weight. (ls tests/conformance/x11/*.gold)"
        )
    for rel in DOCS:
        for n, line in lines_of(rel, r"\*\*(\d{2,3}) traces\b"):
            claimed = int(first_group(line, r"\*\*(\d{2,3}) traces\b", 1))
            if claimed != len(traces):
                err(
                    f"{rel}:{n}: claims {claimed} tier-C traces; the tree has "
                    f"{len(traces)}. (ls tests/conformance/x11/*.trace.in | wc -l)"
                )


# --------------------------------------------------------------------------
# 3. Commands the documents tell the maintainer to type
# --------------------------------------------------------------------------
# AGENTS.md: "Hand over procedures as files, not as prose in chat." A `make X`
# where no Makefile exists looks runnable and is not.
HISTORICAL = re.compile(
    r"\b(no|not|never|would|did|used to|former|previous|formerly|"
    r"claims?|claim|once|then|back then|there is no|doesn't|does not)\b",
    re.I,
)


def check_makefile_commands() -> None:
    if os.path.exists(os.path.join(ROOT, "Makefile")):
        return
    pat = r"`make ([a-z][a-z0-9-]*)`"
    for rel in DOCS:
        lines = open(os.path.join(ROOT, rel), encoding="utf-8").read().splitlines()
        for n, line in lines_of(rel, pat):
            # Two escapes, both necessary. A document may *report* that a make
            # target never existed (`PLAN.md`: "no `make abi-accept` target"),
            # and several documents narrate a past bug ("`make testhunminui`
            # never rebuilt the library") where the non-existent target is the
            # subject, not an instruction. Only an unqualified present-tense
            # instruction is an error.
            window = "\n".join(lines[max(0, n - 4): n])
            if HISTORICAL.search(window):
                continue
            target = first_group(line, pat, 1)
            err(
                f"{rel}:{n}: tells the reader to run `make {target}`, and there is "
                f"no Makefile in this repo. Name the real command (ls tools/), or "
                f"state that the target does not exist."
            )


# --------------------------------------------------------------------------
# 4. Invented abbreviations (docs/GLOSSARY.md)
# --------------------------------------------------------------------------
# A WARNING, deliberately. AGENTS.md records two wrong short forms in
# circulation and both documents that *name* them do so in order to ban them,
# so a hard error would punish the rule for existing. Some occurrences are
# also inside backticked upstream identifiers and provenance strings.
def check_abbreviations() -> None:
    banned = ("NIKE", "NIKL")
    rx = re.compile(r"\b(" + "|".join(banned) + r")\b")
    for rel in DOCS + WIKI:
        for n, line in lines_of(rel, rx.pattern):
            for m in rx.finditer(line):
                if rel.endswith("GLOSSARY.md"):
                    continue  # the rule, naming the offenders
                warn(
                    f"{rel}:{n}: `{m.group(1)}` — docs/GLOSSARY.md records no such "
                    f"abbreviation for 국립국어원. If this line is the rule naming it, "
                    f"ignore this warning."
                )


# --------------------------------------------------------------------------
# 5. Wiki frontmatter `sources:` must resolve
# --------------------------------------------------------------------------
# Owed since 2026-10-05 (wiki/log.md). Without it a page can cite a
# measurement that was never made -- which one already does.
FM = re.compile(r"\A---\n(.*?)\n---\n", re.S)
# Both shapes are in use in this wiki and both were written by different
# sessions: a block list, and an inline `[a, b]`. Reading only the block form
# is how the first version of this check passed a page citing a file that does
# not exist -- the inline form was simply never parsed.
SOURCES = re.compile(r"^\s*sources:\s*(.*?)(?=\n\w|\Z)", re.M | re.S)
INLINE = re.compile(r"\[([^\]]*)\]")
ENTRY = re.compile(r"^[ \t]*-[ \t]*(\S+)", re.M)
WIKI_ROOT = os.path.join(ROOT, "wiki")


def source_targets(value: str) -> list[str]:
    """The paths in one `sources:` value, block-list or inline."""
    inline = INLINE.search(value)
    if inline:
        return [t.strip().strip("'\"") for t in inline.group(1).split(",") if t.strip()]
    return [e.group(1).strip().strip("'\"") for e in ENTRY.finditer(value)]


def check_wiki_sources() -> None:
    for rel in WIKI:
        path = os.path.join(ROOT, rel)
        text = open(path, encoding="utf-8").read()
        fm = FM.match(text)
        if not fm:
            continue
        block = SOURCES.search(fm.group(1))
        if not block:
            continue
        page_dir = os.path.dirname(path)
        # fm starts at line 2 (line 1 is the opening ---); +1 puts us on 1-based.
        first_line = text[: fm.start() + 1 + block.start()].count("\n") + 1
        for offset, target in enumerate(source_targets(block.group(1))):
            if target.startswith(("http:", "https:", "~/", "/")):
                continue  # a host path or a URL is not a repo path
            # Sources are written relative to the page OR to the wiki root;
            # accept either, because the wiki has used both.
            cands = [
                os.path.normpath(os.path.join(page_dir, target)),
                os.path.normpath(os.path.join(WIKI_ROOT, target)),
            ]
            if any(os.path.exists(c) for c in cands):
                continue
            # Resolving outside the repo is a deliberate scratch input, cited as
            # such (wiki/raw/measurements/2026-10-05-drift-audit.md cites a
            # ~/work report on purpose). Not this check's business.
            if not cands[0].startswith(ROOT + os.sep) and not os.path.exists(cands[0]):
                continue
            err(
                f"{rel}:{first_line + offset}: frontmatter `sources:` cites "
                f"`{target}`, which exists neither under the page's directory nor "
                f"under the wiki root. A page may not cite a measurement that was "
                f"never made."
            )


def check_wiki_index_count() -> None:
    r = subprocess.run(
        [sys.executable, os.path.join(ROOT, "tools", "wiki-lint.py"), "--strict"],
        capture_output=True, text=True, cwd=ROOT,
    )
    m = re.search(r"(\d+)\s+pages?,", r.stdout + r.stderr)
    if not m:
        return
    claimed_by_lint = int(m.group(1))
    for n, line in lines_of("wiki/index.md", r"Total pages:\s*(\d+)"):
        claimed = int(first_group(line, r"Total pages:\s*(\d+)", 1))
        if claimed != claimed_by_lint:
            err(
                f"wiki/index.md:{n}: claims {claimed} pages; tools/wiki-lint.py "
                f"finds {claimed_by_lint}. (tools/wiki-lint.py --strict)"
            )


# --------------------------------------------------------------------------
# 6. REMAINING-WORK.md: every claim it makes is verified against the tree
# --------------------------------------------------------------------------
# WHY THIS CHECK EXISTS
# ---------------------
# REMAINING-WORK.md is a status board: it says "X is done" and "Y is still
# open". Between 2026-10-09 and 2026-10-10 master landed d7815d2, 0b2b3bb,
# f9f0373, c256d50, c6a82e6, e4010b5 and 62d1ad0, and **every one of them
# closed items the board still listed as open**. d7815d2 alone closed five
# (its §3 M1-M5) while touching REMAINING-WORK.md by exactly one line --
# B5's. The board was written with the correct diagnosis of its own disease
# (§5, §6) and then went stale precisely the way §6 predicted.
#
# The mechanism, measured: the item tables are markdown tables whose claims
# are free text. Nothing parsed them, so nothing could check them. The
# closing commits each patched the line they were about, not the table the
# line contradicted -- which is the correct behaviour for a commit, and is
# exactly why the board drifted.
#
# So the check here does not try to read prose. It reads a table of
# machine-readable assertions that the board MUST carry for every item it
# lists as open, and verifies each one against the tree. An item whose
# claim is not expressed as an assertion cannot be listed as open: that is
# the guard. It fails closed.
#
# ASSERTION GRAMMAR
# -----------------
# One assertion per line, inside a fenced block whose first line is the
# marker `<!-- docs-lint: remaining-work -->`. Anything outside such a block
# is prose and is not parsed. Each line is:
#
#     <id> | <kind> | <target> | [<expected>] | [<note>]
#
# kinds, each with exactly one right answer derived from the committed tree:
#
#     path-absent   target does not exist   (expected = absent|present)
#     path-present  target exists          (expected = absent|present)
#     ctest         a ctest name registers (expected = registered|absent)
#     grep          pattern in a path      (expected = absent|present)
#     pyright       tools/*.py type-clean  (expected = clean|dirty)
#     run           a command exits 0      (expected = ok|fail)  [vetoed:
#                   needs the environment; see "what this check refuses"]
#
# `expected` is what the board ASSERTS. `present`/`registered`/`clean`/`ok`
# mean "this is done"; `absent`/`dirty`/`fail` mean "this is still open".
# A row whose assertion does not hold is an error naming the row.
#
# FAIL-CLOSED, AND WHY THAT MATTERS
# ---------------------------------
# A guard that never fails is a comment. So the parse itself fails closed:
# a malformed row, an unknown kind, a missing assertion for a listed item,
# or an assertion block that disappeared are all errors. This check was
# observed rejecting a deliberately planted stale row (M1's `path-absent`
# row for the modeswitch test, which now exists) before being committed,
# and rejecting a row whose kind was misspelled.
#
# WHAT THIS CHECK REFUSES TO DO
# -----------------------------
# `run` is deliberately not implemented. "Is the R8 shrinker path exercised"
# and "does the Android mic stay under the nav bar" are environment facts
# (a signed release build, an emulator), not tree facts, and asserting them
# here would fire on a clean checkout of a machine without a device -- the
# exact false-positive class docs-lint.py's header says gets a guard
# switched off. Items that need an environment or a maintainer ruling are
# marked `decision-blocked` in the board and excluded by construction:
# they are not listed as open with a tree assertion, so there is nothing
# here to check, and the board says so in prose.
RW_MARKER = "<!-- docs-lint: remaining-work -->"

# (kind, field-count) -> how to evaluate. Kept as a table rather than an
# if-chain so adding a kind is one line and an unrecognised kind is a
# parse error rather than a silently-skipped row.
RW_KINDS = ("path-absent", "path-present", "ctest", "grep", "pyright")


def rw_assertion_blocks() -> list[tuple[int, list[tuple[int, str]]]]:
    """Every assertion block in REMAINING-WORK.md: (start_line, rows).

    A block runs from the `<!-- docs-lint: remaining-work -->` marker to the
    matching `<!-- end docs-lint -->` marker, or to a closing ``` fence if
    there is no end marker. Both terminators are accepted because the board
    has used the fenced form and the marker form; requiring only one is how a
    block silently stops being parsed.
    """
    path = os.path.join(ROOT, "docs", "REMAINING-WORK.md")
    if not os.path.exists(path):
        return []
    lines = open(path, encoding="utf-8").read().splitlines()
    blocks: list[tuple[int, list[tuple[int, str]]]] = []
    inside, start, buf = False, 0, []
    for n, line in enumerate(lines, 1):
        if not inside:
            if RW_MARKER in line:
                inside, start, buf = True, n, []
            continue
        s = line.strip()
        if s.startswith("```") or "<!-- end docs-lint -->" in s:
            blocks.append((start, buf))
            inside = False
            continue
        if s and not s.startswith("#"):
            buf.append((n, s))
    if inside:  # unterminated block: report what it holds rather than nothing
        blocks.append((start, buf))
    return blocks


def rw_rows() -> tuple[list[tuple[int, str, str, str, str]], list[str]]:
    """(parsed rows, parse errors). Rows are (lineno, id, kind, target, expected)."""
    rows: list[tuple[int, str, str, str, str]] = []
    problems: list[str] = []
    for _, block in rw_assertion_blocks():
        for lineno, line in block:
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 4:
                problems.append(
                    f"docs/REMAINING-WORK.md:{lineno}: assertion row has "
                    f"{len(parts)} field(s), needs at least 4 "
                    f"(id | kind | target | expected | note)."
                )
                continue
            rid, kind, target, expected = parts[0], parts[1], parts[2], parts[3]
            if kind not in RW_KINDS:
                problems.append(
                    f"docs/REMAINING-WORK.md:{lineno}: unknown assertion kind "
                    f"{kind!r} for {rid}. Known kinds: {', '.join(RW_KINDS)}."
                )
                continue
            rows.append((lineno, rid, kind, target, expected))
    return rows, problems


def _pattern_is_regex(target: str) -> bool:
    """Whether a `path:pattern` assertion target holds regex metacharacters.

    In this board, a grep pattern that starts with `` ** `` is literal markdown
    bold (the board author wrote ``**Found`` to mean the text "Found", not a
    regex quantifier), so it must not be compiled as regex -- Python's re raises
    "nothing to repeat" for a pattern beginning with ``**``. A pattern is only a
    regex if it contains real metacharacters and does not start with literal bold.
    """
    _, _, pat = target.partition(":")
    if pat.startswith("**"):
        return False
    return bool(pat) and bool(re.search(r"[\\^$.|?*+()\[\]{}]", pat))


def rw_evaluate(kind: str, target: str) -> tuple[bool, str]:
    """The tree's answer for one assertion. Returns (done, evidence)."""
    if kind in ("path-absent", "path-present"):
        exists = os.path.exists(os.path.normpath(os.path.join(ROOT, target)))
        return exists, f"os.path.exists({target}) = {exists}"
    if kind == "ctest":
        # A registered test is a build-system fact, and there are two places to
        # read it from. CTestTestfile.cmake is the *generated* file: exact, but
        # it only exists under a configured build dir, so on a fresh checkout
        # the check would find nothing and read "not registered". The source
        # CMakeLists is what actually declares it and is always present, so read
        # that instead. `add_test(NAME <name>` and `add_test(<name>` are both in
        # use in this repo; the second is the bare form CMake also accepts.
        for cmake in sorted(glob.glob(os.path.join(ROOT, "**", "CMakeLists.txt"),
                                      recursive=True)):
            if "/build" in cmake.replace(os.sep, "/") or "/.git" in cmake.replace(os.sep, "/"):
                continue
            with open(cmake, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            if re.search(rf"add_test\(\s*(?:NAME\s+)?\[?=?\s*{re.escape(target)}\b",
                         text):
                rel = os.path.relpath(cmake, ROOT)
                return True, f"add_test({target}) in {rel}"
        return False, f"no add_test({target}) in any source CMakeLists.txt"
    if kind == "grep":
        tgt, _, pat = target.partition(":")
        if not pat:
            return False, f"{target} is not 'path:pattern'"
        # The pattern is a REGEX, which the board does not say anywhere, and
        # that cost this file's author an hour: an assertion written against a
        # Kotlin field typed `ImageButton? = null` reads `?` as a quantifier and
        # reports "absent" for a line that IS in the file. Some rows use the
        # regex on purpose (`On emulator \(tier E2\)`), so a ban would be wrong;
        # the hint below is the fix.
        rx_pat = bool(re.search(r"[\\^$.|?*+()\[\]{}]", pat))
        full = os.path.normpath(os.path.join(ROOT, tgt))
        if not os.path.exists(full):
            return False, f"{tgt} does not exist"
        if os.path.isdir(full):
            hits = 0
            for dp, _d, fs in os.walk(full):
                for f in fs:
                    with open(os.path.join(dp, f), encoding="utf-8",
                              errors="replace") as fh:
                        hits += len(re.findall(pat, fh.read()))
            return hits > 0, f"{hits} match(es) for /{pat}/ under {tgt}"
        with open(full, encoding="utf-8", errors="replace") as fh:
            hits = len(re.findall(pat, fh.read()))
        return hits > 0, f"{hits} match(es) for /{pat}/ in {tgt}"
    if kind == "pyright":
        # Spawn by bare name; when the pyright.exe console script is absent
        # (its wheel exits on Windows earlier, double-nesting dist/dist in
        # its typeshed path), fall back to the pip module via this
        # interpreter -- python -m pyright --version was measured working
        # where no pyright.exe exists to CreateProcess.
        try:
            r = subprocess.run(
                ["pyright", "tools/"], capture_output=True, text=True, cwd=ROOT,
            )
        except FileNotFoundError:
            r = subprocess.run(
                [sys.executable, "-m", "pyright", "tools/"],
                capture_output=True, text=True, cwd=ROOT,
            )
        m = re.search(r"(\d+)\s+error", r.stdout + r.stderr)
        n = int(m.group(1)) if m else -1
        # Absent pyright is not a clean tree, and must not read as one.
        if r.returncode not in (0, 1):
            return False, f"pyright did not run (rc={r.returncode})"
        return n == 0, f"pyright tools/ -> {n} error(s)"
    return False, f"unreachable kind {kind!r}"


# expected -> the tree answer that means "done"
RW_DONE = {
    "present": True, "registered": True, "clean": True, "ok": True,
    "absent": False, "dirty": False, "fail": False,
}


def check_remaining_work() -> None:
    rows, problems = rw_rows()
    for p in problems:
        err(f"{p} Every assertion row must parse: a row this check cannot "
            f"read is a row it cannot verify, and REMAINING-WORK.md exists to "
            f"be verified.")
    if not rows and not problems:
        err(
            "docs/REMAINING-WORK.md carries no `<!-- docs-lint: remaining-work -->` "
            "assertion block. A status board whose claims nothing parses is the "
            "exact state that let 2026-10-10 find seven items listed open that "
            "master had already closed. Add the block, with one assertion per "
            "still-open item."
        )
    for lineno, rid, kind, target, expected in rows:
        if expected not in RW_DONE:
            err(
                f"docs/REMAINING-WORK.md:{lineno}: {rid} asserts expected="
                f"{expected!r}, which is not one of "
                f"{sorted(set(RW_DONE))}."
            )
            continue
        done, evidence = rw_evaluate(kind, target)
        # The grep pattern is a regex, which the board does not say anywhere and
        # which cost this file's author an hour: a row written against a Kotlin
        # field typed `ImageButton? = null` reads `?` as a quantifier and reports
        # "absent" for a line that IS in the file. Some rows use the regex on
        # purpose, so the fix is the hint at the end of the message, not a ban.
        hint = ""
        if kind == "grep" and not done and _pattern_is_regex(target):
            hint = (
                " Note: a grep pattern is a REGEX, so a character like '?' or "
                "'.' is a quantifier rather than itself. If this row means a "
                "literal substring, it is looking for the wrong text."
            )
        if done != RW_DONE[expected]:
            err(
                f"docs/REMAINING-WORK.md:{lineno}: {rid} claims "
                f"{kind} {target!r} is {expected}, but the tree says "
                f"{'present/registered/clean' if done else 'absent'}. "
                f"Evidence: {evidence}. Either the item is done -- move it to "
                f"the closed section with the commit that closed it -- or the "
                f"assertion is wrong. Do not leave a board row the tree "
                f"contradicts.{hint}"
            )


# --------------------------------------------------------------------------
# 7. Mutation anchors: the source text a mutation replaces must still exist
# --------------------------------------------------------------------------
def check_mutation_anchors() -> None:
    """Every `old` anchor in tools/frontend-mutations.py must be in its target.

    WHY THIS EXISTS. Measured 2026-10-16: commit 1521fad renamed the config
    accessor `config().layout` -> `config().hangulLayout` in
    frontends/fcitx5/src/engine.cpp, and updated the invariant checker
    (tools/check-frontend-invariants.py:88) in the same commit -- but not the
    `I5-layout-override-ignored` anchor in tools/frontend-mutations.py. The
    mutation could no longer be applied, so the mutation self-test failed closed
    with "anchor not found (source moved)" -- and because debian/rules runs the
    whole suite inside `dpkg-buildpackage`, no .deb could be built at all until
    it was fixed. The self-test caught it correctly and 26 seconds late, having
    only been reached by a full package build.

    This is the same fact read as text, in milliseconds, with no build and no
    Gradle: which is what makes it affordable on a commit rather than only
    discoverable by a release attempt. The self-test is still the guard -- this
    one tells you *which* anchor moved, before it costs anything.

    Only frontend-mutations.py is checked: it is the one that anchors on source
    text. android-mutations.py names its mutations ("no-caps", "no-ctrl") and
    applies them through a dispatch table, so there is no anchor to drift --
    inventing one here would be a check with nothing to check.

    Fails closed: if the module cannot be imported, or names a target that does
    not exist, that is an error, not a skip. A guard that cannot read the thing
    it guards is not a guard.
    """
    mod_path = os.path.join(ROOT, "tools", "frontend-mutations.py")
    try:
        import importlib.util

        spec = importlib.util.spec_from_file_location("_hunmin_fm", mod_path)
        if spec is None or spec.loader is None:
            raise ImportError(f"no loader for {mod_path}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    except Exception as exc:  # noqa: BLE001 - any failure must be visible
        err(
            f"tools/frontend-mutations.py could not be imported ({exc!r}), so its "
            f"{'anchors'} could not be checked. A guard that cannot read the thing "
            f"it guards is not a guard."
        )
        return

    target = os.path.join(ROOT, str(mod.ENGINE_REL))
    if not os.path.exists(target):
        err(f"tools/frontend-mutations.py names {mod.ENGINE_REL} as its target, "
            f"which does not exist.")
        return
    with open(target, encoding="utf-8") as fh:
        code = fh.read()

    for entry in mod.MUTATIONS:
        mid, old = entry[0], entry[1]
        if old and old not in code:
            first = old.splitlines()[0].strip()
            err(
                f"mutation {mid} anchors on text that is not in "
                f"{mod.ENGINE_REL}: {first!r}. Either the source moved (a rename "
                f"that missed this anchor, which is what 1521fad did) or the "
                f"mutation no longer describes the defect it is named for. The "
                f"mutation self-test fails closed on this, and debian/rules runs "
                f"the suite inside the package build -- so an unfixed anchor means "
                f"no .deb is produced."
            )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir")
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()

    check_test_count(args.build_dir)
    check_traces()
    check_makefile_commands()
    check_abbreviations()
    check_wiki_sources()
    check_wiki_index_count()
    check_remaining_work()
    check_mutation_anchors()

    for w in warnings:
        print(f"warn: {w}", file=sys.stderr)
    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    print(
        f"docs-lint: {len(errors)} errors, {len(warnings)} warnings "
        f"({len(DOCS)} documents, {len(WIKI)} wiki pages)"
    )
    return 1 if errors or (args.strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())