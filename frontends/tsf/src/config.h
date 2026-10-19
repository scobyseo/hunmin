/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_TSF_CONFIG_H_
#define HUNMIN_TSF_CONFIG_H_

/* User configuration surface for the TSF frontend (issues #66 / #71)
 *
 * Hangul and Latin layout selection, mirroring the fcitx5 addon config
 * (frontends/fcitx5/src/config.h: option names HangulLayout / LatinLayout,
 * labels 한글 배열 / 영어 배열, defaults "sin3-p2" / "us-qwerty"). A Windows
 * frontend has no configtool; the settings dialog (#71) writes this store and
 * every Activate() reads it.
 *
 * There is no live apply after a save (ruled 2026-10-20, issue #120): the
 * settings exe is a separate process, and nothing asks the running TIP to
 * re-read. A save reaches a host when that host next ACTIVATES the TIP — a
 * new app's first activation, or a Win+Space round-trip in an already-running
 * app (measured 2026-10-20: re-activation re-runs Activate() and re-reads;
 * the hunmin-tsf-debug.log carries only ever Activate()-driven reads). An app
 * sitting on Hunmin across the save keeps the OLD layout until then.
 *
 * Read order, first source that yields a non-empty value wins:
 *   1. environment  HUNMIN_TSF_HANGUL_LAYOUT / HUNMIN_TSF_LATIN_LAYOUT
 *      (the test seam: tier F1 and the config test inject layouts without
 *      touching a real user's registry),
 *   2. registry     <root>\Software\Hunmin\TSF, values HangulLayout /
 *      LatinLayout (REG_SZ, written by the settings dialog),
 *   3. defaults     sin3-p2 / us-qwerty.
 *
 * Raw load/save only here; the adapter owns validation (see tsf.cpp): a stored
 * id is accepted only when hunmin_ctx_layout_supported() passes AND the
 * layout's mode matches the slot (hangul row / latin row), else the default
 * returns. SPEC-MODES R13 boundary: no per-frontend settings store for engine
 * state — the only fields here are layout ids; feature state stays with the
 * engine (hunmin_ic_set_option).
 */

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUNMIN_TSF_CONFIG_ROOT_RELPATH "Software\\Hunmin\\TSF"
#define HUNMIN_TSF_ENV_HANGUL_LAYOUT "HUNMIN_TSF_HANGUL_LAYOUT"
#define HUNMIN_TSF_ENV_LATIN_LAYOUT "HUNMIN_TSF_LATIN_LAYOUT"
#define HUNMIN_TSF_ENV_TOGGLE_KEYS "HUNMIN_TSF_TOGGLE_KEYS"
#define HUNMIN_TSF_ENV_HANJA_KEY "HUNMIN_TSF_HANJA_KEY"
#define HUNMIN_TSF_REGVAL_HANGUL "HangulLayout"
#define HUNMIN_TSF_REGVAL_LATIN "LatinLayout"
#define HUNMIN_TSF_REGVAL_TOGGLE "HangulEngKeys"
#define HUNMIN_TSF_REGVAL_HANJAKEY "HanjaKey"

/* Slots are fixed-size: layout ids are short ASCII strings and the engine
 * compares them by exact bytes; 64 covers every id in libhunmin/data. */
#define HUNMIN_TSF_LAYOUT_ID_CAP 64
/* The mode-toggle binding list ("v1:<vk>:<mods>;...", see keybinding.h) plus
 * NUL: room for four chords plus the headroom a long list needs. */
#define HUNMIN_TSF_TOGGLE_KEYS_CAP 256
/* The Hanja/candidate key: ONE binding in the same grammar (issue #67,
 * maintainer instruction: the physical Hanja key is the default candidate
 * key, not F9, and the key is selectable in the settings dialog). */
#define HUNMIN_TSF_HANJA_KEY_CAP 32

typedef struct HunminTsfConfig {
    char hangulLayout[HUNMIN_TSF_LAYOUT_ID_CAP];
    char latinLayout[HUNMIN_TSF_LAYOUT_ID_CAP];
    /* The mode-toggle bindings, Android's v1:<keyCode>:<modifiers> format.
     * Empty means "nothing configured": the physical Han/Eng key
     * (VK_HANGEUL, bare) stays the toggle whatever is stored — the platform
     * alternate no rebinding can make dead, the maintainer's instruction that
     * the physical key is the default. */
    char toggleKeys[HUNMIN_TSF_TOGGLE_KEYS_CAP];
    /* The candidate-window key, same grammar, ONE entry. Empty means the
     * default: the physical Hanja key (VK_HANJA, bare) — the same
     * platform-alternate rule as the toggle. */
    char hanjaKey[HUNMIN_TSF_HANJA_KEY_CAP];
} HunminTsfConfig;

/* Fill *out from env, then registry, then defaults. root may be NULL (the
 * production caller passes HKEY_CURRENT_USER; a test passes an absent key to
 * pin the deterministic no-registry path). Missing values, an absent key and
 * a NULL root all fall through to the next source, never fail the load.
 * Returns the number of slots satisfied from the registry (0 in the common
 * default case) — diagnostics, not an error signal. */
int hunmin_tsf_config_load(HKEY root, HunminTsfConfig *out);

/* Write both slots as REG_SZ under <root>\Software\Hunmin\TSF, creating the
 * key. Returns TRUE on success, FALSE when any step failed; the caller (the
 * settings dialog) reports a failed save to the user (the Jamotong rule:
 * never silently drop an explicit user action). Layout ids longer than the
 * capacity are rejected rather than truncated. */
BOOL hunmin_tsf_config_save(HKEY root, const HunminTsfConfig *cfg);

#ifdef __cplusplus
}
#endif
#endif /* HUNMIN_TSF_CONFIG_H_ */
