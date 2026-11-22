#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Collect mixed-script rating sentences from KLDP sources (evaluation only).

The licence probe in docs/MODES-GATES.md found no reuse permission for KLDP
forum/blog/wiki. This tool therefore writes its corpus OUTSIDE the repository,
with per-sentence source URLs and a meta file saying what it does not license.
It is for rating autoswitch, not for redistribution.

The sentence filter is imported from tools/build-modes-sentences.py rather than
copied: the same 12-160 char, <=14 word, one Hangul word + one Latin word, no
mixed-script word rule grades the Wikipedia pin and this one.

Usage
  tools/collect-kldp-modes-corpus.py --seed-jsonl SEED.jsonl \
      --out OUT.jsonl --limit 1000
"""
import argparse
import datetime
import hashlib
import html
import importlib.util
import json
import pathlib
import re
import sys
import time
import urllib.parse
import urllib.request
from html.parser import HTMLParser

HERE = pathlib.Path(__file__).resolve().parent
DEFAULT_WIKI_INDEX = "https://wiki.kldp.org/wiki.php/TitleIndex"
DEFAULT_BLOG_INDEX = "https://kldp.org/blog"
DEFAULT_FORUM_INDEX = "https://kldp.org/forum/6"
UA = "hunmin-corpus-probe/1.0 (rating only; docs/MODES-GATES.md)"
NAV = {"TitleIndex", "FindPage", "HelpContents", "UserPreferences",
       "RecentChanges"}


def load_filter():
    path = HERE / "build-modes-sentences.py"
    spec = importlib.util.spec_from_file_location("build_modes_sentences", path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class Fetcher:
    def __init__(self, delay):
        self.delay = delay
        self.last = None
        self.responses = []

    def get(self, url):
        if self.last is not None:
            wait = self.delay - (time.monotonic() - self.last)
            if wait > 0:
                time.sleep(wait)
        req = urllib.request.Request(url, headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
            final = r.geturl()
        self.last = time.monotonic()
        self.responses.append({"url": url, "final_url": final,
                               "sha256": hashlib.sha256(data).hexdigest()})
        return data


def text_of(fragment):
    x = html.unescape(fragment)
    x = re.sub(r"<br\s*/?>", " ", x, flags=re.I)
    return re.sub(r"<[^>]+>", " ", x)


def feed_sentences(data, bms):
    text = data.decode("utf-8", "replace")
    rows = []
    for item in re.findall(r"<item>(.*?)</item>", text, re.S):
        mt = re.search(r"<title>(.*?)</title>", item, re.S)
        link = re.search(r"<link>(.*?)</link>", item, re.S)
        desc = re.search(r"<description>(.*?)</description>", item, re.S)
        if not (mt and link and desc):
            continue
        title = re.sub(r"\s+", " ", text_of(mt.group(1))).strip()
        url = html.unescape(link.group(1)).strip()
        for sent in bms.split_sentences(text_of(desc.group(1))):
            sent = bms.norm_space(sent)
            if keep(sent, bms):
                rows.append({"text": sent,
                             "source": {"kind": "blog", "title": title,
                                        "url": url}})
    return rows


class BodyParser(HTMLParser):
    """Text inside the first `<div class="field-item even">` of a node page."""
    def __init__(self):
        super().__init__()
        self.depth = 0
        self.parts = []

    def handle_starttag(self, tag, attrs):
        classes = (dict(attrs).get("class") or "").split()
        if tag == "div" and "field-item" in classes and "even" in classes:
            self.depth = 1
        elif self.depth and tag == "div":
            self.depth += 1

    def handle_endtag(self, tag):
        if self.depth and tag == "div":
            self.depth -= 1

    def handle_data(self, data):
        if self.depth:
            self.parts.append(data)


def node_sentences(data, title, url, bms):
    p = BodyParser()
    p.feed(data.decode("utf-8", "replace"))
    body = re.sub(r"\s+", " ", " ".join(p.parts)).strip()
    rows = []
    for sent in bms.split_sentences(body):
        sent = bms.norm_space(sent)
        if keep(sent, bms):
            rows.append({"text": sent,
                         "source": {"kind": "blog", "title": title, "url": url}})
    return rows


def blog_nodes(index_bytes):
    text = index_bytes.decode("utf-8", "replace")
    out = []
    for m in re.finditer(r'href="/node/(\d+)"', text):
        if m.group(1) not in out:
            out.append(m.group(1))
    return out


def blog_authors(index_bytes):
    text = index_bytes.decode("utf-8", "replace")
    out = []
    for m in re.finditer(r'href="/blog/(\d+)"', text):
        if m.group(1) not in out:
            out.append(m.group(1))
    return out


def wiki_pages(index_bytes):
    text = index_bytes.decode("euc-kr", "replace")
    out = []
    for m in re.finditer(r'href="/wiki\.php/([^"?#]+)', text):
        name = html.unescape(m.group(1))
        if name in NAV or name in out:
            continue
        out.append(name)
    return out


def wiki_sentences(data, name, bms):
    raw = data.decode("euc-kr", "replace")
    raw = re.sub(r"\[https?://[^\s\]]+\s+([^\]]+)\]", r"\1", raw)
    raw = re.sub(r"\[\[([^\]|]+)\|([^\]]+)\]\]", r"\2", raw)
    raw = re.sub(r"\[\[([^\]]+)\]\]", r"\1", raw)
    raw = re.sub(r"[='*]+", " ", raw)
    rows = []
    for sent in bms.split_sentences(raw):
        sent = bms.norm_space(sent)
        if not keep(sent, bms):
            continue
        rows.append({"text": sent,
                     "source": {"kind": "wiki", "title": name,
                                "url": "https://wiki.kldp.org/wiki.php/"
                                       + urllib.parse.quote(name, safe="/")
                                       + "?action=raw"}})
    return rows


def keep(sentence, bms):
    ok, _ = bms.assess(sentence, 160, 14)
    if not ok:
        return False
    kinds = [bms.classify(w) for w in bms.words(sentence)]
    return "hangul" in kinds and "latin" in kinds


def read_seed(path, bms):
    rows = []
    for raw in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        rec = json.loads(raw)
        if "meta" in rec or "text" not in rec:
            continue
        if keep(rec["text"], bms):
            src = rec.get("source")
            if isinstance(src, str):
                src = {"kind": src, "title": rec.get("title", ""),
                       "url": rec.get("url", "")}
            if not isinstance(src, dict):
                src = {"kind": "seed"}
            rows.append({"text": rec["text"], "source": src})
    return rows


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--seed-jsonl", type=pathlib.Path,
                    help="an earlier probe to include (its path is recorded)")
    ap.add_argument("--wiki-index", default=DEFAULT_WIKI_INDEX)
    ap.add_argument("--skip-wiki", action="store_true")
    ap.add_argument("--blog-index", default=DEFAULT_BLOG_INDEX)
    ap.add_argument("--blog-pages", type=int, default=0,
                    help="listing pages to scan for author feeds; 0 = none")
    ap.add_argument("--author-feeds", type=int, default=0,
                    help="author RSS feeds to read; 0 = none")
    ap.add_argument("--blog-nodes", type=int, default=0,
                    help="full blog node pages to read; 0 = none")
    ap.add_argument("--forum-index", default=DEFAULT_FORUM_INDEX)
    ap.add_argument("--forum-nodes", type=int, default=0,
                    help="full forum topic pages to read; 0 = none")
    ap.add_argument("--kldp-delay", type=float, default=10,
                    help="seconds between kldp.org requests (robots Crawl-delay)")
    ap.add_argument("--delay", type=float, default=0.2,
                    help="seconds between wiki requests (default 0.2)")
    ap.add_argument("--max-pages", type=int, default=0,
                    help="wiki pages to try; 0 = all from TitleIndex")
    ap.add_argument("--limit", type=int, required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    args = ap.parse_args()
    bms = load_filter()
    fetcher = Fetcher(args.delay)
    kldp = Fetcher(args.kldp_delay)

    rows = []
    seen = set()
    seed_info = None
    if args.seed_jsonl:
        rows = read_seed(args.seed_jsonl, bms)
        seed_info = {"path": str(args.seed_jsonl),
                     "sha256": hashlib.sha256(args.seed_jsonl.read_bytes()).hexdigest(),
                     "kept": len(rows)}

    for row in rows:
        seen.add(row["text"])

    authors, nodes, listing = [], [], []
    for page in range(args.blog_pages):
        url = args.blog_index if page == 0 else f"{args.blog_index}?page={page}"
        try:
            page_bytes = kldp.get(url)
            listing.append(page_bytes)
            authors += [a for a in blog_authors(page_bytes) if a not in authors]
            nodes += [n for n in blog_nodes(page_bytes) if n not in nodes]
        except Exception as exc:
            print(f"skip {url}: {exc}", file=sys.stderr)
        if len(rows) >= args.limit:
            break
    feeds_seen = nodes_seen = 0
    for nid in nodes[:args.blog_nodes]:
        if len(rows) >= args.limit:
            break
        url = f"https://kldp.org/node/{nid}"
        try:
            data = kldp.get(url)
        except Exception as exc:
            print(f"skip {url}: {exc}", file=sys.stderr)
            continue
        nodes_seen += 1
        title = re.search(r"<h1[^>]*class=\"title\"[^>]*>(.*?)</h1>",
                          data.decode("utf-8", "replace"), re.S)
        label = re.sub(r"\s+", " ", text_of(title.group(1))).strip() if title else nid
        for row in node_sentences(data, label, url, bms):
            if row["text"] in seen:
                continue
            seen.add(row["text"])
            rows.append(row)
            if len(rows) >= args.limit:
                break

    forum_nodes = []
    for page in range(args.forum_nodes and 9 or 0):
        url = args.forum_index if page == 0 else f"{args.forum_index}?page={page}"
        try:
            forum_nodes += [n for n in blog_nodes(kldp.get(url)) if n not in forum_nodes]
        except Exception as exc:
            print(f"skip {url}: {exc}", file=sys.stderr)
        if len(rows) >= args.limit:
            break
    forum_seen = 0
    for nid in forum_nodes[:args.forum_nodes]:
        if len(rows) >= args.limit:
            break
        url = f"https://kldp.org/node/{nid}"
        try:
            data = kldp.get(url)
        except Exception as exc:
            print(f"skip {url}: {exc}", file=sys.stderr)
            continue
        forum_seen += 1
        title = re.search(r"<h1[^>]*class=\"title\"[^>]*>(.*?)</h1>",
                          data.decode("utf-8", "replace"), re.S)
        label = re.sub(r"\s+", " ", text_of(title.group(1))).strip() if title else nid
        for row in node_sentences(data, label, url, bms):
            row["source"]["kind"] = "forum"
            if row["text"] in seen:
                continue
            seen.add(row["text"])
            rows.append(row)
            if len(rows) >= args.limit:
                break

    for uid in authors[:args.author_feeds]:
        if len(rows) >= args.limit:
            break
        url = f"{args.blog_index}/{uid}/feed"
        try:
            data = kldp.get(url)
        except Exception as exc:
            print(f"skip {url}: {exc}", file=sys.stderr)
            continue
        feeds_seen += 1
        for row in feed_sentences(data, bms):
            if row["text"] in seen:
                continue
            seen.add(row["text"])
            rows.append(row)
            if len(rows) >= args.limit:
                break

    pages_seen = 0
    if not args.skip_wiki:
        index = fetcher.get(args.wiki_index)
        names = wiki_pages(index)
        if args.max_pages:
            names = names[:args.max_pages]
        for name in names:
            if len(rows) >= args.limit:
                break
            url = ("https://wiki.kldp.org/wiki.php/"
                   + urllib.parse.quote(name, safe="/") + "?action=raw")
            try:
                data = fetcher.get(url)
            except Exception as exc:  # a dead wiki page is a drop, not a reason to stop
                print(f"skip {name}: {exc}", file=sys.stderr)
                continue
            pages_seen += 1
            for row in wiki_sentences(data, name, bms):
                if row["text"] in seen:
                    continue
                seen.add(row["text"])
                rows.append(row)
                if len(rows) >= args.limit:
                    break

    rows = rows[:args.limit]
    meta = {
        "tool": "tools/collect-kldp-modes-corpus.py",
        "invocation": " ".join(sys.argv),
        "generated_at": datetime.datetime.now(datetime.timezone.utc)
                                .isoformat(timespec="seconds"),
        "purpose": "autoswitch rating only; no redistribution or committed corpus",
        "rights": "no reuse permission found on kldp.org or wiki.kldp.org "
                  "(docs/MODES-GATES.md licence probe); evaluation use only",
        "filter": "tools/build-modes-sentences.py: 12-160 chars, <=14 words, "
                  ">=1 Hangul word and >=1 Latin word, no mixed-script word",
        "seed": seed_info,
        "wiki": {"index": args.wiki_index, "pages_seen": pages_seen,
                 "delay_seconds": args.delay, "skipped": args.skip_wiki},
        "blog": {"index": args.blog_index, "pages": args.blog_pages,
                  "authors_found": len(authors), "feeds_seen": feeds_seen,
                  "nodes_found": len(nodes), "nodes_seen": nodes_seen,
                  "delay_seconds": args.kldp_delay},
        "forum": {"index": args.forum_index, "nodes_found": len(forum_nodes),
                  "nodes_seen": forum_seen},
        "fetch_log": fetcher.responses + kldp.responses,
        "sentences": len(rows),
        "by_kind": {k: sum(1 for r in rows if r["source"].get("kind") == k)
                    for k in sorted({r["source"].get("kind") for r in rows})},
        "does_not_represent": [
            "not a target population: KLDP posts and wiki pages, not typed messages",
            "not licensed for redistribution; every row carries its source URL",
        ],
    }
    out = args.out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"meta": meta}, ensure_ascii=False) + "\n"
                   + "\n".join(json.dumps(r, ensure_ascii=False) for r in rows) + "\n",
                   encoding="utf-8")
    out.with_suffix(out.suffix + ".meta.json").write_text(
        json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{out}: {len(rows)} sentences {meta['by_kind']} "
          f"from {pages_seen} wiki pages")
    return 0 if len(rows) == args.limit else 1


if __name__ == "__main__":
    sys.exit(main())
