/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1: headless fcitx5 Instance + TestFrontend — key routing,
 * commit strings, caps handling (docs/TESTING.md).
 */
#include "testdir.h"
#include "testfrontend_public.h"
#include "../src/config.h"
#include <cstdlib>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/instance.h>

using namespace fcitx;

static Instance *g_instance;

static void press(ICUUID uuid, const Key &key, bool expectAccepted = true) {
    auto *tf = g_instance->addonManager().addon("testfrontend");
    FCITX_ASSERT(tf);
    bool accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, key, false);
    FCITX_ASSERT(accepted == expectAccepted) << "key " << key.toString() << " accepted=" << accepted;
}

void scheduleEvent(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *hunmin = instance->addonManager().addon("hunmin", true);
        FCITX_ASSERT(hunmin);

        auto defaultGroup = instance->inputMethodManager().currentGroup();
        defaultGroup.inputMethodList().clear();
        defaultGroup.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        // Per-item Layout is what a real user's profile carries, and it must
        // override the addon-wide default (sin3-p2). Pins it to sin3-p2-yet so
        // the caps option below is actually consumed -- see
        // docs/FRONTEND-CONTRACT.md A3/§4; hunminadapter.cpp reads it via
        // InputMethodGroup::layoutFor().
        defaultGroup.inputMethodList().push_back(InputMethodGroupItem("hunmin").setLayout("sin3-p2-yet"));
        defaultGroup.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(defaultGroup);

        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid = testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(testfrontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("Control+space"), false));
        FCITX_ASSERT(instance->inputMethod(ic) == "hunmin");

        // 가: k (cho ㄱ) f (ㅏ) compose; space flushes the syllable
        testfrontend->call<ITestFrontend::pushCommitExpectation>("가");
        press(uuid, Key("k"));
        press(uuid, Key("f"));
        press(uuid, Key("space"), false);

        // 초성체 pair on a fresh buffer: c leaves ㄱ composing, Q -> ㄳ,
        // space flushes
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㄳ");
        press(uuid, Key("c"));
        press(uuid, Key("Q"));
        press(uuid, Key("space"), false);

        // symbol tier (sin3-p2): j opens, k swallowed as tier select,
        // 1 produces ㅇµ -- which, under the shared layer's buffered commit
        // (issue #128 / #74), is held as preedit until the word boundary.
        // The text is unchanged; only the point it reaches the client moved,
        // and that is explicitly not part of the contract
        // (docs/FRONTEND-EQUIVALENCE.md §1). Space is the boundary and passes.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㅇµ");
        press(uuid, Key("j"));
        press(uuid, Key("k"));
        press(uuid, Key("1"));
        press(uuid, Key("space"), false);

        // Empty buffer: space and backspace pass through to the client. The engine
        // declines both with nothing composing, and an unaccepted key is not
        // consumed (FRONTEND-CONTRACT.md 5, rule 3) -- that is how the client
        // gets them. F9 needs no exception: it is intercepted above, on
        // candidates, before it ever reaches the engine.
        press(uuid, Key("space"), false);
        press(uuid, Key("BackSpace"), false);

        // CapsLock: the adapter lowercases the shifted letter and hands the
        // engine caps=1. On sin3-p2-yet that BLOCKS the symbol tier (j k stays
        // choseong jamo), without it the tier opens (j k 1 -> ㅇµ). Replayed
        // from tests/golden/p2-2-4-3-caps.jsonl (cases caps-tier-blocked /
        // caps-tier-open), which is the authority for these expectations.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ᅁᅠ1");
        press(uuid, Key(FcitxKey_J, KeyState::CapsLock));
        press(uuid, Key(FcitxKey_K, KeyState::CapsLock));
        press(uuid, Key(FcitxKey_1, KeyState::CapsLock));
        press(uuid, Key("space"), false); // the boundary the buffered commit waits for

        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㅇµ");
        press(uuid, Key("j"));
        press(uuid, Key("k"));
        press(uuid, Key("1"));
        press(uuid, Key("space"), false);

        // --- the input unit is the USER's choice (config CommitBehavior, issue
        // #128): both policies exist in the shared layer and are selectable,
        // instead of one being baked into the adapter. Set through the addon's
        // own config surface, which is the path a settings UI takes.
        //
        // The committed TEXT is identical under both policies; what differs is
        // when it reaches the client -- so these two blocks type the same keys
        // and assert different delivery points for the same string.
        {
            RawConfig raw;
            raw.setValueByPath("HangulLayout", "sin3-p2");
            raw.setValueByPath("LatinLayout", "us-qwerty");
            raw.setValueByPath("CommitBehavior", "syllable");
            hunmin->setConfig(raw); // drops the in-flight composition
        }
        // syllable: the third key finalises 가 and delivers it immediately
        testfrontend->call<ITestFrontend::pushCommitExpectation>("가");
        press(uuid, Key("k"));
        press(uuid, Key("f"));
        press(uuid, Key("k"));
        // ...and the boundary then delivers the ㅅ-less second syllable
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㄱ");
        press(uuid, Key("space"), false);

        {
            RawConfig raw;
            raw.setValueByPath("HangulLayout", "sin3-p2");
            raw.setValueByPath("LatinLayout", "us-qwerty");
            raw.setValueByPath("CommitBehavior", "word");
            hunmin->setConfig(raw);
        }
        // word: nothing lands until the boundary, then the whole word in one
        // insert -- the same two syllables, one commit
        testfrontend->call<ITestFrontend::pushCommitExpectation>("가ㄱ");
        press(uuid, Key("k"));
        press(uuid, Key("f"));
        press(uuid, Key("k"));
        press(uuid, Key("space"), false);

        // --- the automatic-switch ROW (issue #176, docs/SPEC-MODES.md §R13-정정)
        //
        // Surface B is visibility + discoverability of ONE engine-owned value, so
        // what is asserted here is the issue's checklist: the row names the key
        // the ENGINE reports, the value the dialog writes is the one the engine
        // holds, a rebuilt layer re-applies it instead of falling back to the
        // engine's OFF default, and the row explains itself when the data is
        // missing.
        //
        // The block runs on dubeolsik, and that is a measurement rather than a
        // preference: R8 keeps the mode when BOTH readings are valid, and on
        // sin3-p2 (this test's per-item layout above) `hello` composes 내쭈 -- a
        // valid syllable run -- so no judgement fires. On dubeolsik it composes
        // ㅗ디ㅣㅣㅐ, which is not, and the English reading is `hello`: the vector
        // wordset-test pins as "switch". The per-item Layout= is cleared so the
        // addon falls back to the config, which is the surface this block drives.
        {
            auto group = instance->inputMethodManager().currentGroup();
            group.inputMethodList()[1].setLayout("");
            instance->inputMethodManager().setGroup(group);
            RawConfig raw;
            raw.setValueByPath("HangulLayout", "dubeolsik");
            hunmin->setConfig(raw);
        }

        // The description is what fcitx5's configtool draws, so reading it back
        // out of dumpDescription() is reading the row itself, not a proxy.
        {
            RawConfig desc;
            hunmin->getConfig()->dumpDescription(desc);
            auto dumped = [&desc](const char *path) {
                const std::string *v = desc.valueByPath(path);
                return v ? *v : std::string();
            };
            const std::string row = dumped("HunminConfig/AutoSwitch/Description");
            const std::string tip = dumped("HunminConfig/AutoSwitch/ToolTip");
            // Table A (2026-10-31) is AUTOSWITCH = Ctrl. The adapter asks the
            // engine for that number (hunmin_buffered_preedit_special_chord) and
            // the platform supplies only its own key's name, so this pins the
            // whole string the dialog draws.
            FCITX_ASSERT(row.find("Ctrl") != std::string::npos) << "row: " << row;
            FCITX_ASSERT(row.find("한/영") != std::string::npos) << "row: " << row;
            // Discoverability is the tooltip's job: the row has to teach that the
            // same state is reachable by key (R13's own wording).
            FCITX_ASSERT(tip.find("한/영") != std::string::npos) << "tip: " << tip;
            // The word list is installed in this tree, so the row must not call
            // itself unavailable.
            FCITX_ASSERT(row.find("사전 없음") == std::string::npos) << "row: " << row;
        }

        // The data-missing half cannot be produced through the addon here -- the
        // context resolved its data at construction and caches the answer -- so
        // the row's unavailable text is pinned directly. It is the string the
        // dialog draws, and "the row explains itself" is that string existing
        // (issue #176's third checklist item).
        {
            HunminConfig bare;
            RawConfig desc;
            bare.dumpDescription(desc);
            const std::string *off = desc.valueByPath("HunminConfig/AutoSwitch/DefaultValue");
            // OFF is B3's ruling of 2026-10-11, and the item's default has to be
            // that value rather than a checked box nobody turned on.
            FCITX_ASSERT(off && *off == "False") << "default: " << (off ? *off : std::string("(absent)"));

            bare.autoswitch.annotation().setDataPresent(false);
            RawConfig missing;
            bare.dumpDescription(missing);
            const std::string *row = missing.valueByPath("HunminConfig/AutoSwitch/Description");
            const std::string *tip = missing.valueByPath("HunminConfig/AutoSwitch/ToolTip");
            FCITX_ASSERT(row && row->find("사전 없음") != std::string::npos) << "row: " << (row ? *row : std::string("(absent)"));
            FCITX_ASSERT(tip && tip->find("설치") != std::string::npos) << "tip: " << (tip ? *tip : std::string("(absent)"));
        }

        // The dialog's tick -> the engine. setConfig() is the path the settings UI
        // takes, and what it stores is the engine's answer: ON is accepted because
        // the word list is installed here.
        {
            RawConfig raw;
            raw.setValueByPath("AutoSwitch", "True");
            hunmin->setConfig(raw); // partial load: the other three keys stay
            const auto *cfg = static_cast<const HunminConfig *>(hunmin->getConfig());
            FCITX_ASSERT(cfg->autoswitch.value()) << "stored OFF after writing ON";
        }

        // ...and the value really reached the LAYER, which is the half a config
        // round trip alone cannot show. `hello` is an English word and its Hangul
        // reading is not a syllable run, so R8 delivers the English reading and
        // the mode follows it (docs/SPEC-MODES.md §R8/R10, issue #164).
        testfrontend->call<ITestFrontend::pushCommitExpectation>("hello");
        press(uuid, Key("h"));
        press(uuid, Key("e"));
        press(uuid, Key("l"));
        press(uuid, Key("l"));
        press(uuid, Key("o"));
        press(uuid, Key("space"), false);
        // The next key is typed in the Latin half -- the switch happened.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("a");
        press(uuid, Key("a"));
        press(uuid, Key("space"), false);

        // A rebuilt layer starts from the engine's default OFF, so the stored
        // value has to be re-applied (the fourth checklist item). Two config
        // applies force the rebuild and leave AutoSwitch alone.
        {
            RawConfig raw;
            raw.setValueByPath("CommitBehavior", "syllable");
            hunmin->setConfig(raw);
            RawConfig back;
            back.setValueByPath("CommitBehavior", "word");
            hunmin->setConfig(back);
        }
        // The same word again is the discriminator: with the value still in force
        // it is judged and delivered as `hello`, and with it lost the fresh layer
        // would deliver its Hangul reading instead.
        testfrontend->call<ITestFrontend::pushCommitExpectation>("hello");
        press(uuid, Key("h"));
        press(uuid, Key("e"));
        press(uuid, Key("l"));
        press(uuid, Key("l"));
        press(uuid, Key("o"));
        press(uuid, Key("space"), false);

        // Route 2 (PreInputMethod watcher): Ctrl+Hangul is claimed by the
        // watcher while hunmin is the active IM, and the engine's special
        // host services the AUTOSWITCH chord. The event is accepted (does not
        // reach the IM phase), and the stored value flips.
        {
            RawConfig raw;
            raw.setValueByPath("AutoSwitch", "True");
            hunmin->setConfig(raw);
            const auto *cfg = static_cast<const HunminConfig *>(hunmin->getConfig());
            FCITX_ASSERT(cfg->autoswitch.value()) << "autoswitch ON before watcher";
            press(uuid, Key(FcitxKey_Hangul, KeyState::Ctrl));
            // The watcher accepted the chord; the engine flipped the layer.
            FCITX_ASSERT(!cfg->autoswitch.value()) << "autoswitch still ON after Ctrl+Hangul";

            // The chord-flipped value must survive a config reload.
            hunmin->reloadConfig();
            FCITX_ASSERT(!cfg->autoswitch.value()) << "autoswitch reverted after reload";
        }

        instance->deactivate();
    });

    instance->eventDispatcher().schedule([instance]() { instance->exit(); });
}

int main() {
    setenv("HUNMIN_DATA_PATH", TESTING_SOURCE_DIR "/libhunmin/data", 1);
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {TEST_ADDON_DIR}, {TEST_DATA_DIR});
    char arg0[] = "testhunmin";
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
