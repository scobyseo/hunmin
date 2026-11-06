/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * production-store-guard.h - put the user's settings store back after a test.
 *
 * A test that drives a path which WRITES the production store
 * (HKCU\Software\Hunmin\TSF) must leave it as it found it: the settings
 * dialog's Apply (settingsdialog.cpp) and the AUTOSWITCH chord's
 * CTextService::SpecialPerform (keydispatch.cpp) both save to
 * HKEY_CURRENT_USER by design, because that is the real user's path.
 *
 * Measured 2026-11-06: TsfConfigTest.TheHanEngKeyCarriesTheSpecialFunctionOnAChord
 * flipped Autoswitch on every run. The win32 conformance corpus replays with
 * whatever that store says, so tier-F1's dubeolsik-seosi alternated
 * 100 % / 96 % run to run -- the runner never pinned the seam (trace-emit does
 * now) and the store was the toggle.
 *
 * The snapshot is the RAW registry values, deliberately not
 * hunmin_tsf_config_load(): the loader lets a non-empty HUNMIN_TSF_* env var
 * win over the registry, and these tests set those vars, so a load-derived
 * snapshot would write the ENV's values into the registry on restore. A value
 * that did not exist is deleted, and a key that did not exist is removed
 * whole, so a host without the store is left without it.
 */
#pragma once

#include <windows.h>

#include <string>

#include "../src/tsf.h" /* HUNMIN_TSF_CONFIG_ROOT_RELPATH, the value names */

namespace tsfmock {

class ProductionStoreSnapshot {
  public:
    ProductionStoreSnapshot() { Take(); }
    ~ProductionStoreSnapshot() { Restore(); }
    ProductionStoreSnapshot(const ProductionStoreSnapshot &) = delete;
    ProductionStoreSnapshot &operator=(const ProductionStoreSnapshot &) = delete;

    /* Idempotent: the fixture calls it from TearDown, the destructor is the
     * backstop for a test that fails between the two. */
    void Restore() {
        if (!taken_)
            return;
        taken_ = false;
        if (!existed_) {
            RegDeleteTreeA(HKEY_CURRENT_USER, HUNMIN_TSF_CONFIG_ROOT_RELPATH);
            return;
        }
        HKEY key = nullptr;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, HUNMIN_TSF_CONFIG_ROOT_RELPATH, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) !=
            ERROR_SUCCESS)
            return;
        for (size_t i = 0; i < kCount; ++i) {
            if (present_[i])
                RegSetValueExA(key, kNames[i], 0, REG_SZ, (const BYTE *)values_[i].data(), (DWORD)values_[i].size());
            else
                RegDeleteValueA(key, kNames[i]);
        }
        RegCloseKey(key);
    }

  private:
    static constexpr size_t kCount = 6;

    void Take() {
        taken_ = true;
        HKEY key = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, HUNMIN_TSF_CONFIG_ROOT_RELPATH, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            existed_ = false;
            return;
        }
        existed_ = true;
        for (size_t i = 0; i < kCount; ++i) {
            DWORD size = 0;
            DWORD type = 0;
            present_[i] = false;
            /* Two-pass read so a long value is never truncated: the first call
             * sizes it, the second fills it. `size` counts the terminating NUL
             * for REG_SZ, and it is kept so the restore writes it back. */
            if (RegQueryValueExA(key, kNames[i], nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ)
                continue;
            values_[i].resize(size);
            if (RegQueryValueExA(key, kNames[i], nullptr, &type, (BYTE *)values_[i].data(), &size) == ERROR_SUCCESS && type == REG_SZ) {
                values_[i].resize(size);
                present_[i] = true;
            }
        }
        RegCloseKey(key);
    }

    static constexpr const char *kNames[kCount] = {HUNMIN_TSF_REGVAL_HANGUL, HUNMIN_TSF_REGVAL_LATIN,  HUNMIN_TSF_REGVAL_TOGGLE,
                                                   HUNMIN_TSF_REGVAL_HANJAKEY, HUNMIN_TSF_REGVAL_COMMITUNIT, HUNMIN_TSF_REGVAL_AUTOSWITCH};

    bool taken_ = false;
    bool existed_ = false;
    bool present_[kCount] = {};
    std::string values_[kCount];
};

} // namespace tsfmock