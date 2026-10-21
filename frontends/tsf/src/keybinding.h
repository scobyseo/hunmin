/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_TSF_KEYBINDING_H_
#define HUNMIN_TSF_KEYBINDING_H_

/* The mode-toggle key bindings (issue #66 / #71 discussion.
 *
 * Direct port of frontends/android KeyBinding.kt's semantics into Windows
 * vocabulary, at the maintainer's instruction:
 *   - Default THE Han/Eng key: VK_HANGEUL (0x15), bare. On Korean hardware
 *     that physical key is what a user reaches for first, exactly like
 *     Android's KEYCODE_LANGUAGE_SWITCH / Samsung KANA(218) — the user's
 *     note: "there kana is detected as han/eng key".
 *   - Additional keys are configurable (Right Alt, Shift+Space, ...), stored
 *     the same way Android stores one: "v1:<vk>:<mods>", DECIMAL numbers,
 *     one combo per list entry, entries joined with ';'.
 *   - Stored bindings ADD; the physical Han/Eng key is a platform alternate
 *     that no rebinding can make dead (Android's rule: "binding replaces
 *     defaults, never platformAlternates").
 *
 * Modifier vocabulary fits Windows instead of Android's metaState because the
 * event source is different: TSF hands the TIP (vk, lParam) and the thread's
 * input state is authoritative. Bits:
 *   0x1 SHIFT  0x2 CTRL  0x4 ALT
 * Side selection is carried by the VK itself (VK_RMENU vs VK_LMENU — the
 * mirror of Android's side-coded ALT_RIGHT), so the ALT bit here means the
 * family, either side, exactly when a binding wants an unspecified-side Alt.
 * Shift has one meaning on both platforms.
 *
 * why numbers and not names: android's WHY keeps here too — the stored string
 * is numbers end to end and the comparison is the only contract.
 */

/* windows.h is now a plain include (SHORT, and the provider default's
 * GetKeyState): the old "deliberately free of windows.h" rule served the
 * per-TU #define GetKeyState macro seam, which is retired - the harness
 * registers its provider (hunmin_tsf_keystate_register) instead. */
#include <windows.h>

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#define HUNMIN_TSF_KEYMOD_SHIFT 0x1u
#define HUNMIN_TSF_KEYMOD_CTRL 0x2u
#define HUNMIN_TSF_KEYMOD_ALT 0x4u

/* vk of the physical Han/Eng key. windows.h defines VK_HANGEUL = 0x15; spell
 * it out here because the mock harness includes this TU without windows.h
 * having a Korean keyboard to prove it against. */
#define HUNMIN_TSF_VIRTUAL_HANGEUL 0x15u
/* vk of the physical Hanja key — the default candidate-window key (issue #67,
 * maintainer instruction: the Hanja key, not F9). windows.h defines
 * VK_HANJA = 0x19; same reason to spell it out. */
#define HUNMIN_TSF_VIRTUAL_HANJA 0x19u

typedef struct HunminKeyCombo {
    unsigned int vk;
    unsigned int mods;
} HunminKeyCombo;

/* Parse a stored list ("v1:21:0;v1:32:1", empty or NULL legal) into *out.
 * Returns the number of parsed combos; bad entries are skipped, a bad list is
 * not an error — an unparseable binding must never swallow a keystroke (the
 * Android rule: "matches nothing when unparseable"). */
int hunmin_tsf_keybindings_parse(const char *list, HunminKeyCombo *out,
                                 int cap);

/* Is (vk, modsState) one of the bindings? modsState is the snapshot the
 * caller built (below), NOT raw GetKeyState calls: a binding must be matched
 * against the same state the next decision sees, and the mock harness
 * injects state, so the point of truth stays outside this module. */
int hunmin_tsf_keybindings_match(const HunminKeyCombo *bindings, int count,
                                 unsigned int vk, unsigned int modsState);

/* The modifier bits the current thread input state implies. Shift/Control/Alt
 * via the aggregate vks; the family rule only (the VK names a side binding). */
unsigned int hunmin_tsf_keymods_from_state(void);

/* The CapsLock TOGGLE state (issue #112, ruling "wire and make it common to
 * all platform"): 1 when toggled on. A toggle key reports its state in the
 * LOW bit of GetKeyState (0x0001), not the 0x8000 down bit the modifiers
 * above use - and it is deliberately NOT a HUNMIN_TSF_KEYMOD bit: bindings
 * match three families exactly, and CapsLock must never make a bare binding
 * miss (Android's isModifierChord excludes it the same way). Read beside
 * hunmin_tsf_keymods_from_state() so host state enters the key path at one
 * seam, and fed to the engine as the "caps" option (the yethangul P
 * variable, hunmin.h:93) once per key event - the same per-event 1/0 shape
 * fcitx5 has (engine.cpp:327) and Android since #112's alignment. */
int hunmin_tsf_capslock_from_state(void);

/* The thread input state the key decisions read. The platform provider
 * (GetKeyState) is the default; a harness registers its own snapshot
 * provider - the state seam the old per-TU #define GetKeyState macro
 * carried, without the macro. NULL resets to the platform provider. */
typedef SHORT (*HunminTsfKeyStateFn)(int vk);
void hunmin_tsf_keystate_register(HunminTsfKeyStateFn provider);

#ifdef __cplusplus
}
#endif
#endif /* HUNMIN_TSF_KEYBINDING_H_ */
