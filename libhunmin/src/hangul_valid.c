/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hangul_valid.c - the structural half of the modes judgement: a predicate over
 * rendered text, consulting no data file (docs/SPEC-MODES.md §ReqMode8).
 *
 * What it rejects, and why each rejection is a decision, is recorded once in
 * `docs/DESIGN-CORE.md` §"What `hunmin_hangul_valid` rejects"; the code below is
 * the same rule expressed as a decomposition, so a widening of the precomposed
 * block cannot pass silently.
 */
#include <stdint.h>

#include "hunmin.h"
#include "tables.h"

/* UTF-8, one code point. Advances *p. Returns the code point, or 0 when the
 * sequence is malformed, overlong, a surrogate or truncated. */
static uint32_t decode_utf8(const unsigned char **p) {
    const unsigned char *s = *p;
    uint32_t c = s[0];
    int extra;
    uint32_t min;

    if (c < 0x80) {
        *p = s + 1;
        return c;
    }
    if ((c & 0xe0) == 0xc0) {
        extra = 1;
        c &= 0x1f;
        min = 0x80;
    } else if ((c & 0xf0) == 0xe0) {
        extra = 2;
        c &= 0x0f;
        min = 0x800;
    } else if ((c & 0xf8) == 0xf0) {
        extra = 3;
        c &= 0x07;
        min = 0x10000;
    } else {
        return 0;
    }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xc0) != 0x80) return 0;
        c = (c << 6) | (uint32_t)(s[i] & 0x3f);
    }
    if (c < min) return 0;                       /* overlong */
    if (c >= 0xd800 && c <= 0xdfff) return 0;    /* surrogate */
    if (c > 0x10ffff) return 0;
    *p = s + 1 + extra;
    return c;
}

int hunmin_hangul_valid(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    int syllables = 0;

    if (!text) return 0;

    while (*p) {
        uint32_t c = decode_utf8(&p);
        uint32_t s, cho, jung, jong;

        if (c == 0) return 0;              /* malformed UTF-8 */
        if (c < 0xac00 || c > 0xd7a3) return 0; /* jamo, latin, digit, symbol */

        s = c - 0xac00;
        cho = 0x1100 + s / (21 * 28);
        jung = 0x1161 + (s % (21 * 28)) / 28;
        jong = 0x11a7 + (s % 28); /* 0x11a7 is the no-jongseong filler */

        if (!hm_is_choseong_conjoinable((ucschar)cho)) return 0;
        if (!hm_is_jungseong_conjoinable((ucschar)jung)) return 0;
        if (jong != 0x11a7 && !hm_is_jongseong_conjoinable((ucschar)jong)) return 0;

        syllables++;
    }

    return syllables > 0;
}

int hunmin_hangul_valid_old(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    int syllables = 0;

    if (!text) return 0;

    while (*p) {
        uint32_t c = decode_utf8(&p);

        if (c == 0) return 0; /* malformed UTF-8 */

        /* A precomposed syllable is one complete group, whatever it is: the
         * modern block is the only place one can exist, so it is also the only
         * place this branch fires. */
        if (c >= 0xac00 && c <= 0xd7a3) {
            syllables++;
            continue;
        }

        /* Otherwise the group is (cho)(jung)(jong) over the EXTENDED sets, and a
         * leading choseong is required — the engine's renderer writes the CF
         * filler (U+115F) when a syllable has no initial, so a bare jungseong
         * here (ㅜㄷ in 랴ㅜㄷ) is a shape the composer never emits, not a
         * syllable. That is what keeps 랴ㅜㄷ invalid while ᅀᅡ / ᅌᅵ / ᅀᅡᆶ are
         * valid. */
        if (!hm_is_choseong((ucschar)c)) return 0;
        c = decode_utf8(&p);
        if (c == 0 || !hm_is_jungseong((ucschar)c)) return 0;
        if (*p) {
            const unsigned char *next = p;
            uint32_t d = decode_utf8(&p);
            if (!d || !hm_is_jongseong((ucschar)d)) p = next; /* the next group */
        }
        syllables++;
    }

    return syllables > 0;
}