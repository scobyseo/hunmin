/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Test-harness compatibility for the fcitx5 D1/D2/C runners (issue #322).
 *
 * Lowering the addon's floor to 5.1.7 (noble's distro fcitx5) made the ADDON
 * compile against it, but the harness did not: two of the symbols it drives an
 * Instance with are newer than 5.1.7. Both were pinned by reading the upstream
 * headers across tags -- not guessed, and not inferred from a version-number
 * smell:
 *
 *   Instance::eventDispatcher()          @since 5.1.9
 *   setupTestingEnvironmentPath (path)   @since 5.1.13
 *
 * The addon never calls either (they are test scaffolding), so this lives in
 * test/ and guards on the compile definitions frontends/fcitx5/CMakeLists.txt
 * derives from Fcitx5Core_VERSION. On 5.1.19+ the shim is a single passthrough;
 * there is one code path per branch, and a lane that falls through silently is
 * the "green having measured nothing" shape this repository records.
 */
#ifndef HUNMIN_FCITX_TEST_HARNESS_COMPAT_H
#define HUNMIN_FCITX_TEST_HARNESS_COMPAT_H

#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/testing.h>
#include <fcitx/instance.h>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace hunmin_test {

/* The dispatcher a runner schedules its body on.
 *
 * 5.1.9+ exposes Instance::eventDispatcher() already attached to the instance's
 * loop. Below it the Instance has no such accessor, and the harness owns the
 * dispatcher and attaches it to instance->eventLoop() itself -- exactly what
 * fcitx5 5.1.7's own testinstance.cpp does (dispatcher.attach(&instance
 * .eventLoop())). attach() must run in the event loop's thread and these
 * runners are single-threaded, so a function-local static is safe: it is
 * created, attached and used on the one thread that then runs exec(). */
inline fcitx::EventDispatcher &dispatcher(fcitx::Instance &instance) {
#if defined(HUNMIN_FCITX_INSTANCE_DISPATCHER)
    return instance.eventDispatcher();
#else
    static fcitx::EventDispatcher d;
    static bool attached = false;
    if (!attached) {
        d.attach(&instance.eventLoop());
        attached = true;
    }
    return d;
#endif
}

inline fcitx::EventDispatcher &dispatcher(fcitx::Instance *instance) {
    return dispatcher(*instance);
}

/* Point fcitx5 at the build tree's addon/data dirs.
 *
 * 5.1.13+ has the std::filesystem overload; below it the std::string overload
 * is the same contract (both forward to the same Private setup). The runner's
 * arguments are const char* macros, so both branches take them as such and
 * convert. */
inline void setupTestingEnvironmentPath(const std::string &testBinaryDir,
                                        std::initializer_list<const char *> addonDirs,
                                        std::initializer_list<const char *> dataDirs) {
#if defined(HUNMIN_FCITX_TESTING_ENV_PATH)
    std::vector<std::filesystem::path> addons, datas;
    for (const char *p : addonDirs) {
        addons.emplace_back(p);
    }
    for (const char *p : dataDirs) {
        datas.emplace_back(p);
    }
    fcitx::setupTestingEnvironmentPath(testBinaryDir, addons, datas);
#else
    std::vector<std::string> addons(addonDirs.begin(), addonDirs.end());
    std::vector<std::string> datas(dataDirs.begin(), dataDirs.end());
    fcitx::setupTestingEnvironment(testBinaryDir, addons, datas);
#endif
}

} // namespace hunmin_test

#endif // HUNMIN_FCITX_TEST_HARNESS_COMPAT_H