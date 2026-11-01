/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Registry + environment reader/writer for the TSF frontend configuration
 * store (frontends/tsf/src/config.h). Deliberately ctx-free: raw IO only.
 */
#include "config.h"

#include <stdio.h>
#include <string.h>

static void copyStr(const char *src, char *dst, size_t cap) {
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

/* One slot: env first, then the REG_SZ under the store key. Returns 1 when
 * the registry satisfied the slot (the loader's diagnostic count), and never
 * leaves a slot empty — env miss + registry miss = the default. cap is the
 * dst capacity (the slots differ: layout ids 64, the hanja key 32). */
static int loadSlot(HKEY root, const char *envName, const char *regValName, const char *def, char *dst, size_t cap) {
    char envBuf[HUNMIN_TSF_LAYOUT_ID_CAP];
    /* > 0 excludes an empty-valued variable from counting as set: an empty
     * HUNMIN_TSF_* means "no override", the same rule
     * hunmin_ctx_from_module() applies to HUNMIN_DATA_DIR. */
    if (GetEnvironmentVariableA(envName, envBuf, (DWORD)sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
        copyStr(envBuf, dst, cap);
        return 0;
    }
    if (root) {
        HKEY key = nullptr;
        if (RegOpenKeyExA(root, HUNMIN_TSF_CONFIG_ROOT_RELPATH, 0, KEY_READ, &key) == ERROR_SUCCESS) {
            DWORD type = 0;
            BYTE buf[HUNMIN_TSF_LAYOUT_ID_CAP];
            DWORD size = (DWORD)sizeof(buf);
            LONG got = RegQueryValueExA(key, regValName, nullptr, &type, buf, &size);
            RegCloseKey(key);
            /* REG_SZ only, NUL-terminated within the buffer: anything else is
             * treated as absent rather than trusted. */
            if (got == ERROR_SUCCESS && type == REG_SZ && (int)size > 0 && buf[0] != '\0') {
                copyStr((const char *)buf, dst, cap);
                return 1;
            }
        }
    }
    copyStr(def, dst, cap);
    return 0;
}

/* Load the toggle binding list the same way: env wins, then the registry
 * string, then "" (nothing configured). */
static void loadToggleKeys(HKEY root, char *dst) {
    char envBuf[HUNMIN_TSF_TOGGLE_KEYS_CAP];
    if (GetEnvironmentVariableA(HUNMIN_TSF_ENV_TOGGLE_KEYS, envBuf, (DWORD)sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
        copyStr(envBuf, dst, HUNMIN_TSF_TOGGLE_KEYS_CAP);
        return;
    }
    if (root) {
        HKEY key = nullptr;
        if (RegOpenKeyExA(root, HUNMIN_TSF_CONFIG_ROOT_RELPATH, 0, KEY_READ, &key) == ERROR_SUCCESS) {
            DWORD type = 0;
            BYTE buf[HUNMIN_TSF_TOGGLE_KEYS_CAP];
            DWORD size = (DWORD)sizeof(buf);
            LONG got = RegQueryValueExA(key, HUNMIN_TSF_REGVAL_TOGGLE, nullptr, &type, buf, &size);
            RegCloseKey(key);
            if (got == ERROR_SUCCESS && type == REG_SZ && (int)size > 0 && buf[0] != '\0') {
                copyStr((const char *)buf, dst, HUNMIN_TSF_TOGGLE_KEYS_CAP);
                return;
            }
        }
    }
    copyStr("", dst, HUNMIN_TSF_TOGGLE_KEYS_CAP); /* nothing configured: env empty + registry miss */
}

/* A flag slot: the same env -> registry -> default chain, parsed as "1" = on.
 * Anything else (absent, empty, a typo) means OFF, which is the B3 default and
 * the fail-closed direction: a corrupted store must not enable a feature nobody
 * asked for. */
static int loadFlag(HKEY root, const char *envName, const char *regValName) {
    char buf[4];
    loadSlot(root, envName, regValName, "0", buf, sizeof buf);
    return strcmp(buf, "1") == 0 ? 1 : 0;
}

extern "C" int hunmin_tsf_config_load(HKEY root, HunminTsfConfig *out) {
    if (!out)
        return 0;
    /* The fcitx5 defaults (frontends/fcitx5/src/config.h): hangul sin3-p2,
     * latin us-qwerty. Cross-frontend consistency beats a Windows-local
     * choice; the settings dialog lists every data dir layout anyway. */
    int fromReg = loadSlot(root, HUNMIN_TSF_ENV_HANGUL_LAYOUT, HUNMIN_TSF_REGVAL_HANGUL, "sin3-p2", out->hangulLayout, HUNMIN_TSF_LAYOUT_ID_CAP);
    fromReg += loadSlot(root, HUNMIN_TSF_ENV_LATIN_LAYOUT, HUNMIN_TSF_REGVAL_LATIN, "us-qwerty", out->latinLayout, HUNMIN_TSF_LAYOUT_ID_CAP);
    loadToggleKeys(root, out->toggleKeys);
    /* The candidate key (issue #67): the physical Hanja key (VK_HANJA = 0x19
     * = decimal 25, bare) is the default, the same platform-alternate rule
     * the toggle binding carries. */
    fromReg += loadSlot(root, HUNMIN_TSF_ENV_HANJA_KEY, HUNMIN_TSF_REGVAL_HANJAKEY, "v1:25:0", out->hanjaKey, HUNMIN_TSF_HANJA_KEY_CAP);
    /* The commit unit (issue #128). Default "word": TSF's native composition
     * shows the whole word, so that is the unit its preedit already displays.
     * Both units exist in the shared layer and produce the same text. */
    fromReg += loadSlot(root, HUNMIN_TSF_ENV_COMMIT_UNIT, HUNMIN_TSF_REGVAL_COMMITUNIT, "word", out->commitUnit,
                        HUNMIN_TSF_COMMIT_UNIT_CAP);
    /* Anything but the literal "syllable" means the buffered word -- a typo in
     * the registry must not silently change the unit. */
    if (strcmp(out->commitUnit, "syllable") != 0)
        snprintf(out->commitUnit, HUNMIN_TSF_COMMIT_UNIT_CAP, "%s", "word");
    /* R13-정정's ON/OFF (issue #175), written by the settings dialog and read
     * once per Activate/Apply. OFF unless the store says "1" (B3). */
    out->autoswitch = loadFlag(root, HUNMIN_TSF_ENV_AUTOSWITCH, HUNMIN_TSF_REGVAL_AUTOSWITCH);
    return fromReg;
}

extern "C" BOOL hunmin_tsf_config_save(HKEY root, const HunminTsfConfig *cfg) {
    if (!root || !cfg)
        return FALSE;
    if (memchr(cfg->hangulLayout, '\0', HUNMIN_TSF_LAYOUT_ID_CAP) == nullptr || memchr(cfg->latinLayout, '\0', HUNMIN_TSF_LAYOUT_ID_CAP) == nullptr ||
        memchr(cfg->toggleKeys, '\0', HUNMIN_TSF_TOGGLE_KEYS_CAP) == nullptr || memchr(cfg->hanjaKey, '\0', HUNMIN_TSF_HANJA_KEY_CAP) == nullptr ||
        memchr(cfg->commitUnit, '\0', HUNMIN_TSF_COMMIT_UNIT_CAP) == nullptr)
        return FALSE; /* unterminated field: reject, never truncate */
    HKEY key = nullptr;
    if (RegCreateKeyExA(root, HUNMIN_TSF_CONFIG_ROOT_RELPATH, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return FALSE;
    /* The flag is stored as the same REG_SZ "0"/"1" every other slot uses, so
     * one reader shape (config_load) covers the whole store. */
    const char *autoswitchText = cfg->autoswitch ? "1" : "0";
    struct {
        const char *name;
        const char *value;
    } vals[6] = {{HUNMIN_TSF_REGVAL_HANGUL, cfg->hangulLayout},
                 {HUNMIN_TSF_REGVAL_LATIN, cfg->latinLayout},
                 {HUNMIN_TSF_REGVAL_TOGGLE, cfg->toggleKeys},
                 {HUNMIN_TSF_REGVAL_HANJAKEY, cfg->hanjaKey},
                 {HUNMIN_TSF_REGVAL_COMMITUNIT, cfg->commitUnit},
                 {HUNMIN_TSF_REGVAL_AUTOSWITCH, autoswitchText}};
    for (int i = 0; i < 6; ++i) {
        if (RegSetValueExA(key, vals[i].name, 0, REG_SZ, (const BYTE *)vals[i].value, (DWORD)strlen(vals[i].value) + 1) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return FALSE;
        }
    }
    RegCloseKey(key);
    return TRUE;
}
