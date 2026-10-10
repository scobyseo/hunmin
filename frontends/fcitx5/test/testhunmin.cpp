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
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/instance.h>
#include <cstdlib>

using namespace fcitx;

static Instance *g_instance;

static void press(ICUUID uuid, const Key &key, bool expectAccepted = true) {
    auto *tf = g_instance->addonManager().addon("testfrontend");
    FCITX_ASSERT(tf);
    bool accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, key, false);
    FCITX_ASSERT(accepted == expectAccepted)
        << "key " << key.toString() << " accepted=" << accepted;
}

void scheduleEvent(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *hunmin = instance->addonManager().addon("hunmin", true);
        FCITX_ASSERT(hunmin);

        auto defaultGroup = instance->inputMethodManager().currentGroup();
        defaultGroup.inputMethodList().clear();
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("keyboard-us"));
        // Per-item Layout is what a real user's profile carries, and it must
        // override the addon-wide default (sin3-p2). Pins it to sin3-p2-yet so
        // the caps option below is actually consumed -- see
        // docs/FRONTEND-CONTRACT.md A3/§4; engine.cpp reads it via
        // InputMethodGroup::layoutFor().
        defaultGroup.inputMethodList().push_back(
            InputMethodGroupItem("hunmin").setLayout("sin3-p2-yet"));
        defaultGroup.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(defaultGroup);

        auto *testfrontend = instance->addonManager().addon("testfrontend");
        auto uuid =
            testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(testfrontend->call<ITestFrontend::sendKeyEvent>(
            uuid, Key("Control+space"), false));
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
        // 1 commits ㅇµ
        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㅇµ");
        press(uuid, Key("j"));
        press(uuid, Key("k"));
        press(uuid, Key("1"));

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

        testfrontend->call<ITestFrontend::pushCommitExpectation>("ㅇµ");
        press(uuid, Key("j"));
        press(uuid, Key("k"));
        press(uuid, Key("1"));

        instance->deactivate();
    });

    instance->eventDispatcher().schedule([instance]() { instance->exit(); });
}

int main() {
    setenv("HUNMIN_DATA_PATH", TESTING_SOURCE_DIR "/libhunmin/data", 1);
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {TEST_ADDON_DIR},
                                {TEST_DATA_DIR});
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
