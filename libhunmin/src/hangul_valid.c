/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hangul_valid.c - the structural half of the modes judgement
 * (docs/SPEC-MODES.md §R8-정정, ruling 2026-10-25: `hangul_valid` is "the
 * buffer composes as Korean syllables of the shape (choseong + jungseong) or
 * (choseong + jungseong + jongseong) only"). Issue #166.
 *
 * A PREDICATE OVER RENDERED TEXT, NOT A LOOKUP. It consults no data file, no
 * dictionary and no word list: the only thing it needs is the composition rule
 * tables.c already carries, which is why it is a pure function and why an
 * absent data directory cannot change its answer. R8-정정's point is that the
 * Hangul side of the pair has no source to choose.
 *
 * WHAT IT REJECTS, and each rejection is a decision this file records:
 *
 *   an in-flight syllable   the engine keeps a partially composed syllable as
 *                           conjoining jamo (or compatibility jamo on screen),
 *                           so `가ㄴ` is not a finished buffer and is not valid.
 *   latin, digits, space,   a buffer that completes at a delimiter never holds
 *   punctuation             one, so seeing one means the caller passed a
 *                           sentence rather than a buffer -- and a digit run is
 *                           a buffer (R6) that is not a Hangul word.
 *   old Hangul              `hm_jamo_to_syllable` accepts only the modern
 *                           19/21/28 sets (tables.c `*_conjoinable`), so an
 *                           archaic jongseong cannot be precomposed and the
 *                           composer renders it as jamo (ic.c jaso_to_string).
 *                           Four shipped layouts (dubeolsik-yet, sebeolsik-yet,
 *                           ahnmatmae, sin3-p2-yet) therefore read as NOT
 *                           hangul-valid. That is the literal reading of
 *                           "well-formed MODERN syllable"; if the judgement
 *                           must treat old-Hangul buffers as Hangul, this is
 *                           where the rule changes, not in a frontend.
 *
 * Equivalent to "every character is a precomposed syllable", because U+AC00..
 * U+D7A3 is exactly the 19x21x28 block. The decomposition below is done anyway:
 * it names the cho/jung/jong sets the requirement names, so a future widening
 * of the block cannot silently pass, and a reader can check the rule against
 * the requirement without holding the Unicode arithmetic in their head.
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