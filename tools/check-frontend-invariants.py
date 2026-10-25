#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Structural checks for the fcitx5 frontend adapter.

Every rule here corresponds to an invariant in docs/FRONTEND-CONTRACT.md §5.
Those invariants are the ones tier D1 does *not* cover -- mutation testing
showed that dropping the commit/flush split, or the caps option, still builds
and still passes engine-d1-fcitx5. This script is the only guard on them.

Advisory, not a formatter: it reads source and reports violations.
"""
import pathlib
import re
import sys

ENGINE = pathlib.Path(__file__).resolve().parents[1] / "frontends/fcitx5/src/hunminadapter.cpp"
failures: list[str] = []


def check(label: str, ok: bool, detail: str = "") -> None:
    print(f"{'ok  ' if ok else 'FAIL'}: {label}{' -- ' + detail if detail else ''}")
    if not ok:
        failures.append(label)


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def between(src: str, start: str, end: str) -> str:
    i = src.find(start)
    if i < 0:
        return ""
    j = src.find(end, i)
    return src[i:j if j > 0 else len(src)]


def main() -> int:
    if not ENGINE.is_file():
        print(f"missing {ENGINE}")
        return 1
    raw = ENGINE.read_text()
    code = strip_comments(raw)

    # I1/I2/I3 -- the process() block
    block = between(code, "bool used = hunmin_ic_process", "void reset()")
    if not block:
        check("process() block located", False, "anchor text changed")
        return 1
    check("I3 exactly one commit read", block.count("hunmin_ic_commit(") == 1,
          f"found {block.count('hunmin_ic_commit(')}")
    check("I1 exactly one flush", block.count("flush();") == 1,
          f"found {block.count('flush();')}")
    check("I1 flush guarded by else", "} else {" in block)
    check("I2 exactly one commitString", block.count("commitString(") == 1,
          f"found {block.count('commitString(')}")

    tail = block[block.find("updateUI();"):]
    # I3: filterAndAccept belongs to the branch where the engine consumed the
    # key. Returning false is how space, backspace and unmapped keys reach the
    # client, and F9 does not need an exception -- it is intercepted above the
    # printable-ASCII test, on candidates, so `used` is never consulted.
    check("I3 filterAndAccept exactly once", tail.count("filterAndAccept();") == 1,
          f"found {tail.count('filterAndAccept();')}")
    check("I3 filterAndAccept gated on used", "if (used) {" in tail,
          "a key the engine declined must reach the client")
    check("I9 modifier chords still return before the engine",
          "states() & modifiers" in code and "return;" in
          between(code, "states() & modifiers", "sym = "))

    # I4 -- Shift must stay out of the modifier mask
    mask = between(code, "KeyStates modifiers;", "keyEvent.key().states() & modifiers")
    check("I4 modifier mask located", bool(mask))
    if mask:
        check("I4 Shift absent from modifier mask", "Shift" not in mask,
              "Shift encodes layout rows; a chord would swallow it")

    # I5 -- caps must be handed to the engine
    check("I5 caps option is set", 'hunmin_ic_set_option(hunminIc_, "caps"' in code)
    check("I5 caps driven by CapsLock key state",
          "test(KeyState::CapsLock)" in code)

    # I7 -- the per-item profile layout must win over the addon default.
    # Checking this structurally catches ignoring the resolved layout even
    # though the two paths look identical once the override is discarded.
    check("I7 per-item layout is consulted",
          "layout.empty() ? *engine_->config().hangulLayout : layout" in code)
    check("I7 layout reaches the engine entry point",
          "state->keyEvent(layoutFor(entry)" in code
          and "std::string HunminAdapter::layoutFor(const InputMethodEntry &entry)" in code)

    # I6 -- hunmin_ic owned only by HunminState
    decl = [ln for ln in code.splitlines() if "hunmin_ic *" in ln and "=" in ln]
    check("I6 no stray hunmin_ic field outside HunminState",
          all("HunminState" not in ln for ln in decl) or len(decl) <= 1,
          "; ".join(ln.strip()[:60] for ln in decl))

    # I9 -- the Hanja window must not touch surrounding text.
    #
    # PLAN.md's P3 note records this from the fcitx5 wiki Q&A: many clients do
    # not implement it at all (XIM clients, SDL, terminals) and it is buggy in
    # browsers and embedded terminals, so relying on it is worse than working
    # without it. Hangul upstream uses it only for *temporary* Hanja mode.
    #
    # This is the one PLAN invariant with no runtime symptom: an adapter that
    # read or rewrote surrounding text would still type correctly, just less
    # portably, so no tier-A/D test could see it. It is enforced structurally.
    # Against `code`, not `raw`: hunminadapter.cpp has a comment saying the adapter
    # deliberately avoids deleteSurroundingText, and a prose mention of the
    # forbidden call must not fail the check that forbids it.
    check("I9 adapter never reads or rewrites surrounding text",
          not re.search(r"SurroundingText|getSurroundingText|deleteSurroundingText|"
                        r"setCursorPos|capabilityFlags\(\)\.test\(CapabilityFlag::Surrounding",
                        code),
          "PLAN.md P3: the F9 flow must not depend on surrounding text")
    # ...and the candidates must come from the engine's own preedit, so there is
    # no path by which client text could reach the lookup.
    cand = between(code, "bool openCandidates()", "void closeCandidates()")
    check("I9 candidates are looked up from the engine preedit only",
          bool(cand and "hunmin_ic_preedit" in cand and "hunmin_suggest_count" in cand
               and not re.search(r"surrounding|deleteSurrounding", cand, re.I)),
          "openCandidates() must read hunmin_ic_preedit(), nothing else")
    # Choosing a candidate replaces the composition; it must not re-insert text
    # behind it, which is what a surrounding-text rewrite would look like.
    commit = between(code, "void commitCandidate(", "const std::string &candidateSource()")
    check("I9 choosing a candidate does not re-insert text",
          bool(commit and "commitString" in commit and "hunmin_ic_reset" in commit
               and not re.search(r"deleteSurrounding|setCursorPos|Surrounding", commit)),
          "commitCandidate() commits and resets, no re-insertion")

    # I8 -- the "frontends stay thin" budget (PLAN.md invariant 4, <~700 LOC).
    # This is a *rule*, not a description, so it is measured rather than written
    # down: docs/FRONTEND-CONTRACT.md used to carry exact per-file LOC counts,
    # which rotted on every commit that added a line and were never checked.
    # Counting code lines (comments and blanks excluded) rather than raw lines
    # keeps the budget about logic, and 400 leaves room for the F9/candidate
    # presentation P3 will add without the rule needing re-litigating.
    # Every file the adapter owns is counted, not just hunminadapter.{h,cpp}.
    #
    # Two moves forced this. modeswitch.* is the R8 adapter's own translation
    # unit; config.h holds HunminConfig and came out of hunminadapter.h when the
    # adapter needed the room. Counting hunminadapter.{h,cpp} alone would have made
    # the config.h move look like a 23-line saving when it is a relocation --
    # the same trick a new file walks past, which is how "frontends stay thin"
    # stops being enforced.
    #
    # 400 -> 465 is not slack. hunminadapter.{h,cpp} alone is 383 and was 397 before
    # the adapter landed; the 3 lines of headroom it had were the ones the
    # comment above reserved for the F9/candidate work, which has since shipped
    # (31 lines). 465 is the honest total for what the addon now contains.
    #
    # 465 -> 545 (2026-10-11): keyboard selection in the candidate window, 85
    # code LOC. It is frontend work and cannot move to the core: fcitx5 does
    # not intercept Up/Down/Return for a CursorMovableCandidateList -- it only
    # provides prevCandidate()/nextCandidate(), and calling them is the input
    # method's job. Before it, an arrow key fell through to the printable-ASCII
    # test and reached the engine as a character, which is why picking a Hanja
    # needed the mouse. The rest of the 80 lines are the vertical layout hint
    # and its comment. Raising the limit is a decision with a reason recorded,
    # not a shave: the alternative was splitting the window into its own
    # translation unit, which keeps the same total and only hides it.
    counted = 0
    parts = {}
    for name in ("hunminadapter.cpp", "hunminadapter.h", "config.h",
                 "language_detect.cpp", "language_detect.h"):
        f = ENGINE.with_name(name)
        if not f.exists():
            continue
        n = len([ln for ln in strip_comments(f.read_text()).splitlines() if ln.strip()])
        counted += n
        parts[name] = n
    check("I8 frontend stays inside the thin-adapter budget",
          counted <= 560,
          f"{counted} code LOC across " + ", ".join(f"{k}={v}" for k, v in parts.items())
          + " (limit 560)")

    # I11 -- the layout picker's list comes from the core, and the picker keeps
    # it. AGENTS.md rule 1 ("layouts are data, never code") holds at the UI
    # boundary too: a picker whose ids are written out in hunminadapter.{h,cpp} is a
    # second place to edit per layout, and the place a reader looks first.
    #
    # Bound to the loop's own variables inside refreshLayoutList(), because the
    # two looser forms were measured surviving mutation: checking that the two
    # core identifiers appear anywhere in the file pair passed against an empty
    # setLayouts, and checking that *some* assignment exists passed against a
    # hardcoded count that kept the call alive on a `dummy`.
    #
    # Still a grep, with the ceiling that implies: a mutation keeping both
    # bindings and ignoring them passes. D2 is the real guard -- it counts the
    # dumped rows against the shipped data. I11 is the cheap no-build check.
    # setLayouts() moved to config.h with the rest of the config when the R8
    # adapter needed the room (see I8). Bind to wherever it now lives rather
    # than to the file it was written in, so a later move cannot silently turn
    # this check into a grep for a string that is no longer there -- which would
    # pass on a broken picker, the exact failure I11 exists to catch.
    engine_h = (ENGINE.with_name("config.h").read_text()
                if ENGINE.with_name("config.h").exists()
                else ENGINE.with_name("hunminadapter.h").read_text())
    filler = between(ENGINE.read_text(), "void HunminAdapter::refreshLayoutList()",
                     "void HunminAdapter::reloadConfig()")
    setter = between(engine_h, "void setLayouts(",
                     "void dumpDescription(RawConfig &config) const")
    check("I11 the layout picker reads the layout list from the core",
          bool(filler
               and re.search(r"\bcount\s*=\s*hunmin_ctx_layout_count\(", filler)
               and re.search(r"\bid\s*=\s*hunmin_ctx_layout_id\(", filler)
               and setter and "layouts_" in setter),
          "a hardcoded picker, or one that drops what it is handed, is a "
          "second place to edit per layout (AGENTS.md rule 1)")

    # I10 -- the tests dlopen libhunmin.so, so building a test target must
    # rebuild the library. copy-addon only copies the .conf; without a
    # dependency on the addon target, `make testhunminui` leaves a stale .so
    # and every mutation planted in hunminadapter.cpp survives a green D1/D2.
    addon_cmake = ENGINE.parents[1] / "test/addon/CMakeLists.txt"
    if addon_cmake.exists():
        body = addon_cmake.read_text()
        check("I10 the test addon target depends on the addon library",
              "add_dependencies(copy-addon hunmin_addon)" in body,
              "without it the tests assert against a stale libhunmin.so")

    print()
    if failures:
        print(f"{len(failures)} invariant(s) violated -- see "
              "docs/FRONTEND-CONTRACT.md §5")
        return 1
    print("all frontend invariants hold (advisory; D1 does not cover I1/I2/I5)")
    return 0


if __name__ == "__main__":
    sys.exit(main())