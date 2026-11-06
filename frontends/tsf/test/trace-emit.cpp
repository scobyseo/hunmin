/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #80): replay a Win32 platform-native trace
 * (docs/trace-spec.md, {"vk","scan","mods"} events) through the real TSF
 * adapter over the mock TSF layer (mock-tsf.h), and emit the per-event
 * result as JSONL for tools/trace-check.py to diff against the expectation
 * tier A implies -- the same gate fcitx5 tier C applies.
 *
 * Usage: tsf-trace-emit <case>.trace.in [--delivery native|interim]
 *
 * The delivery mode is a REGISTRATION seam, not a trace field (issue #185):
 * docs/FRONTEND-EQUIVALENCE.md §2 requires the same commit / preedit /
 * consumed in both of TSF's delivery modes, so the same corpus with the same
 * expectations replays twice - once through the native composition path, once
 * through the interim document-span path. The second has no composition to
 * observe, so its assertion channel is the document the mode actually writes:
 * the mock's edit-control channel (mock-tsf-edit.h), read the same way the
 * mock's own document is.
 *
 * Exit 0 diff-clean output, 1 assertion failed, 2 bad input,
 * 77 skip (engine data unavailable -- the runner/toolchain-absent path).
 */
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <msctf.h>
#include <richedit.h>
#include <shlwapi.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include "mock-tsf-edit.h"
#include "mock-tsf.h"
#include "tsf-testdir.h"

/*
 * Two test seams, both registration-based now (the include-.cpp white-box
 * seam is retired): the adapter source is LINKED, not included, so the
 * compiled logic is the linked DLL's logic.
 *
 * 1. The key-state provider: the live adapter reads shift from the
 *    thread's keyboard state; the harness registers tsf_mock_GetKeyState
 *    into keybinding.h's provider slot so a shifted event tests the
 *    shifted row deterministically.
 * 2. The shell's Snapshot() observation channel (tsf.h) exposes the
 *    engine ic, ctx and buffered word without #define-private-public.
 */
SHORT tsf_mock_GetKeyState(int vk);
SHORT g_mockKeyState[256] = {};

SHORT tsf_mock_GetKeyState(int vk) { return g_mockKeyState[vk & 0xFF]; }

// Definition for the extern tsf.cpp declares; dllmain.cpp (which owns it in
// the DLL build) is not linked into the harness.
extern "C" HINSTANCE g_hunmin_hinst = nullptr;

#include "../src/tsf.h"

namespace {

// --- the small JSONL reader/writer; same shape as fcitx5's trace-emit ---

std::string jsonEscape(const std::string &s) {
    std::string o;
    for (char c : s) {
        switch (c) {
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                o += buf;
            } else {
                o += c;
            }
        }
    }
    return o;
}

unsigned long numberField(const std::string &line, const char *key, unsigned long fallback) {
    const std::string pat = std::string("\"") + key + "\"";
    auto p = line.find(pat);
    if (p == std::string::npos)
        return fallback;
    p = line.find(':', p + pat.size());
    if (p == std::string::npos)
        return fallback;
    ++p;
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
        ++p;
    if (p < line.size() && line[p] == '"')
        return fallback;
    return std::strtoul(line.c_str() + p, nullptr, 0);
}

// `consumed` is a JSON boolean in the trace (docs/trace-spec.md), not a
// string: reading it through stringField() would try to match the opening
// quote, never find one, and silently read every event as `false` -- which
// re-labels all consumed events as boundaries and breaks the contract.
bool boolField(const std::string &line, const char *key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto p = line.find(pat);
    if (p == std::string::npos)
        return false;
    p = line.find(':', p + pat.size());
    if (p == std::string::npos)
        return false;
    ++p;
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
        ++p;
    return line.compare(p, 4, "true") == 0;
}

std::string stringField(const std::string &line, const char *key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto p = line.find(pat);
    if (p == std::string::npos)
        return {};
    p = line.find(':', p + pat.size());
    if (p == std::string::npos)
        return {};
    ++p;
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
        ++p;
    if (p >= line.size() || line[p] != '"')
        return {};
    ++p;
    std::string o;
    while (p < line.size() && line[p] != '"') {
        if (line[p] == '\\' && p + 1 < line.size()) {
            switch (line[++p]) {
            case 'n':
                o += '\n';
                break;
            case 't':
                o += '\t';
                break;
            case 'r':
                o += '\r';
                break;
            case 'u': {
                // BMP code points only; the corpus is NFC Hangul plus ASCII.
                const unsigned cp = std::strtoul(line.c_str() + p + 1, nullptr, 16);
                if (cp < 0x80) {
                    o += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    o += static_cast<char>(0xC0 | (cp >> 6));
                    o += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    o += static_cast<char>(0xE0 | (cp >> 12));
                    o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    o += static_cast<char>(0x80 | (cp & 0xFFF));
                }
                p += 4;
                break;
            }
            default:
                o += line[p];
            }
            ++p;
            continue;
        }
        o += line[p++];
    }
    return o;
}

// Case-separator engine options (docs/trace-spec.md): `options` is an object
// of option name -> 0/1, applied to the fresh engine at the case boundary.
std::vector<std::pair<std::string, int>> optionFields(const std::string &line) {
    std::vector<std::pair<std::string, int>> out;
    auto p = line.find("\"options\"");
    if (p == std::string::npos)
        return out;
    p = line.find('{', p);
    if (p == std::string::npos)
        return out;
    const auto end = line.find('}', p);
    if (end == std::string::npos)
        return out;
    while (p < end) {
        const auto q = line.find('"', p);
        if (q == std::string::npos || q >= end)
            break;
        const auto r = line.find('"', q + 1);
        if (r == std::string::npos || r > end)
            break;
        const std::string name = line.substr(q + 1, r - q - 1);
        auto c = line.find(':', r + 1);
        if (c == std::string::npos || c > end)
            break;
        ++c;
        while (c < end && std::isspace(static_cast<unsigned char>(line[c])))
            ++c;
        out.emplace_back(name, static_cast<int>(std::strtol(line.c_str() + c, nullptr, 0)));
        p = line.find(',', c);
        if (p == std::string::npos || p > end)
            break;
    }
    return out;
}

std::vector<std::string> readLines(const std::string &path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "tsf-trace-emit: cannot open " << path << "\n";
        std::exit(2);
    }
    std::vector<std::string> out;
    std::string l;
    while (std::getline(f, l)) {
        if (!l.empty())
            out.push_back(l);
    }
    return out;
}

std::string wideToUtf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? (size_t)n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

[[noreturn]] void fail(const std::string &msg) {
    std::cerr << "tsf-trace-emit: " << msg << "\n";
    std::exit(1);
}

} // namespace

int main(int argc, char **argv) {
    /* CoInitialize for the adapter's ITfCategoryMgr::RegisterGUID (the
     * display-attribute atom, issue #98): the real system category manager
     * serves it in a console process just as in a host. */
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#ifdef _DEBUG
    // Debug-CRT asserts default to a message box and die with rc 3 before
    // anything reaches stdout/stderr; run the assertion text to stderr so
    // ctest output can carry it.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    /* argv: the trace, plus the delivery mode this replay runs in (issue
     * #185). The mode is a registration seam, not a trace field: one ctest
     * entry per mode passes its own value, so the SAME .trace.in and .gold are
     * the expectation for both. */
    std::string tracePath;
    std::string delivery = "native";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--delivery") {
            if (i + 1 >= argc) {
                std::cerr << "usage: tsf-trace-emit <case>.trace.in [--delivery native|interim]\n";
                return 2;
            }
            delivery = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "tsf-trace-emit: unknown option " << arg << "\n";
            return 2;
        } else {
            tracePath = arg;
        }
    }
    if (tracePath.empty() || (delivery != "native" && delivery != "interim")) {
        std::cerr << "usage: tsf-trace-emit <case>.trace.in [--delivery native|interim]\n";
        return 2;
    }
    const bool interim = delivery == "interim";
    const auto lines = readLines(tracePath);
    if (lines.empty() || lines[0].find("\"meta\"") == std::string::npos) {
        std::cerr << "tsf-trace-emit: line 1 must be meta (docs/trace-spec.md)\n";
        return 2;
    }
    const std::string platform = stringField(lines[0], "platform");
    if (platform != "win32") {
        std::cerr << "tsf-trace-emit: corpus platform is " << platform << "; this runner replays win32 only\n";
        return 2;
    }

    // Engine data: the repo tree this build was configured from.
    _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
    // Keep the adapter's own debug log out of the checked-out tree when the
    // sandbox sets this; inside ctest the module-relative default (next to
    // the exe, in the build tree) is fine and need not be redirected.

    // Layout wiring (issue #66): the trace's own meta decides the layout the
    // adapter composes with, overriding whatever the host's HKCU TIP config
    // holds — a developer's real settings must not move a conformance run.
    // The latin slot is pinned too, so no trace depends on stored state.
    const std::string traceLayout = stringField(lines[0], "layout");
    if (traceLayout.empty()) {
        std::cerr << "tsf-trace-emit: meta carries no layout\n";
        return 2;
    }
    _putenv_s("HUNMIN_TSF_HANGUL_LAYOUT", traceLayout.c_str());
    _putenv_s("HUNMIN_TSF_LATIN_LAYOUT", "us-qwerty");
    /* Autoswitch (R13-정정) is stored state the corpus was NOT recorded under,
     * and it changes what the layer shows: with it ON the engine replaces an
     * invalid Hangul word with its Latin reading, so a replay composited `Hs`
     * where tier A recorded `ㅗ` and the buffered-word assertions broke at
     * dubeolsik-seosi event 117 (measured 2026-11-06: the production store had
     * Autoswitch=1, persisted by a settings chord test's SpecialPerform). Pin
     * the seam OFF, non-empty so config.cpp's loadSlot lets it override the
     * registry -- the same rule the layout pins above follow. */
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "0");
    /* The delivery seam (issue #185): the adapter's own forced-mode hatch
     * (tsf.cpp DetectLegacyMode). Cleared for a native run too, so a value a
     * sibling case or the caller's environment leaked cannot flip the mode
     * under a run that is supposed to measure the composition path. */
    _putenv_s("HUNMIN_TSF_INTERIM", interim ? "1" : "");

    tsfmock::MockThreadMgr *tm = nullptr;
    tsfmock::MockContext *ctx = nullptr;
    tsfmock::MockDocumentMgr *dm = nullptr;
    CTextService *svc = nullptr;

    /* The interim mode's client (issue #185): a real control window that holds
     * a document the way MockContext::doc holds the TSF one, and records every
     * write with the span it replaced. Created for BOTH modes - the native path
     * must be measurably indifferent to it - and reset with each case's own
     * CTextService (the delivery resolves the control by focus and counts its
     * span from zero, so the two must start from the same state). */
    tsfmock::MockEditControl edit;
    if (!edit.Create()) {
        std::cerr << "skip: the mock's edit-control channel could not be created\n";
        std::exit(77);
    }

    // One adapter+mock setup per case. A batch file separates cases with
    // {"meta": {"case": ...}} lines; each case starts from a clean engine
    // and a clean mock document (docs/trace-spec.md "Multi-case batch
    // files"), the same isolation tier A gives every case.
    auto SvcSnap = [&]() { return svc->Snapshot(); };
    /* Tier A's preedit (the engine's in-flight buffer) and, per issue #185, the
     * document THIS mode writes: the mock's document under the native path, the
     * client control under the interim one. One set of expectations reads both. */
    auto Preedit = [&]() { return std::string(SvcSnap().layer ? hunmin_buffered_preedit_composing(SvcSnap().layer) : ""); };
    auto DocText = [&]() { return wideToUtf8(interim ? edit.doc() : ctx->doc); };
    auto startRun = [&]() {
        edit.Reset(); // a clean client, like the clean engine below
        tm = new tsfmock::MockThreadMgr(); // ref 1
        ctx = new tsfmock::MockContext(); // ref 1
        dm = new tsfmock::MockDocumentMgr(ctx); // ref 1
        ctx->AddRef(); // dm holds the top reference
        svc = new CTextService(); // ref 1
        svc->Activate(tm, 1);
        // The key-sink advise contract the measured registration work
        // established: sink present, and foreground=TRUE (docs/
        // WINDOWS-TSF-REGISTRATION.md; fForeground=FALSE left a profile that
        // was selected but never received keys).
        if (!tm->keySink) {
            std::cerr << "skip: Activate did not advise a key sink\n";
            std::exit(77);
        }
        if (tm->keySinkForeground != TRUE)
            fail("AdviseKeyEventSink was called with fForeground != TRUE");
        if (!SvcSnap().ctx || !SvcSnap().layer) {
            std::cerr << "skip: engine context unavailable (check HUNMIN_DATA_DIR / libhunmin/data)\n";
            std::exit(77);
        }
    };
    auto endRun = [&]() {
        svc->Release(); // adapter destructor frees ic_/ctx_/lastCtx_
        ctx->Release();
        dm->Release();
        tm->Release();
        svc = nullptr;
        ctx = nullptr;
        dm = nullptr;
        tm = nullptr;
    };

    // Register the harness's key-state snapshot as the adapter's provider
    // (keybinding.h) - the seam the old #define GetKeyState macro carried.
    hunmin_tsf_keystate_register(&tsf_mock_GetKeyState);

    startRun();
    std::cout << lines[0] << "\n";

    // Buffering-contract state, all derived from the .in's own expectations
    // (copied verbatim from tier A) -- nothing here invents an answer:
    //   heldExpected  : commits tier A records since the last boundary; the
    //                   buffered word the adapter must be holding
    //   cumulativeCommits : every tier-A commit so far; what the document
    //                   must contain once the events have passed
    std::string heldExpected;
    std::string cumulativeCommits;

    for (size_t i = 1; i < lines.size(); ++i) {
        const std::string &line = lines[i];
        if (line.find("\"meta\"") != std::string::npos) {
            // Batch separator: reset the adapter and every per-case
            // assertion accumulator, then keep going (issues #80).
            endRun();
            startRun();
            heldExpected.clear();
            cumulativeCommits.clear();
            /* The case separator's `options` (docs/trace-spec.md): engine
             * options the source golden pinned for this case, applied to the
             * fresh engine before its first key. A rejected name is a trace the
             * runner cannot reproduce, not a value to ignore. */
            for (const auto &opt : optionFields(line)) {
                if (!hunmin_buffered_preedit_set_option(SvcSnap().layer, opt.first.c_str(), opt.second))
                    fail("case separator option \"" + opt.first + "\" is not an engine option");
            }
            std::cout << line << "\n";
            continue;
        }
        const unsigned long vk = numberField(line, "vk", 0);
        const unsigned long scan = numberField(line, "scan", 0);
        const unsigned long mods = numberField(line, "mods", 0);
        if (vk == 0)
            fail("event has no vk: not a win32 trace");
        const bool consumedExpected = boolField(line, "consumed");
        const std::string expectedCommit = stringField(line, "commit");

        // Inject the event's own key state, then walk the documented
        // chain: OnTestKeyDown (preview), then OnKeyDown (docs/
        // WINDOWS-TSF-REGISTRATION.md "C&C: key-down propagation").
        g_mockKeyState[VK_SHIFT & 0xFF] = (mods & 0x01) ? (SHORT)-32768 : (SHORT)0;
        /* CapsLock bit 0x02: GetKeyState(VK_CAPITAL) toggled-on, the seam
         * hunmin_tsf_capslock_from_state() reads (docs/trace-spec.md). */
        g_mockKeyState[VK_CAPITAL & 0xFF] = (mods & 0x02) ? (SHORT)0x8001 : (SHORT)0;
        const std::wstring wordBefore = SvcSnap().finished;
        const int endsBefore = ctx->compositionEnds;
        const size_t insertsBefore = ctx->insertions.size();
        const std::string docBefore = DocText();
        const size_t writesBefore = edit.writeCount();

        BOOL testEat = FALSE;
        svc->OnTestKeyDown(ctx, (WPARAM)vk, (LPARAM)scan, &testEat);
        BOOL eat = FALSE;
        svc->OnKeyDown(ctx, (WPARAM)vk, (LPARAM)scan, &eat);
        /* The interim write is POSTED (delivery.cpp: never inside a TSF
         * callback - the measured focus storm), so settle it here, exactly as
         * the live host does at the entry of its next key event. The native
         * delivery's FlushPending is a documented no-op. */
        svc->FlushPendingDocWrites();

        const size_t insertsAfter = ctx->insertions.size();
        // Tier A's preedit is the engine's in-flight buffer after the key.
        const std::string preedit = Preedit();
        const std::string docAfter = DocText();
        const size_t writesAfter = edit.writeCount();
        // Collect the SetText payloads this event produced (0 or 1; more is
        // a buffering violation caught by the assertion below).
        std::string insertedUtf8;
        for (size_t k = insertsBefore; k < insertsAfter; ++k)
            insertedUtf8 += wideToUtf8(ctx->insertions[k]);

        // Tier-A semantics reconstruction. The adapter holds each event's
        // finalised text in wordbuf_ and drains it only at a boundary, so:
        //   - insert this event:  wordbuf before the insert was a prefix of
        //     it; tier A's commit for this event is the remainder
        //   - no insert:          wordbuf grew (or stayed) by exactly that
        //     commit; the delta is the commit
        const std::string beforeUtf8 = wideToUtf8(wordBefore);
        const std::string afterUtf8 = wideToUtf8(SvcSnap().finished);
        std::string commit;
        if (interim) {
            /* Under the interim delivery the control IS the document: the
             * writer appends the newly finished half and rewrites the span
             * with the syllable in flight (delivery.cpp InterimWrite), so the
             * text always reads [cumulative commits][preedit]. Stripping the
             * span gives this event's commit - the same tier-A quantity the
             * native branches below reconstruct from the composition. */
            if (docAfter.size() < preedit.size() || docAfter.compare(docAfter.size() - preedit.size(), preedit.size(), preedit) != 0)
                fail("event " + std::to_string(i) + ": the control's text \"" + docAfter + "\" does not end with the in-flight preedit \"" + preedit +
                     "\"");
            const std::string cumulativeNow = docAfter.substr(0, docAfter.size() - preedit.size());
            if (cumulativeNow.size() < cumulativeCommits.size() || cumulativeNow.compare(0, cumulativeCommits.size(), cumulativeCommits) != 0)
                fail("event " + std::to_string(i) + ": the control no longer carries the committed prefix \"" + cumulativeCommits + "\" (holds \"" +
                     cumulativeNow + "\")");
            commit = cumulativeNow.substr(cumulativeCommits.size());
        } else if (insertsAfter > insertsBefore) {
            if (insertedUtf8.size() < beforeUtf8.size() || insertedUtf8.compare(0, beforeUtf8.size(), beforeUtf8) != 0)
                fail("insert at event " + std::to_string(i) + " does not extend the buffered word: contract break");
            commit = insertedUtf8.substr(beforeUtf8.size());
        } else if (ctx->compositionEnds > endsBefore) {
            /* Native boundary (issue #98): the word drained through the
             * composition end -- no plain insert, by design. The flush and
             * the drain both happen INSIDE this event, so before/after word
             * buffers can agree; the fused range text is the delivered word
             * and this event's own engine commit is its tail past the
             * previously held text. */
            const std::string fused = wideToUtf8(ctx->compositionText);
            if (beforeUtf8 != heldExpected || fused.size() < heldExpected.size() || fused.compare(0, heldExpected.size(), heldExpected) != 0)
                fail("event " + std::to_string(i) + ": composition fused \"" + fused + "\" does not carry held \"" + heldExpected + "\"");
            commit = fused.substr(heldExpected.size());
        } else {
            if (afterUtf8.size() < beforeUtf8.size() || afterUtf8.compare(0, beforeUtf8.size(), beforeUtf8) != 0)
                fail("word buffer shrank without an insert at event " + std::to_string(i));
            commit = afterUtf8.substr(beforeUtf8.size());
        }

        // The buffering contract itself (issue #73, requirement 3 of the
        // tier): nothing may be inserted while the engine consumes keys;
        // at a boundary exactly one session must carry the whole word. It is a
        // statement about the TSF channels, so the native mode owns it; the
        // interim mode's equivalent is its channel proof in this block.
        if (interim) {
            /* Issue #185: under the interim delivery the client's text arrives
             * ONLY through the control's own EDIT-family messages, and only
             * when the writer wrote - a document that moved with no write means
             * the mode is not the one under test, and a TSF delivery here means
             * the same. The PREEDIT may move with no write: at a boundary the
             * span is closed by resetting the layer, and the syllable it showed
             * is already document text (delivery.cpp InterimDrain). */
            if (ctx->compositionStarts != 0 || !ctx->insertions.empty())
                fail("event " + std::to_string(i) + ": interim mode delivered through TSF (composition starts " +
                     std::to_string(ctx->compositionStarts) + ", plain inserts " + std::to_string(ctx->insertions.size()) + ")");
            if (docAfter != docBefore && writesAfter == writesBefore)
                fail("event " + std::to_string(i) + ": the control's text moved with no write through the edit-control channel");
        } else if (consumedExpected) {
            if (insertsAfter != insertsBefore)
                fail("event " + std::to_string(i) + " inserted while the engine consumed the key");
            // The composition shows the buffers (issue #98): never a plain
            // insert while the engine consumes; the composition channel
            // covers the visible text.
            heldExpected += expectedCommit;
        } else {
            const std::string word = heldExpected + expectedCommit;
            if (!word.empty()) {
                // Boundary delivery of the pending word, exactly once:
                // either the composition ENDED with the word fused (the
                // native path, issue #98), or the host refused compositions
                // and the plain fallback inserted it (the 0.2 shape). Never
                // both, never none.
                const bool compEnded = ctx->compositionEnds > endsBefore;
                const std::string fused = wideToUtf8(ctx->compositionText);
                if (!(compEnded && fused == word) && !(insertsAfter - insertsBefore == 1 && insertedUtf8 == word))
                    fail("event " + std::to_string(i) + ": expected one boundary delivery of \"" + word + "\" (composition fused \"" +
                         (compEnded ? fused : "(none)") + "\", plain inserts " + std::to_string(insertsAfter - insertsBefore) + ")");
            } else if (insertsAfter != insertsBefore) {
                fail("event " + std::to_string(i) + " inserted with nothing pending");
            }
            heldExpected.clear();
        }
        cumulativeCommits += expectedCommit;
        // Total committed text (docs/WINDOWS-TSF-BUFFERING.md), issue #98:
        // with a composition the pending text IS document text (the
        // composition range holds the whole buffered word plus the in-flight
        // preedit), so the document now carries cumulativeCommits + preedit;
        // the fallback path (a host that refused compositions) keeps the
        // 0.2 shape, document + held. Under the interim delivery (issue #185)
        // the same invariant holds with no composition and no held word at
        // all - the client control carries every commit, then the preedit.
        const std::string heldNow = wideToUtf8(SvcSnap().finished);
        const std::string docNow = DocText();
        const std::string preeditNow = preedit;
        if (interim) {
            if (!heldNow.empty())
                fail("event " + std::to_string(i) + ": the interim delivery left \"" + heldNow + "\" held in the layer");
            if (docNow != cumulativeCommits + preeditNow)
                fail("committed text after event " + std::to_string(i) + ": control \"" + docNow + "\" != tier A's cumulative \"" +
                     cumulativeCommits + "\" + preedit \"" + preeditNow + "\"");
        } else if (ctx->compositionStarts > 0) {
            const bool compActive = ctx->compositionActive;
            if (compActive) {
                // Invariant: what the composition shows == held + preedit.
                const std::string inRange = wideToUtf8(ctx->compositionText);
                if (inRange.size() != heldNow.size() + preeditNow.size() || inRange.compare(0, heldNow.size(), heldNow) != 0 ||
                    inRange.compare(heldNow.size(), preeditNow.size(), preeditNow) != 0)
                    fail("event " + std::to_string(i) + ": composition \"" + inRange + "\" != held \"" + heldNow + "\" + preedit \"" + preeditNow +
                         "\"");
            }
            if (docNow.size() != cumulativeCommits.size() + preeditNow.size() ||
                docNow.compare(0, cumulativeCommits.size(), cumulativeCommits) != 0 ||
                docNow.compare(cumulativeCommits.size(), preeditNow.size(), preeditNow) != 0)
                fail("committed text after event " + std::to_string(i) + ": document \"" + docNow + "\" != cumulative \"" + cumulativeCommits +
                     "\" + preedit \"" + preeditNow + "\"");
        } else if (cumulativeCommits.size() != heldNow.size() + docNow.size() || cumulativeCommits.compare(0, docNow.size(), docNow) != 0 ||
                   cumulativeCommits.compare(docNow.size(), heldNow.size(), heldNow) != 0)
            fail("committed text after event " + std::to_string(i) + ": document \"" + docNow + "\" + held \"" + heldNow +
                 "\" != tier A's cumulative \"" + cumulativeCommits + "\"");

        std::cout << "{\"in\": {\"vk\": " << vk << ", \"scan\": " << scan << ", \"mods\": " << mods << "}, \"commit\": \"" << jsonEscape(commit)
                  << "\", \"preedit\": \"" << jsonEscape(preedit) << "\", \"consumed\": " << (eat ? "true" : "false") << "}" << std::endl;
    }

    endRun();
    return 0;
}
