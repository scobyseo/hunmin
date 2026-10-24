/* Lifted 날개셋 condition expressions (schema v2 `case`).
 * Grammar: or := and ('||' and)* ; and := unary ('&&' unary)*
 *          unary := '!' unary | cmp ; cmp := atom [op atom]
 *          atom := var | number(0x..|dec) | '(' or ')'
 *          var  := 'D' | 'E' | 'F' (slot codepoints) | 'T' (tier state)
 *                  | 'P' (Caps Lock) | any other single letter (user flag)
 * Numbers are Unicode codepoints after converter rewriting (docs/SPEC-P2.md). */
#ifndef HUNMIN_EXPR_H
#define HUNMIN_EXPR_H

#include <stddef.h>
#include <stdint.h>

typedef struct hm_expr hm_expr;

typedef struct {
    uint32_t D, E, F; /* slot contents (0 = empty) */
    uint32_t T; /* symbol-tier state (0 = none) */
    int J; /* current jung is an absorbable right-side variant (h3) */
    int P; /* Caps Lock */
    const int *flags; /* user flags indexed by char (A-Z,a-z), may be NULL (all 0) */
} hm_state;

hm_expr *hm_expr_parse(const char *s, char *err, size_t errlen);
int hm_expr_eval(const hm_expr *e, hm_state st); /* 0/1 */
void hm_expr_free(hm_expr *e);

#endif
