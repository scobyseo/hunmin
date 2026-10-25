#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Lint the hunmin wiki against wiki/SCHEMA.md.

Advisory by design, with ONE mechanical check that is not: `status hygiene`.
A page marked `void` or `superseded` that is still linked from an `active`
page is how a retracted measurement keeps getting quoted, and that is
detectable without judgement. Everything else here is a hint for a human.

Run:  tools/wiki-lint.py [--strict]
      --strict turns advisory findings into a non-zero exit (for CI)

Not registered in ctest: the wiki is documentation, and a doc test that fails
on prose would be noise. Run it before claiming the wiki is consistent.
"""
import argparse
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WIKI = os.path.join(ROOT, "wiki")

FM = re.compile(r"\A---\n(.*?)\n---\n", re.S)
LINK = re.compile(r"\[\[([^\]|#]+)(?:\|[^\]]*)?\]\]")
PAGE_DIRS = ("concepts", "entities", "decisions")
RAW_DIRS = ("raw/sessions", "raw/measurements", "raw/upstream")
VALID_STATUS = {"draft", "active", "superseded", "void"}
REQUIRED_FM = ("title", "created", "updated", "type", "status", "tags")

# Frontmatter `sources:` parsing — both block list and inline `[a, b]` forms.
# Owed since 2026-10-05 (wiki/log.md). Without it a page can cite a measurement
# that was never made — which one already does.
SOURCES = re.compile(r"^\s*sources:\s*(.*?)(?=\n\w|\Z)", re.M | re.S)
INLINE = re.compile(r"\[([^\]]*)\]")
ENTRY = re.compile(r"^[ \t]*-[ \t]*(\S+)", re.M)

# The tag taxonomy is read from SCHEMA.md rather than copied here. Two sources
# of truth with nothing comparing them is how the previous hardcoded set went
# stale in one direction (a tag added to the set, never to SCHEMA) without any
# check noticing, which is exactly what happened to `scope` on 2026-10-05.
#
# SCHEMA.md's taxonomy is five bullets, each `**Category**: tag, tag, ...`, with
# an optional explanatory paragraph indented under it. Only the leading
# comma-separated run of bare lowercase words on the heading line is the tag
# list; anything after it is prose explaining the category, and treating prose as
# tags would invent requirements out of an explanation. So the format has to
# stay a single line of tags -- if you wrap it, say why in a comment here.
TAXONOMY_HEADING = "## Tag taxonomy"
# A tag is lowercase alphanumerics, optionally hyphenated -- the vocabulary is
# open, so the pattern must not decide what names are legal, only what a tag
# token LOOKS like. Anything capitalised or punctuated is prose, not a tag.
TAG_TOKEN = r"[a-z0-9]+(?:-[a-z0-9]+)*"
TAXONOMY_HEAD = re.compile(rf"- \*\*(\w+)\*\*:\s*({TAG_TOKEN}(?:\s*,\s*{TAG_TOKEN})*)\s*,?\s*$")
# Deliberately loose about the marker: `- **Meta**:` and `- Meta:` both count as
# a category bullet, and a list that wraps onto continuation lines is still one
# category. So this matches the bullet start; the tag list is accumulated from
# the heading line plus following lines until the prose starts.
TAXONOMY_BULLET = re.compile(r"-\s*\*{0,2}(\w+)\*{0,2}:")
CONTINUATION = re.compile(rf"^\s*({TAG_TOKEN}(?:\s*,\s*{TAG_TOKEN})*)\s*,?\s*$")
MIN_TAGS = 1        # a taxonomy with fewer tags than this is a parse failure


def schema_tags(path=None):
    """The tag vocabulary, from SCHEMA.md, or an empty set if it is not intact.

    Intact means every category bullet contributed at least one tag. A category
    whose tag list is absent, unparseable, or continued onto a line the grammar
    does not accept is a malformed taxonomy, not an empty category -- dropping it
    would shrink the vocabulary silently, so a page using a dropped tag would
    pass as if that tag had been invented on the page and sanctioned nowhere:
    the exact failure this change exists to remove. Partial results are worse
    than none here, so this returns an empty set and the caller raises an error.
    """
    path = path or os.path.join(WIKI, "SCHEMA.md")
    try:
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
    except OSError:
        return set()
    block = text.split(TAXONOMY_HEADING, 1)
    if len(block) < 2:
        return set()
    body = block[1].split("\n## ", 1)[0]

    tags = set()
    categories = 0
    reading = False
    for raw in body.splitlines():
        line = raw.strip()
        if TAXONOMY_BULLET.match(raw):
            categories += 1
            reading = True
            m = TAXONOMY_HEAD.match(line)
            if not m:
                return set()      # a category exists but its list does not parse
            tags |= {t.strip() for t in m.group(2).split(",")}
        elif reading:
            c = CONTINUATION.match(line)
            if c:
                tags |= {t.strip() for t in c.group(1).split(",")}
            else:
                reading = False    # prose begins; the taxonomy has ended
    return tags if categories and len(tags) >= MIN_TAGS else set()

errors, warnings = [], []


def err(m):
    errors.append(m)


def warn(m):
    warnings.append(m)


def parse(path):
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    m = FM.match(text)
    if not m:
        return text, {}, text
    fm = {}
    key = None
    for line in m.group(1).splitlines():
        k = re.match(r"^([A-Za-z_]+):\s*(.*)$", line)
        if k:
            key = k.group(1)
            fm[key] = k.group(2).strip()
        elif key and line.strip().startswith("- "):
            fm.setdefault(key + "_list", []).append(line.strip()[2:].strip())
    return text, fm, text[m.end():]


def source_targets(value: str) -> list[str]:
    """The paths in one `sources:` value, block-list or inline."""
    inline = INLINE.search(value)
    if inline:
        return [t.strip().strip("'\"") for t in inline.group(1).split(",") if t.strip()]
    return [e.group(1).strip().strip("'\"") for e in ENTRY.finditer(value)]


# A `path:line` citation is only evidence while the line exists. The 2026-10-22
# extraction round cut tsf.cpp from 2,320 lines to 528, which silently turned
# three citations in a decision page into pointers past EOF (issue #134). The
# check is deliberately narrow: it fires only when the FILE exists and the line
# is past its end, so a dated `raw/` record naming a path that was renamed later
# is not flagged here (raw/ is exempt entirely).
CITE = re.compile(r"`?([A-Za-z0-9_./-]+\.(?:c|h|cpp|kt|py|sh|md|yml|json)):(\d+)(?:-(\d+))?`?")


def check_citation_lines(rel: str, text: str) -> None:
    if "/raw/" in rel.replace(os.sep, "/"):
        return
    for m in CITE.finditer(text):
        path, first, last = m.group(1), int(m.group(2)), m.group(3)
        full = os.path.join(ROOT, path)
        if not os.path.exists(full):
            continue  # a missing path is a different defect class; not this rule
        try:
            n = sum(1 for _ in open(full, encoding="utf-8", errors="replace"))
        except OSError:
            continue
        over = int(last) if last else first
        if over > n:
            err(f"{rel}: cites {path}:{over}, but that file has {n} lines "
                f"-- a citation past EOF is not evidence")


def check_sources_exist(rel: str, path: str, text: str) -> None:
    """Verify all `sources:` targets exist (relative to page dir or wiki root)."""
    fm = FM.match(text)
    if not fm:
        return
    block = SOURCES.search(fm.group(1))
    if not block:
        return
    page_dir = os.path.dirname(path)
    first_line = text[: fm.start() + 1 + block.start()].count("\n") + 1
    for offset, target in enumerate(source_targets(block.group(1))):
        if target.startswith(("http:", "https:", "~/", "/")):
            continue  # a host path or a URL is not a repo path
        # Sources are written relative to the page OR to the wiki root;
        # accept either, because the wiki has used both.
        cands = [
            os.path.normpath(os.path.join(page_dir, target)),
            os.path.normpath(os.path.join(WIKI, target)),
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


def pages():
    for d in PAGE_DIRS:
        base = os.path.join(WIKI, d)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            if name.endswith(".md"):
                yield os.path.join(d, name), os.path.join(base, name)


def raw_pages():
    for d in RAW_DIRS:
        base = os.path.join(WIKI, d)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            if name.endswith(".md"):
                yield os.path.join(d, name), os.path.join(base, name)


def main():
    ap = argparse.ArgumentParser(
        description="Lint the hunmin wiki against wiki/SCHEMA.md.")
    ap.add_argument("--strict", action="store_true",
                    help="treat advisory findings as failures (for CI)")
    a = ap.parse_args()

    if not os.path.isdir(WIKI):
        sys.exit(f"no wiki at {WIKI}")

    index = os.path.join(WIKI, "index.md")
    index_text = ""
    if os.path.exists(index):
        index_text = open(index, encoding="utf-8").read()
    else:
        err("index.md is missing (SCHEMA.md requires it)")

    allp = dict(list(pages()) + list(raw_pages()))
    stems = {os.path.splitext(os.path.basename(p))[0] for p in allp}
    meta = {}
    inbound = defaultdict(set)
    outbound: dict[str, set[str]] = {}
    status = {}

    # Read the vocabulary once, from SCHEMA.md. If it cannot be read, every tag
    # on every page is "unknown" -- which would turn a broken linter into a
    # silent pass over the whole wiki. That is the failure mode this change
    # exists to remove, so it is an error rather than a warning.
    valid_tags = schema_tags()
    if not valid_tags:
        err("SCHEMA.md tag taxonomy is missing or unparseable — "
            "cannot validate any tag (expected '- **Category**: tag, tag, ...' "
            "under '## Tag taxonomy')")

    for rel, path in allp.items():
        stem = os.path.splitext(os.path.basename(path))[0]
        text, fm, body = parse(path)
        meta[stem] = fm
        st = fm.get("status", "")
        status[stem] = st

        if rel.split(os.sep)[0] in PAGE_DIRS:
            for field in REQUIRED_FM:
                if field not in fm:
                    err(f"{rel}: frontmatter missing '{field}'")
            if st and st not in VALID_STATUS:
                err(f"{rel}: status '{st}' is not one of {sorted(VALID_STATUS)}")
            tags = [t.strip().strip("\"'") for t in
                    (fm.get("tags") or "").strip("[]").split(",") if t.strip()]
            for t in tags:
                if t and t not in valid_tags:
                    warn(f"{rel}: tag '{t}' is not in the SCHEMA taxonomy")
            if fm.get("contested", "").lower() == "true":
                warn(f"{rel}: marked contested — needs maintainer review")
        else:
            if not fm.get("ingested"):
                warn(f"{rel}: raw page has no 'ingested:' stamp")

        check_sources_exist(rel, path, text)
        check_citation_lines(rel, text)

        for target in LINK.findall(body):
            inbound[os.path.splitext(os.path.basename(target))[0]].add(stem)
            outbound.setdefault(stem, set()).add(
                os.path.splitext(os.path.basename(target))[0])

    # --- the mechanical one: a retracted page still cited from a live one ---
    retracted = {s for s, st in status.items() if st in ("void", "superseded")}
    # `inbound` is keyed by TARGET (who links here); the direction this check
    # needs is the page that CONTAINS the link, so read the outbound map. The
    # two were swapped here until 2026-10-25, which made the guard report the
    # linked-to page as the offender -- and therefore never caught a live page
    # citing a void one, which is the defect it exists to catch.
    for src, targets in outbound.items():
        if status.get(src) not in ("active", "draft"):
            continue
        for t in sorted(targets):
            if t in retracted:
                err(f"{src} (status: {status.get(src)}) links to {t} "
                    f"(status: {status.get(t)}) — a retracted page is still cited")

    # --- structural ---
    for stem, srcs in inbound.items():
        if stem in stems and not srcs and stem not in ("index", "SCHEMA", "log"):
            warn(f"{stem}: orphan — no inbound wikilink")
    for rel, path in allp.items():
        text, fm, body = parse(path)
        for target in LINK.findall(body):
            t = os.path.splitext(os.path.basename(target))[0]
            if t not in stems and t not in ("index", "SCHEMA", "log"):
                err(f"{rel}: broken wikilink [[{target}]]")
    if index_text:
        for stem in stems:
            if stem in ("index", "SCHEMA", "log"):
                continue
            if stem not in index_text:
                warn(f"{stem}: not listed in index.md")

    log = os.path.join(WIKI, "log.md")
    if os.path.exists(log):
        n = len(re.findall(r"^## \[", open(log, encoding="utf-8").read(), re.M))
        if n > 500:
            warn(f"log.md has {n} entries — rotate per SCHEMA.md")
    else:
        warn("log.md is missing")

    for e in errors:
        print(f"  ERROR    {e}")
    for w in warnings:
        print(f"  advisory {w}")
    print(f"\n{len(allp)} pages, {len(errors)} error(s), {len(warnings)} advisory")
    print("advisory findings do not fail the lint; only errors do.")
    return 1 if (errors or (a.strict and warnings)) else 0


if __name__ == "__main__":
    sys.exit(main())