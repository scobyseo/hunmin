/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D2: what the adapter presents to the UI.
 *
 * D1 asserts commits (what reaches the client buffer). D2 asserts the other
 * half of the contract: the InputPanel preedit the addon publishes, read
 * back through the same accessors a UI addon would use. No pixels, no fcitx5
 * UI fake — 5.1.19 ships none that can be asserted against
 * (docs/TESTING.md D2; see the correction note there).
 *
 * Both preedit branches of HunminState::updateUI() are covered:
 *   - contexts WITHOUT CapabilityFlag::Preedit  -> inputPanel().preedit()
 *   - contexts WITH    CapabilityFlag::Preedit  -> inputPanel().clientPreedit()
 * testfrontend's IC carries no capability flags, so it exercises the first;
 * the second is forced here via setCapabilityFlags().
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <filesystem>
#include <system_error>
#include <cstdlib>

using namespace fcitx;

static Instance *g_instance;

// Press a key without asserting acceptance — preedit tests care about the
// panel, and pass-through keys (space) legitimately return false.
static void tap(ICUUID uuid, const Key &key) {
    auto *tf = g_instance->addonManager().addon("testfrontend");
    FCITX_ASSERT(tf);
    tf->call<ITestFrontend::sendKeyEvent>(uuid, key, false);
}

// The server-side preedit: what the adapter sets when the context does NOT
// advertise CapabilityFlag::Preedit.
static const Text &panelOf(InputContext *ic) {
    FCITX_ASSERT(!ic->capabilityFlags().test(CapabilityFlag::Preedit));
    return ic->inputPanel().preedit();
}

// Same accessor, without the precondition -- used to prove the *other*
// branch is not also being written.
static const std::string &otherPanelOf(InputContext *ic) {
    static std::string s;
    s = ic->inputPanel().preedit().toString();
    return s;
}

static const Text &clientPanelOf(InputContext *ic) {
    FCITX_ASSERT(ic->capabilityFlags().test(CapabilityFlag::Preedit));
    return ic->inputPanel().clientPreedit();
}

/* Return the valueByPath prefix under which the Layout option's description
 * lives, walking the dump to find it.
 *
 * Finding the prefix by walking rather than hardcoding one is the point: a
 * wrong prefix makes every count below zero, and zero rows still satisfies a
 * "<= 12" style check, so the test would pass while asserting nothing. */
static std::string walkDump(const fcitx::RawConfig &c, const std::string &path,
                            std::string *layoutPrefix) {
    for (auto name : c.subItems()) {
        auto sub = c.get(name);
        if (!sub) {
            continue;
        }
        const std::string here =
            path.empty() ? name : path + "/" + std::string(name);
        if (name == "Layout") {
            *layoutPrefix = here;
        }
        walkDump(*sub, here, layoutPrefix);
    }
    return {};
}

void scheduleEvent(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        FCITX_ASSERT(instance->addonManager().addon("hunmin", true));

        auto group = instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(
            InputMethodGroupItem("hunmin").setLayout("sin3-p2"));
        group.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(group);

        auto *tf = instance->addonManager().addon("testfrontend");
        auto uuid = tf->call<ITestFrontend::createInputContext>("d2app");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(tf->call<ITestFrontend::sendKeyEvent>(uuid, Key("Control+space"), false));
        FCITX_ASSERT(instance->inputMethod(ic) == "hunmin");

        // NB: InputPanel::empty() also covers auxUp/auxDown, and fcitx5 fills
        // auxUp with the IM label ("한") from the label() in
        // inputmethod/hunmin.conf. So emptiness is asserted on the preedit
        // accessors, not on empty().

        // --- composing: each keystroke republishes the whole composition.
        // The panel shows the engine's preedit (한글), not the key that
        // produced it. Expectations are hunmin-sim --trace for sin3-p2 on
        // "k f k space", so tier A stays the authority (docs/TESTING.md).
        tap(uuid, Key("k"));
        FCITX_ASSERT(panelOf(ic).toString() == "\u3131");
        tap(uuid, Key("f"));
        FCITX_ASSERT(panelOf(ic).toString() == "\uac00");
        // the third key commits the syllable and starts a fresh composition
        tf->call<ITestFrontend::pushCommitExpectation>("\uac00");
        tap(uuid, Key("k"));
        FCITX_ASSERT(panelOf(ic).toString() == "\u3131");

        // --- space flushes: commit goes out and the panel is cleared
        tf->call<ITestFrontend::pushCommitExpectation>("\u3131");
        tap(uuid, Key("space"));
        FCITX_ASSERT(panelOf(ic).toString().empty());

        // --- a rejected key must also clear the panel rather than leave a
        // stale composition visible after the client consumed it
        tap(uuid, Key("k"));
        FCITX_ASSERT(panelOf(ic).toString() == "\u3131");
        tap(uuid, Key("BackSpace"));
        FCITX_ASSERT(panelOf(ic).toString().empty());

        // --- the client-preedit branch: same content, different accessor
        ic->setCapabilityFlags(CapabilityFlags(CapabilityFlag::Preedit));
        tap(uuid, Key("k"));
        tap(uuid, Key("f"));
        FCITX_ASSERT(clientPanelOf(ic).toString() == "\uac00");
        FCITX_ASSERT(otherPanelOf(ic).empty()) << "must not populate both";
        tf->call<ITestFrontend::pushCommitExpectation>("\uac00");
        tap(uuid, Key("space"));
        FCITX_ASSERT(clientPanelOf(ic).toString().empty());

        // --- cursor is at the end of the preedit, which is what a UI needs
        // to place the caret
        ic->setCapabilityFlags(CapabilityFlags());
        tap(uuid, Key("k"));
        tap(uuid, Key("f"));
        const Text &t = panelOf(ic);
        FCITX_ASSERT(t.toString() == "\uac00");
        // cursor() is a byte offset; textLength() is the byte length.
                FCITX_ASSERT(t.cursor() == static_cast<int>(t.textLength()))
                    << "cursor " << t.cursor() << " vs textLength " << t.textLength();
                tf->call<ITestFrontend::pushCommitExpectation>("가");
                tap(uuid, Key("space"));
                FCITX_ASSERT(panelOf(ic).toString().empty());

                // --- F9 opens the Hanja window for the current preedit (P3).
        // The whole reason the adapter consumes keys the engine declined: F9
        // is outside printable ASCII, so hunmin_ic_process() never sees it
        // (FRONTEND-CONTRACT.md A11).
        //
        // 푸 is chosen because it is in hanja/ and in neither krdict nor
        // idioms, so every candidate is a bare syllable: one Hanja each, and
        // the comment carries the kind because there is no gloss to show.
        // \uac00 would also carry a krdict gloss and lead the list with it.
        tap(uuid, Key("p"));
        tap(uuid, Key("o"));
        FCITX_ASSERT(panelOf(ic).toString() == "\ud478");

        bool accepted = tf->call<ITestFrontend::sendKeyEvent>(
            uuid, Key(FcitxKey_F9), false);
        FCITX_ASSERT(accepted) << "F9 with a candidate preedit must be consumed";

        auto list = ic->inputPanel().candidateList();
        FCITX_ASSERT(list) << "F9 produced no candidate list";
        FCITX_ASSERT(list->size() > 0) << "candidate list is empty for \ud478";
        FCITX_ASSERT(list->candidate(0).comment().toString() == "hanja")
            << "kind label is '" << list->candidate(0).comment().toString() << "'";
        // 푸 has exactly one candidate in hanja.json, U+4B0C, and no
        // frequency data -- asserted from the file rather than remembered.
        FCITX_ASSERT(list->size() == 1) << list->size();
        FCITX_ASSERT(list->candidate(0).text().toString() == "\u4b0c")
            << list->candidate(0).text().toString();

        // Choosing a candidate commits the Hanja in place of the preedit it
        // came from. DisplayOnlyCandidateWord would silently drop it.
        // 푸's only candidate is U+4B0C, per hanja.json.
        tf->call<ITestFrontend::pushCommitExpectation>("\u4b0c");
        list->candidate(0).select(ic);
        FCITX_ASSERT(!ic->inputPanel().candidateList())
            << "choosing did not close the window";
        FCITX_ASSERT(panelOf(ic).toString().empty())
            << "choosing left the source Hangul in the preedit";

        // Re-open and close with F9 instead, which must not disturb anything.
        tap(uuid, Key("p"));
        tap(uuid, Key("o"));
        FCITX_ASSERT(panelOf(ic).toString() == "\ud478");
        accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_F9),
                                                         false);
        // First F9 re-opens, second closes: neither disturbs the preedit.
        FCITX_ASSERT(accepted);
        FCITX_ASSERT(ic->inputPanel().candidateList())
            << "the second F9 did not re-open the window";
        accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_F9),
                                                         false);
        FCITX_ASSERT(accepted);
        FCITX_ASSERT(!ic->inputPanel().candidateList())
            << "F9 did not close the candidate window";
        FCITX_ASSERT(panelOf(ic).toString() == "\ud478")
            << "closing the window disturbed the preedit";

        // ... and the pass-through case: a lone initial has no entry, so F9 is
        // not ours to consume and must reach the client.
        // 푸 is two jamo, so backspace peels them off one at a time.
        tap(uuid, Key("BackSpace"));
        FCITX_ASSERT(panelOf(ic).toString() == "\u314d");
        tap(uuid, Key("BackSpace"));
        FCITX_ASSERT(panelOf(ic).toString().empty());
        tap(uuid, Key("k"));
        FCITX_ASSERT(panelOf(ic).toString() == "\u3131");
        accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_F9),
                                                         false);
        FCITX_ASSERT(!accepted) << "F9 with no candidates must reach the client";

        // ... and with no composition at all. Distinct from the case above:
        // there the preedit holds a syllable that simply has no entry, here
        // it is empty, and neither may swallow F9.
        accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, Key(FcitxKey_F9),
                                                         false);
        FCITX_ASSERT(!accepted) << "F9 with an empty preedit must reach the client";
        FCITX_ASSERT(!ic->inputPanel().candidateList())
            << "a pass-through F9 must not open a window";
        /* Known gap: openCandidates() also early-returns on an *empty*
         * preedit, before it ever calls hunmin_suggest_count(). Dropping that
         * guard still lets the empty string resolve to zero candidates, so
         * both paths pass this test and the mutation survives. The guard is
         * kept because relying on the lookup to reject "" is an accident of
         * the API rather than a stated contract -- but it is not verified
         * behaviour, and this is where to look if that ever changes. */

        tap(uuid, Key("BackSpace"));
        FCITX_ASSERT(panelOf(ic).toString().empty());

        // --- the layout picker offers every layout, as a combobox.
        //
        // This asserts on the description dump configtool actually receives
        // (Option::dumpDescription -> Enum/n rows), not on the fact that the
        // annotation type was chosen. An EnumAnnotation whose list was never
        // filled still renders as a combobox -- an empty one -- so asserting
        // the widget type proves nothing; asserting the rows do.
        {
            auto *hunmin = instance->addonManager().addon("hunmin");
            FCITX_ASSERT(hunmin);
            auto *engine = dynamic_cast<InputMethodEngine *>(hunmin);
            FCITX_ASSERT(engine);
            fcitx::RawConfig desc;
            engine->getConfig()->dumpDescription(desc);

            // The dump is nested -- each config under its typeName(), each option under
            // its own path -- and the two are joined with '$' while the
            // separator inside a path is '/' (configuration.cpp:33-51,
            // rawconfig.cpp:66-73). Walking to find the prefix beats asserting
            // against a guessed one: a wrong prefix yields zero rows, and zero
            // rows still satisfies every count below, so the test would pass
            // while asserting nothing.
            std::string base;
            walkDump(desc, "", &base);
            FCITX_ASSERT(!base.empty())
                << "no Layout option in the description dump";

            // The VALUE, not just the key. `valueByPath` returns a pointer for
            // any existing key, so asserting presence passed with IsEnum set to
            // "False" -- measured, by planting exactly that. configtool branches
            // on the value (optionwidget.cpp:546), so only "True" gives the
            // combobox.
            const std::string *isEnum = desc.valueByPath(base + "/IsEnum");
            FCITX_ASSERT(isEnum && *isEnum == "True")
                << "Layout IsEnum is "
                << (isEnum ? *isEnum : std::string("(absent)"))
                << ", so configtool renders a text field and the user has to "
                   "type the id";

            // Every layout the addon offers must be one the data ships, and
            // every layout the data ships that this build can open must be
            // offered. Both counts are taken from the data directory on disk
            // rather than a hardcoded 12, so adding a layout file cannot leave
            // this passing on a stale number.
            //
            // Checked against the data rather than through the core API: this
            // binary dlopen()s the addon and does not link libhunmin (see the
            // target_link_libraries line -- I10 is about exactly this), so
            // hunmin_ctx_* is not available here. Reading the files is also the
            // stronger check: it catches a row whose id is real but whose file
            // is gone.
            int shipped = 0;
            {
                std::error_code ec;
                for (auto it = std::filesystem::directory_iterator(
                         std::string(TESTING_SOURCE_DIR) + "/libhunmin/data/layouts",
                         ec);
                     !ec && it != std::filesystem::directory_iterator(); ++it) {
                    if (it->path().extension() == ".json") {
                        shipped++;
                    }
                }
                FCITX_ASSERT(!ec) << "cannot read the layouts directory: " << ec;
            }
            FCITX_ASSERT(shipped > 0) << "no layout files found";

            int rows = 0;
            for (int i = 0;; i++) {
                if (!desc.valueByPath(base + "/Enum/" + std::to_string(i))) {
                    break;
                }
                rows++;
            }
            // Equality, not ">=": the picker must offer exactly the layouts the
            // data ships. A row the core cannot open is a row that fails at
            // keystroke time, and a missing one is a layout the user cannot
            // pick.
            FCITX_ASSERT(rows == shipped)
                << "picker offers " << rows << " layouts, data ships " << shipped;

            // Known gap, recorded rather than papered over: dropping
            // refreshLayoutList() from reloadConfig() survives this test.
            // reloadConfig() is an addon virtual that only fcitx5 itself calls,
            // over D-Bus, when the config is reloaded -- a testfrontend instance
            // never does that, so the refresh-on-reload behaviour is not covered
            // here. It is not observable from this tier at all; D3 with a real
            // configtool would be the place, and D3 does not run one. If a
            // layout is ever added while fcitx5 is running, that is the gap
            // that would let the picker go stale.

            // And every id offered has a file behind it -- a stale row would
            // otherwise survive the count.
            for (int i = 0; i < rows; i++) {
                const std::string *id =
                    desc.valueByPath(base + "/Enum/" + std::to_string(i));
                const std::string file = std::string(TESTING_SOURCE_DIR) +
                                         "/libhunmin/data/layouts/" + *id +
                                         ".json";
                std::error_code ec;
                FCITX_ASSERT(std::filesystem::exists(file, ec))
                    << "picker offers '" << *id << "', which has no data file";
            }
        }

        instance->deactivate();
    });

    instance->eventDispatcher().schedule([instance]() { instance->exit(); });
}

int main() {
    setenv("HUNMIN_DATA_PATH", TESTING_SOURCE_DIR "/libhunmin/data", 1);
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {TEST_ADDON_DIR},
                                {TEST_DATA_DIR});
    char arg0[] = "testhunminui";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,hunmin";
    char *argv[] = {arg0, arg1, arg2};
    fcitx::Log::setLogRule("default=5,hunmin=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    g_instance = &instance;
    scheduleEvent(&instance);
    instance.exec();
    return 0;
}