/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Toggle-key binding parse/match (frontends/tsf/src/keybinding.h), the Windows
 * port of frontends/android KeyBinding.kt. Deliberately free of windows.h so
 * the tier-F1 harness can compile it with GetKeyState injected by the mock:
 * the adapter calls hunmin_tsf_keymods_from_state() once per event and passes
 * the snapshot down, so the point where host state enters the decision is one
 * function the harness can pin.
 */
#include "keybinding.h"

#include <ctype.h>
#include <string.h>
#include <windows.h>

int hunmin_tsf_keybindings_parse(const char *list, HunminKeyCombo *out, int cap) {
    if ((list == nullptr || list[0] == '\0') || cap <= 0 || out == nullptr)
        return 0;
    int n = 0;
    const char *p = list;
    while (*p != '\0') {
        /* One entry: "v1:<vk>:<mods>", decimal numbers, Android format. */
        if (p[0] == 'v' && p[1] == '1' && p[2] == ':') {
            unsigned long vk = 0;
            unsigned long mods = 0;
            char *end = nullptr;
            vk = strtoul(p + 3, &end, 10);
            if (end != nullptr && *end == ':') {
                mods = strtoul(end + 1, &end, 10);
                if (end != nullptr && (*end == ';' || *end == '\0') && vk <= 0xFFu && mods <= 0x7u && ((vk & 0xFFu) != 0u)) {
                    if (n < cap) {
                        out[n].vk = (unsigned)vk;
                        out[n].mods = (unsigned)mods;
                        ++n;
                    }
                }
            }
        }
        /* Skip to the next entry or the end; a malformed entry yields no
         * combo and never aborts the list (matches: an unparseable binding
         * can't swallow a keystroke). */
        while (*p != ';' && *p != '\0')
            ++p;
        if (*p == ';')
            ++p;
    }
    return n;
}

int hunmin_tsf_keybindings_match(const HunminKeyCombo *bindings, int count, unsigned int vk, unsigned int modsState) {
    if (bindings == nullptr)
        return 0;
    for (int i = 0; i < count; ++i) {
        if (bindings[i].vk != vk)
            continue;
        const unsigned int want = bindings[i].mods;
        /* Bits are matched exactly: a bare binding must not fire under a
         * modifier, and a chord must not fire bare — Shift+Space and a shifted
         * layout row would otherwise fight over the same event. A side that
         * the vk already names (VK_RMENU) is not a modifier bit; only the
         * three family bits above decide. */
        if (want == modsState)
            return 1;
    }
    return 0;
}

/* The thread input state: the platform provider is the default; a harness
 * registers its snapshot provider over this one slot (keybinding.h). */
static SHORT hunmin_tsf_platform_keystate(int vk) { return GetKeyState(vk); }
static HunminTsfKeyStateFn g_hunmin_tsf_keystate = &hunmin_tsf_platform_keystate;

void hunmin_tsf_keystate_register(HunminTsfKeyStateFn provider) { g_hunmin_tsf_keystate = provider ? provider : &hunmin_tsf_platform_keystate; }

unsigned int hunmin_tsf_keymods_from_state(void) {
    unsigned int m = 0;
    if ((g_hunmin_tsf_keystate(VK_SHIFT) & 0x8000u) != 0u)
        m |= HUNMIN_TSF_KEYMOD_SHIFT;
    if ((g_hunmin_tsf_keystate(VK_CONTROL) & 0x8000u) != 0u)
        m |= HUNMIN_TSF_KEYMOD_CTRL;
    /* Either-side Alt: a side-specific binding names VK_RMENU / VK_LMENU as
     * the vk, so the family bit only indicates "some Alt is down". */
    if (((g_hunmin_tsf_keystate(VK_MENU) | g_hunmin_tsf_keystate(VK_LMENU) | g_hunmin_tsf_keystate(VK_RMENU)) & 0x8000u) != 0u)
        m |= HUNMIN_TSF_KEYMOD_ALT;
    return m;
}

int hunmin_tsf_capslock_from_state(void) {
    /* Toggle key: the LOW bit carries the toggled state, never 0x8000. */
    return (g_hunmin_tsf_keystate(VK_CAPITAL) & 0x0001u) != 0u;
}
