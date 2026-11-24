#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Measure what partial-match Hanja recommendation would buy on a pinned corpus.

Issue #300 (0.4 Track 4, parent #286): before any engine code, measure on the
shipped krdict how often a typed Hangul buffer has a dictionary prefix, how long
that prefix is, how many selection rounds an out-of-dictionary eojeol needs
under greedy longest-prefix segmentation, and what that saves in keystrokes
against the current single-syllable fallback. The figures and the pricing model
live in docs/OPTIONS-MEASURED.md §HPX; this tool is how they are reproduced.

MODEL (assumptions, not rulings — the doc states them with the numbers):
  * A "typed buffer" is one eojeol form from a frequency list, weighted by its
    token count. Only pure precomposed-syllable forms (가..힣) are measurable;
    everything else is excluded and the exclusion is counted.
  * Dictionary = the tables the engine loads (candidate_tables.c): krdict word
    keys, idiom readings (the engine's >=4-syllable gate applies), and the
    single-syllable hanja table as the fallback. Keys with non-syllable
    characters (affix markers, spaces) can never match a pure-Hangul buffer and
    are excluded and counted.
  * "Has a prefix" means a PROPER prefix of >=2 syllables that is a krdict or
    idiom key. A 1-syllable prefix is the existing fallback, not a gain.
  * Full-conversion intent: every syllable with a hanja reading gets converted.
    Baseline rounds = one selection per hanja-capable syllable. Prefix rounds =
    greedy longest dictionary match (>=2 syllables) per step, else one syllable
    (hanja selection if the syllable has a reading, free Hangul commit if not).
    A dynamic program gives the minimum-rounds segmentation, so the greedy
    rule's gap to the best rule is measured, not assumed away.
  * Keystrokes: typing cost is identical under every option and drops out.
    A baseline round costs F9 + one selection key. A prefix round costs one
    selection key if the window re-opens on the remainder by itself, or F9 +
    one key if it does not; both are reported. Candidate paging is rank-
    dependent and unknown, so it is reported as exposure (candidates per round
    vs the 10-label page ceiling), never folded into the headline.

The bulk pass reads the same JSON tables the engine reads; it does not
reimplement composition. --verify ties the table view back to the engine oracle
(hunmin_suggest_count via ctypes) on a frequency-ordered sample and times the
lookup, which is the per-probe cost a prefix search would pay.

USAGE
  tools/measure-hanja-prefix.py --corpus ~/work/wikicorpus/full/hangul.txt \
      --format count-form --out ~/work/hunmin-measurements/t4-1-prefix/kowiki.json
  tools/measure-hanja-prefix.py --corpus ... --format form-count --verify 2000 ...
  tools/measure-hanja-prefix.py --selfcheck        # fixture guard, ctest tier-G

Corpus paths are arguments, never hardcoded (AGENTS.md: bulk corpora live
outside the repository). The tool writes <out> and <out>.meta.json; the meta
carries sha256 pins of every input, the command, and what the run excludes.
"""
import argparse
import collections
import ctypes
import datetime
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
DATA = HERE.parent / "libhunmin" / "data"
LIB = HERE.parent / "build" / "libhunmin.so"

SCHEMA = "hunmin.hanja-prefix-report/1"
PAGE = 10  # fcitx5 kMaxCandidatePageSize / TSF 10-row page; exposure ceiling


def is_syllables(s):
    return bool(s) and all("\uac00" <= c <= "\ud7a3" for c in s)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class Tables:
    """The engine's three candidate tables, as membership + multiplicity."""

    def __init__(self, data_dir):
        self.data_dir = pathlib.Path(data_dir)
        kp = self.data_dir / "krdict" / "krdict.json"
        ip = self.data_dir / "idioms" / "idioms.json"
        hp = self.data_dir / "hanja" / "hanja.json"
        self.paths = {"krdict": kp, "idioms": ip, "hanja": hp}
        self.sha = {k: sha256(p) for k, p in self.paths.items()}

        kd = json.loads(kp.read_text(encoding="utf-8"))
        keys = [e["ko"] for e in kd["entries"] if isinstance(e.get("ko"), str)]
        self.words = collections.Counter(k for k in keys if is_syllables(k))
        self.krdict_entries = len(keys)
        self.krdict_excluded_keys = len(set(keys) - set(self.words))

        idl = json.loads(ip.read_text(encoding="utf-8"))
        # The engine gates idioms on >=4 syllables (hunmin_suggest_count).
        rd = [e["reading"] for e in idl["entries"]
              if isinstance(e.get("reading"), str) and len(e["reading"]) >= 4
              and is_syllables(e["reading"])]
        self.idioms = collections.Counter(rd)
        self.idiom_entries = len(idl["entries"])

        hd = json.loads(hp.read_text(encoding="utf-8"))
        self.hanja = {k: len(v) for k, v in hd["entries"].items()}
        self.hanja_entries = sum(self.hanja.values())

    def hit(self, key):
        """Dictionary match of >=2 syllables, as a prefix search would probe."""
        return key in self.words or (len(key) >= 4 and key in self.idioms)

    def ncands(self, key):
        return self.words.get(key, 0) + self.idioms.get(key, 0)

    def longest(self, s, pos, proper=False):
        """Longest dictionary match at pos, >=2 syllables; 0 if none.
        proper=True forbids consuming the whole remaining string."""
        hi = len(s) - pos - (1 if proper else 0)
        for L in range(hi, 1, -1):
            if self.hit(s[pos:pos + L]):
                return L
        return 0


def greedy_segments(t, form):
    """[(length, kind, candidates, rounds)] — kind: word|idiom|hanja|hangul."""
    segs = []
    pos, n = 0, len(form)
    while pos < n:
        L = t.longest(form, pos)
        if L:
            key = form[pos:pos + L]
            kind = "word" if key in t.words else "idiom"
            segs.append((L, kind, t.ncands(key), 1))
            pos += L
        else:
            ch = form[pos]
            if ch in t.hanja:
                segs.append((1, "hanja", t.hanja[ch], 1))
            else:
                segs.append((1, "hangul", 0, 0))
            pos += 1
    return segs


def optimal_rounds(t, form):
    """Minimum selections for full conversion; the greedy rule's lower bound."""
    n = len(form)
    dp = [0] * (n + 1)
    for i in range(n - 1, -1, -1):
        best = dp[i + 1] + (1 if form[i] in t.hanja else 0)
        for L in range(2, n - i + 1):
            if t.hit(form[i:i + L]):
                cand = 1 + dp[i + L]
                if cand < best:
                    best = cand
        dp[i] = best
    return dp[0]


def read_corpus(path, fmt):
    """Yield (form, count). count-form: count<TAB>form; form-count: the reverse.
    The two shipped orientations are opposite (624 vs wikicorpus/119); reading
    one with the other's parser inverts the columns silently, so fmt is explicit
    or sniffed, never guessed per row."""
    with open(path, encoding="utf-8") as f:
        first = f.readline().rstrip("\n").split("\t")
        if len(first) != 2:
            raise SystemExit(f"{path}: first line is not two TAB fields: {first!r}")
        if fmt == "auto":
            a, b = first[0].isdigit(), first[1].isdigit()
            if a == b:
                raise SystemExit(f"{path}: cannot sniff orientation from {first!r}; pass --format")
            fmt = "count-form" if a else "form-count"
        f.seek(0)
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 2:
                continue
            if fmt == "count-form":
                cs, form = parts
            else:
                form, cs = parts
            try:
                count = int(cs)
            except ValueError:
                continue
            if count > 0:
                yield form, count


class Stats:
    def __init__(self):
        self.rows = 0
        self.tokens = 0
        self.excluded_forms = 0
        self.excluded_tokens = 0
        self.single_forms = 0
        self.single_tokens = 0
        self.single_covered_tokens = 0
        self.multi_forms = 0
        self.multi_tokens = 0
        self.exact_forms = 0
        self.exact_tokens = 0
        self.exact_idiom_tokens = 0
        self.oov_forms = 0
        self.oov_tokens = 0
        self.prefix_forms = 0
        self.prefix_tokens = 0
        self.prefix_len_sum_w = 0
        self.prefix_len_sum_f = 0
        self.prefix_len_hist = collections.Counter()
        self.baseline_sum_w = 0
        self.baseline_sum_f = 0
        self.greedy_sum_w = 0
        self.greedy_sum_f = 0
        self.optimal_sum_w = 0
        self.optimal_sum_f = 0
        self.greedy_eq_opt_tokens = 0
        self.savings_hist = collections.Counter()  # baseline-greedy -> tokens
        self.zero_baseline_tokens = 0
        self.new_coverage_tokens = 0
        self.base_pos_tokens = 0
        self.base_pos_b_sum_w = 0
        self.base_pos_g_sum_w = 0
        self.base_cand_sum_w = 0
        self.base_overpage_w = 0
        self.greedy_cand_sum_w = 0
        self.greedy_overpage_w = 0
        self.greedy_rounds_w = 0
        self.g_word_rounds_w = 0
        self.g_word_cand_sum_w = 0
        self.g_word_overpage_w = 0
        self.g_hanja_rounds_w = 0
        self.g_hanja_cand_sum_w = 0
        self.g_hanja_overpage_w = 0
        self.examples = []

    def observe(self, t, form, count):
        self.rows += 1
        self.tokens += count
        if not is_syllables(form):
            self.excluded_forms += 1
            self.excluded_tokens += count
            return
        n = len(form)
        if n == 1:
            self.single_forms += 1
            self.single_tokens += count
            if form in t.hanja or form in t.words:
                self.single_covered_tokens += count
            return
        self.multi_forms += 1
        self.multi_tokens += count
        if t.hit(form):
            self.exact_forms += 1
            self.exact_tokens += count
            if form not in t.words:
                self.exact_idiom_tokens += count
            return
        self.oov_forms += 1
        self.oov_tokens += count
        base = sum(1 for ch in form if ch in t.hanja)
        self.baseline_sum_w += base * count
        self.baseline_sum_f += base
        if base == 0:
            self.zero_baseline_tokens += count
        else:
            self.base_pos_tokens += count
            self.base_pos_b_sum_w += base * count
        for ch in form:
            m = t.hanja.get(ch, 0)
            if m:
                self.base_cand_sum_w += m * count
                if m > PAGE:
                    self.base_overpage_w += count
        pl = t.longest(form, 0, proper=True)
        if pl:
            self.prefix_forms += 1
            self.prefix_tokens += count
            self.prefix_len_sum_w += pl * count
            self.prefix_len_sum_f += pl
            self.prefix_len_hist[pl] += count
        segs = greedy_segments(t, form)
        g = sum(s[3] for s in segs)
        if base > 0:
            self.base_pos_g_sum_w += g * count
        if base == 0 and g > 0:
            # a dictionary word converts syllables the single-char table cannot:
            # coverage the fallback does not have, not a keystroke saving
            self.new_coverage_tokens += count
        self.greedy_sum_w += g * count
        self.greedy_sum_f += g
        self.greedy_rounds_w += g * count
        for L, kind, nc, r in segs:
            if r:
                self.greedy_cand_sum_w += nc * count
                if nc > PAGE:
                    self.greedy_overpage_w += count
                if kind in ("word", "idiom"):
                    self.g_word_rounds_w += count
                    self.g_word_cand_sum_w += nc * count
                    if nc > PAGE:
                        self.g_word_overpage_w += count
                else:
                    self.g_hanja_rounds_w += count
                    self.g_hanja_cand_sum_w += nc * count
                    if nc > PAGE:
                        self.g_hanja_overpage_w += count
        opt = optimal_rounds(t, form)
        self.optimal_sum_w += opt * count
        self.optimal_sum_f += opt
        if g == opt:
            self.greedy_eq_opt_tokens += count
        self.savings_hist[base - g] += count
        if pl and len(self.examples) < 25:
            self.examples.append({
                "form": form, "count": count, "syllables": n,
                "longest_proper_prefix": pl,
                "segments": [{"len": L, "kind": k, "candidates": nc}
                             for L, k, nc, _ in segs],
                "baseline_rounds": base, "greedy_rounds": g, "optimal_rounds": opt,
            })


def rate(num, den):
    return round(num / den, 6) if den else None


def mean(sum_w, den):
    return round(sum_w / den, 4) if den else None


def summarize(t, s, fmt, corpus_path, top):
    ot = s.oov_tokens
    rep = {
        "schema": SCHEMA,
        "corpus": {"path": str(corpus_path), "sha256": sha256(corpus_path),
                   "format": fmt, "rows_seen": s.rows, "tokens_seen": s.tokens,
                   "top_cap": top},
        "tables": {
            "krdict": {"sha256": t.sha["krdict"], "entries": t.krdict_entries,
                       "usable_distinct_keys": len(t.words),
                       "excluded_non_syllable_keys": t.krdict_excluded_keys},
            "idioms": {"sha256": t.sha["idioms"], "entries": t.idiom_entries,
                       "usable_readings": len(t.idioms)},
            "hanja": {"sha256": t.sha["hanja"], "syllables": len(t.hanja),
                      "entries": t.hanja_entries},
        },
        "population": {
            "excluded_non_syllable": {"forms": s.excluded_forms, "tokens": s.excluded_tokens},
            "single_syllable": {"forms": s.single_forms, "tokens": s.single_tokens,
                                "token_share": rate(s.single_tokens, s.tokens),
                                "covered_tokens": s.single_covered_tokens},
            "multi_syllable": {"forms": s.multi_forms, "tokens": s.multi_tokens},
        },
        "exact_dictionary_hit": {
            "forms": s.exact_forms, "tokens": s.exact_tokens,
            "token_rate_multi": rate(s.exact_tokens, s.multi_tokens),
            "form_rate_multi": rate(s.exact_forms, s.multi_forms),
            "idiom_tokens": s.exact_idiom_tokens,
        },
        "oov": {
            "forms": s.oov_forms, "tokens": ot,
            "token_rate_multi": rate(ot, s.multi_tokens),
            "prefix_hit": {"forms": s.prefix_forms, "tokens": s.prefix_tokens,
                           "token_rate": rate(s.prefix_tokens, ot),
                           "form_rate": rate(s.prefix_forms, s.oov_forms)},
            "longest_proper_prefix_syllables": {
                "mean_token_weighted": mean(s.prefix_len_sum_w, s.prefix_tokens),
                "mean_form_weighted": mean(s.prefix_len_sum_f, s.prefix_forms),
                "histogram_tokens": dict(sorted(s.prefix_len_hist.items())),
            },
            "rounds": {
                "baseline_mean_token_weighted": mean(s.baseline_sum_w, ot),
                "greedy_mean_token_weighted": mean(s.greedy_sum_w, ot),
                "optimal_mean_token_weighted": mean(s.optimal_sum_w, ot),
                "baseline_mean_form_weighted": mean(s.baseline_sum_f, s.oov_forms),
                "greedy_mean_form_weighted": mean(s.greedy_sum_f, s.oov_forms),
                "optimal_mean_form_weighted": mean(s.optimal_sum_f, s.oov_forms),
                "greedy_equals_optimal_token_share": rate(s.greedy_eq_opt_tokens, ot),
                "savings_histogram_tokens": dict(sorted(s.savings_hist.items())),
                "zero_baseline_tokens": s.zero_baseline_tokens,
                "new_coverage_tokens": s.new_coverage_tokens,
                "conditional_on_baseline_positive": {
                    "tokens": s.base_pos_tokens,
                    "baseline_mean": mean(s.base_pos_b_sum_w, s.base_pos_tokens),
                    "greedy_mean": mean(s.base_pos_g_sum_w, s.base_pos_tokens),
                },
            },
            "keystrokes": {
                "note": "typing cost identical under all options; selection keys only. "
                        "baseline round = F9 + digit (2). prefix round = digit (1) if the "
                        "window re-opens on the remainder, F9 + digit (2) if not. "
                        "paging exposure reported separately, rank-dependent.",
                "baseline_keys_per_oov_token": mean(2 * s.baseline_sum_w, ot),
                "prefix_keys_per_oov_token_reopen": mean(s.greedy_sum_w, ot),
                "prefix_keys_per_oov_token_f9": mean(2 * s.greedy_sum_w, ot),
                "saved_keys_per_1000_multi_tokens_reopen":
                    mean((2 * s.baseline_sum_w - s.greedy_sum_w) * 1000, s.multi_tokens),
                "saved_keys_per_1000_multi_tokens_f9":
                    mean((2 * s.baseline_sum_w - 2 * s.greedy_sum_w) * 1000, s.multi_tokens),
                "saved_keys_per_oov_token_reopen":
                    mean(2 * s.baseline_sum_w - s.greedy_sum_w, ot),
                "saved_keys_per_oov_token_f9":
                    mean(2 * (s.baseline_sum_w - s.greedy_sum_w), ot),
            },
            "candidate_exposure": {
                "page_ceiling": PAGE,
                "baseline_mean_candidates_per_round":
                    mean(s.base_cand_sum_w, s.baseline_sum_w),
                "baseline_rounds_over_page_share":
                    rate(s.base_overpage_w, s.baseline_sum_w),
                "greedy_mean_candidates_per_round":
                    mean(s.greedy_cand_sum_w, s.greedy_rounds_w),
                "greedy_rounds_over_page_share":
                    rate(s.greedy_overpage_w, s.greedy_rounds_w),
                "dictionary_rounds": {
                    "rounds_weighted": s.g_word_rounds_w,
                    "mean_candidates": mean(s.g_word_cand_sum_w, s.g_word_rounds_w),
                    "over_page_share": rate(s.g_word_overpage_w, s.g_word_rounds_w),
                },
                "single_syllable_rounds": {
                    "rounds_weighted": s.g_hanja_rounds_w,
                    "mean_candidates": mean(s.g_hanja_cand_sum_w, s.g_hanja_rounds_w),
                    "over_page_share": rate(s.g_hanja_overpage_w, s.g_hanja_rounds_w),
                },
            },
            "examples_top_frequency": s.examples,
        },
    }
    return rep


def engine_verify(t, corpus_path, fmt, n, libpath):
    """Cross-check the table view against hunmin_suggest_count, and time it."""
    if not pathlib.Path(libpath).exists():
        raise SystemExit(f"{libpath} not found — build first (cmake -B build && cmake --build build)")
    lib = ctypes.CDLL(str(libpath))
    lib.hunmin_ctx_new.restype = ctypes.c_void_p
    lib.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
    lib.hunmin_ctx_free.argtypes = [ctypes.c_void_p]
    lib.hunmin_suggest_count.restype = ctypes.c_int
    lib.hunmin_suggest_count.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.hunmin_suggest_kind.restype = ctypes.c_char_p
    lib.hunmin_suggest_kind.argtypes = [ctypes.c_void_p, ctypes.c_int]
    dirs = (ctypes.c_char_p * 1)(str(t.data_dir).encode())
    ctx = lib.hunmin_ctx_new(dirs, 1)
    if not ctx:
        raise SystemExit("hunmin_ctx_new failed")
    checked = mismatch = 0
    total_us = 0.0
    probes = 0
    # Warm-up: the tables load lazily on the first suggest call (6 MB JSON);
    # timing that call would price the load, not the probe.
    lib.hunmin_suggest_count(ctx, "가".encode())
    try:
        for form, _count in read_corpus(corpus_path, fmt):
            if checked >= n:
                break
            if not is_syllables(form) or len(form) < 2:
                continue
            b = form.encode()
            t0 = time.perf_counter()
            cnt = lib.hunmin_suggest_count(ctx, b)
            total_us += (time.perf_counter() - t0) * 1e6
            probes += 1
            kinds = {lib.hunmin_suggest_kind(ctx, i).decode() for i in range(cnt)}
            engine_hit = bool(kinds & {"word", "idiom"})
            if engine_hit != t.hit(form):
                mismatch += 1
                if mismatch <= 5:
                    print(f"verify mismatch: {form!r} engine={engine_hit} tables={t.hit(form)}",
                          file=sys.stderr)
            if not engine_hit:
                # every dictionary segment of the greedy split must be an engine hit
                pos = 0
                for L, kind, _nc, _r in greedy_segments(t, form):
                    if kind in ("word", "idiom"):
                        key = form[pos:pos + L]
                        c2 = lib.hunmin_suggest_count(ctx, key.encode())
                        k2 = {lib.hunmin_suggest_kind(ctx, i).decode() for i in range(c2)}
                        if not (k2 & {"word", "idiom"}):
                            mismatch += 1
                            if mismatch <= 5:
                                print(f"verify segment mismatch: {key!r} in {form!r}",
                                      file=sys.stderr)
                    pos += L
            checked += 1
    finally:
        lib.hunmin_ctx_free(ctx)
    return {"forms_checked": checked, "mismatches": mismatch,
            "mean_us_per_suggest_call": round(total_us / probes, 3) if probes else None,
            "probes_timed": probes}


def git_commit():
    try:
        return subprocess.run(["git", "-C", str(HERE), "rev-parse", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        return None


def write_meta(out_path, args, fmt, tables, extra=None):
    meta = {
        "schema": SCHEMA + ".meta",
        "what_this_is": "Prefix-match measurement for 0.4 Track 4 (issue #300): "
                        "dictionary-prefix coverage, rounds and keystroke pricing on a "
                        "pinned frequency list, against the shipped candidate tables.",
        "producer": "tools/measure-hanja-prefix.py",
        "git_commit": git_commit(),
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "command": " ".join(sys.argv),
        "inputs": {
            "corpus": {"path": str(args.corpus), "sha256": sha256(args.corpus),
                       "format": fmt,
                       "licence": "see the corpus's own meta/provenance file; bulk corpora "
                                  "are measurement instruments, never vendored (AGENTS.md)"},
            "data": {k: {"path": str(p), "sha256": tables.sha[k]}
                     for k, p in tables.paths.items()},
        },
        "assumptions": [
            "typed buffer = one eojeol form, weighted by token count; full-conversion intent",
            "prefix = proper prefix of >=2 syllables in krdict/idioms; 1 syllable is the existing fallback",
            "baseline round = F9 + one selection key; prefix round = 1 key (window re-opens) or 2 (F9 again)",
            "paging is rank-dependent: reported as candidate-count exposure vs the 10-label page, not priced",
            "greedy longest-prefix is the measured rule; DP minimum rounds bounds what a smarter rule could add",
        ],
        "what_it_does_not_represent": [
            "the population a pass line will name — that choice is a ruling (issue #286); "
            "each corpus carries its own bias in its own provenance",
            "candidate rank: which candidate the user wants is unmeasurable from a frequency list",
            "engine-side prefix-search latency: --verify times the existing exact-match probe, "
            "which is the per-probe cost shape, not an implementation",
        ],
    }
    if extra:
        meta.update(extra)
    mp = pathlib.Path(str(out_path) + ".meta.json")
    mp.write_text(json.dumps(meta, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    return mp


def run_measure(args):
    tables = Tables(args.data)
    fmt = args.format
    stats = Stats()
    t0 = time.perf_counter()
    seen = 0
    for form, count in read_corpus(args.corpus, fmt):
        stats.observe(tables, form, count)
        seen += 1
        if args.top and seen >= args.top:
            break
    # read_corpus sniffed the orientation; recover it for the record
    if fmt == "auto":
        with open(args.corpus, encoding="utf-8") as f:
            a = f.readline().rstrip("\n").split("\t")
        fmt = "count-form" if a[0].isdigit() else "form-count"
    rep = summarize(tables, stats, fmt, args.corpus, args.top or None)
    rep["wall_seconds"] = round(time.perf_counter() - t0, 1)
    if args.verify:
        rep["engine_verify"] = engine_verify(tables, args.corpus, fmt, args.verify, args.lib)
        if rep["engine_verify"]["mismatches"]:
            raise SystemExit("engine cross-check found mismatches — the table view drifted")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(rep, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
        mp = write_meta(args.out, args, fmt, tables)
        print(f"wrote {args.out} and {mp}")
    else:
        print(json.dumps(rep, ensure_ascii=False, indent=1))
    o = rep["oov"]
    print(f"rows={stats.rows} tokens={stats.tokens} "
          f"oov_tokens={o['tokens']} prefix_rate={o['prefix_hit']['token_rate']} "
          f"rounds base/greedy/opt={o['rounds']['baseline_mean_token_weighted']}/"
          f"{o['rounds']['greedy_mean_token_weighted']}/{o['rounds']['optimal_mean_token_weighted']}",
          file=sys.stderr)
    return 0


# ---------------------------------------------------------------------------
# selfcheck — a fixture small enough to compute by hand, and a negative control
# ---------------------------------------------------------------------------

def _fixture(root):
    d = pathlib.Path(root)
    (d / "krdict").mkdir(parents=True)
    (d / "idioms").mkdir()
    (d / "hanja").mkdir()
    # hanja: 가(2 readings) 나 다 라 사 have readings; 마 does not.
    hanja = {"가": [{"v": "家"}, {"v": "歌"}], "나": [{"v": "羅"}],
             "다": [{"v": "多"}], "라": [{"v": "羅"}], "사": [{"v": "事"}]}
    (d / "hanja" / "hanja.json").write_text(
        json.dumps({"entries": hanja}, ensure_ascii=False), encoding="utf-8")
    # krdict: 가나(1) 가나다(1) 나라(2) 마무(1, both syllables hanja-less) 가(1, single)
    kd = [{"ko": k, "hanja": "X"} for k in ("가나", "가나다", "나라", "나라", "마무", "가")]
    kd.append({"ko": "-가", "hanja": "家"})  # non-syllable key: excluded
    (d / "krdict" / "krdict.json").write_text(
        json.dumps({"entries": kd}, ensure_ascii=False), encoding="utf-8")
    (d / "idioms" / "idioms.json").write_text(
        json.dumps({"entries": [{"hanja": "X", "reading": "가나다라"}]}, ensure_ascii=False),
        encoding="utf-8")
    corpus = d / "corpus.txt"
    # count-form, frequency order irrelevant to the tool
    rows = [
        (100, "가나다라"),  # exact idiom hit
        (10, "가나다"),     # exact word hit
        (5, "가나"),        # exact word hit
        (20, "가나무"),     # OOV, prefix 가나; 무 hanja-less -> greedy 1, base 2
        (30, "가나다라사"),  # OOV (5 syl); greedy 가나다라? not a word key... see below
        (7, "마무사"),      # OOV; greedy 마무(word)+사 = 2 rounds vs base 1 -> savings -1
        (4, "마무무"),      # OOV; base 0 (no hanja syllable), greedy 마무+무 = 1 -> new coverage
        (3, "가나2"),       # excluded: non-syllable
        (2, "가"),          # single syllable, covered
        (1, "마"),          # single syllable, uncovered
    ]
    corpus.write_text("".join(f"{c}\t{f}\n" for c, f in rows), encoding="utf-8")
    return d, corpus


def selfcheck():
    with tempfile.TemporaryDirectory(prefix="hanja-prefix-selfcheck-") as root:
        d, corpus = _fixture(root)
        t = Tables(d)
        assert t.krdict_excluded_keys == 1, t.krdict_excluded_keys
        assert len(t.words) == 5 and t.words["나라"] == 2
        assert len(t.idioms) == 1 and len(t.hanja) == 5

        # 가나다라사: proper prefixes — 가나다라 is an idiom reading (4 syl) -> pl=4.
        # greedy: 가나다라(idiom,1) + 사(hanja,1) = 2; baseline 5; optimal 2.
        s = Stats()
        for form, count in read_corpus(corpus, "count-form"):
            s.observe(t, form, count)
        assert s.rows == 10 and s.tokens == 182
        assert s.excluded_forms == 1 and s.excluded_tokens == 3
        assert s.single_forms == 2 and s.single_tokens == 3 and s.single_covered_tokens == 2
        assert s.multi_tokens == 176
        assert s.exact_tokens == 115 and s.exact_idiom_tokens == 100
        assert s.oov_tokens == 61 and s.oov_forms == 4
        assert s.prefix_tokens == 61 and s.prefix_forms == 4
        # 마무사: proper prefix 마무 (2); 마무무: proper prefix 마무 (2)
        # prefix_len_sum_w = 2*20 + 4*30 + 2*7 + 2*4 = 182
        assert s.prefix_len_sum_w == 182, s.prefix_len_sum_w
        # rounds: 가나무 base2 greedy1; 가나다라사 base5 greedy2; 마무사 base1 greedy2; 마무무 base0 greedy1
        assert s.baseline_sum_w == 2 * 20 + 5 * 30 + 1 * 7, s.baseline_sum_w
        assert s.greedy_sum_w == 1 * 20 + 2 * 30 + 2 * 7 + 1 * 4, s.greedy_sum_w
        # optimal: 마무무 has nothing to convert -> 0; greedy still takes the word = 1
        assert s.optimal_sum_w == 1 * 20 + 2 * 30 + 1 * 7, s.optimal_sum_w
        assert s.savings_hist[1] == 20 and s.savings_hist[3] == 30, s.savings_hist
        assert s.savings_hist[-1] == 11  # 마무사 7 + 마무무 4
        assert s.zero_baseline_tokens == 4 and s.new_coverage_tokens == 4
        assert s.base_pos_tokens == 57 and s.base_pos_b_sum_w == 197 and s.base_pos_g_sum_w == 94
        # greedy == optimal on 가나무(1==1) and 가나다라사(2==2), not 마무사(2!=1), 마무무(1!=0)
        assert s.greedy_eq_opt_tokens == 50
        # candidate exposure: baseline rounds 가(2)+나(1) | 5 syllables | 사(1)
        assert s.base_cand_sum_w == (2 + 1) * 20 + (2 + 1 + 1 + 1 + 1) * 30 + 1 * 7
        assert s.base_overpage_w == 0  # fixture multiplicities all <= PAGE
        # greedy word rounds: 가나(1 cand)*20, idiom 가나다라(1)*30, 마무(1)*7, 마무(1)*4
        assert s.greedy_cand_sum_w == 1 * 20 + (1 + 1) * 30 + (1 + 1) * 7 + 1 * 4
        assert s.g_word_rounds_w == 61 and s.g_word_cand_sum_w == 61
        assert s.g_hanja_rounds_w == 37 and s.g_hanja_cand_sum_w == 37
        assert s.g_word_overpage_w == 0 and s.g_hanja_overpage_w == 0

        # the greedy rule can be worse than the optimum: fixture 가나다라 with the
        # idiom removed and 나다라 added as a word — greedy 가나+다+라=3, optimal 가+나다라=2
        (d / "idioms" / "idioms.json").write_text('{"entries": []}', encoding="utf-8")
        kd = [{"ko": k, "hanja": "X"} for k in ("가나", "나다라")]
        (d / "krdict" / "krdict.json").write_text(
            json.dumps({"entries": kd}, ensure_ascii=False), encoding="utf-8")
        t2 = Tables(d)
        segs = greedy_segments(t2, "가나다라")
        assert sum(x[3] for x in segs) == 3, segs
        assert optimal_rounds(t2, "가나다라") == 2

        # negative control: the guard must be able to fail
        try:
            assert optimal_rounds(t2, "가나다라") == 3, "deliberate wrong expectation"
        except AssertionError:
            pass
        else:
            raise SystemExit("selfcheck negative control did not fail")
    print("hanja-prefix selfcheck: OK (fixture aggregates, greedy-vs-optimal gap, negative control)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--corpus", type=pathlib.Path, help="frequency list, outside the repo")
    ap.add_argument("--format", choices=["count-form", "form-count", "auto"], default="auto",
                    help="column orientation; 624 is form-count, wikicorpus/119 are count-form")
    ap.add_argument("--data", type=pathlib.Path, default=DATA)
    ap.add_argument("--lib", type=pathlib.Path, default=LIB, help="engine .so for --verify")
    ap.add_argument("--out", type=pathlib.Path, help="report JSON; meta written beside it")
    ap.add_argument("--top", type=int, default=0, help="cap rows (frequency order); 0 = all")
    ap.add_argument("--verify", type=int, default=0,
                    help="cross-check N frequency-ordered forms against hunmin_suggest_count")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck()
    if not args.corpus:
        ap.error("--corpus is required unless --selfcheck")
    return run_measure(args)


if __name__ == "__main__":
    sys.exit(main())
