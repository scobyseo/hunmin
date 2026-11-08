// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef HUNMIN_TSF_TEST_TSF_ENV_HELPERS_H
#define HUNMIN_TSF_TEST_TSF_ENV_HELPERS_H
/*
 * Additive TSF test helpers (#272 P4). Not wired into existing tests yet.
 * Purpose: centralize common env/keystate setup patterns observed in TSF gtest
 * fixtures without changing test semantics. Migrate call sites only when the
 * TSF test tier is runnable and behavior can be verified unchanged.
 */
#ifdef _WIN32
#include <stdlib.h>

#include "../src/keybinding.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"

namespace hunmin_tsf_test {

inline void SetDefaultDataDirEnv() {
    _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
}

inline void SetLayoutEnv(const char *hangul, const char *latin) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, hangul ? hangul : "");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, latin ? latin : "");
}

inline void SetToggleKeysEnv(const char *keys) {
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, keys ? keys : "");
}

inline void ClearLayoutAndToggleEnv() {
    SetLayoutEnv("", "");
    SetToggleKeysEnv("");
}

inline void RegisterKeystateMock() {
    tsf_test_register_keystate();
}

} // namespace hunmin_tsf_test
#endif // _WIN32
#endif
