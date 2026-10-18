/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier A: hunmin_ic_process_key() — the explicit-modifier entry point
 * appended beside the case-encoding hunmin_ic_process() (issue #90,
 * docs/IC-INPUT-OPTIONS.md §2-B, option B).
 *
 * The property asserted is EQUALITY over the whole reachable event space:
 * for every shipped layout and every (base, shift) row, the new call must
 * produce exactly what the legacy call produces for the character that row
 * resolves to — same commit, same preedit, same consumption decision. An
 * appended entry point invalidates no golden and no corpus (§0-2 of the
 * options doc); this test is the guard that pins the appended surface to the
 * behaviour those corpora already record.
 *
 * The legacy character is computed here from an INDEPENDENT copy of the
 * US-QWERTY shift map, not from the engine: should the loader's map ever lose
 * a shifted form (shifted_of losing injectivity or coverage, §4 of the same
 * doc), the two sides of the equality would be fed different characters and
 * the test reads red, rather than both sides agreeing on a wrong event.
 */
#include "hunmin.h"

#include <stdio.h>
#include <string.h>

static int failures;

static int check(int cond, const char *what) {
  if (!cond) {
    printf("FAIL: %s\n", what);
    failures++;
  }
  return cond;
}

/* Legacy (case-encoding) character an (base, shift) event resolves to —
 * the loader's own US-QWERTY map (data.c BASE/SHIFTED, 2026-10-18 tree),
 * spelled out independently. */
static uint32_t legacy_ch(uint32_t base, uint32_t shift) {
  static const char *BASE[] = {
      "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
      "[", "]", "\\", ";", "'", ",", ".", "/", NULL};
  static const char *SHIFTED[] = {
      "~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+",
      "{", "}", "|", ":",  "\"", "<", ">", "?", NULL};
  if (!shift) return base;
  if (base >= 'a' && base <= 'z') return base - 'a' + 'A';
  if (base < 128)
    for (int i = 0; BASE[i]; i++)
      if (BASE[i][0] == (char)base) return (uint32_t)SHIFTED[i][0];
  return base; /* already-resolved rule: nothing to shift (SPEC-P2) */
}

/* One event through both front doors; every difference aborts early so the
 * report names WHERE it diverged, not just that it did. */
static void equate(hunmin_ic *legacy, hunmin_ic *key, uint32_t base,
                   uint32_t shift, const char *layout) {
  uint32_t want = legacy_ch(base, shift);
  bool rl = hunmin_ic_process(legacy, want);
  bool rk = hunmin_ic_process_key(key, base, shift);
  const char *lc = hunmin_ic_commit(legacy), *lp = hunmin_ic_preedit(legacy);
  const char *kc = hunmin_ic_commit(key), *kp = hunmin_ic_preedit(key);
  if (!strcmp(lc, kc) && !strcmp(lp, kp) && rl == rk) return;
  printf("FAIL: %s: process_key(base=%u, shift=%u) diverges from process(%u)\n"
         "  commit '%s' vs '%s'; preedit '%s' vs '%s'; consumed %d vs %d\n",
         layout, base, shift, want, lc, kc, lp, kp, (int)rl, (int)rk);
  failures++;
}

int main(void) {
  const char *dirs[] = {"libhunmin/data"};
  hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
  check(ctx != NULL, "ctx_new");
  if (!ctx) return 1;

  /* --- equality sweep: every layout, every reachable (base, shift) row --- */
  int n = hunmin_ctx_layout_count(ctx);
  check(n > 0, "the data set ships at least one layout");
  for (int i = 0; i < n; i++) {
    const char *id = hunmin_ctx_layout_id(ctx, i);
    hunmin_ic *a = hunmin_ic_new(ctx, id), *b = hunmin_ic_new(ctx, id);
    if (!check(a != NULL && b != NULL, id ? id : "?")) continue;
    for (uint32_t base = 0; base < 128; base++)
      for (uint32_t shift = 0; shift <= 1; shift++)
        equate(a, b, base, shift, id ? id : "?");
    hunmin_ic_free(a);
    hunmin_ic_free(b);
  }
  if (failures) goto done;

  /* --- already-resolved rule, named so a red run says WHAT was violated ---
   * A shifted form ('A', '!') passed as base must reach the engine unchanged,
   * and the shift bit must select nothing further on it — the fcitx5-shaped
   * host case. On dubeolsik, 'A' is the shifted row of 'g' (ㄲ); both arms
   * must therefore type the same thing. */
  {
    hunmin_ic *a = hunmin_ic_new(ctx, "dubeolsik");
    hunmin_ic *b = hunmin_ic_new(ctx, "dubeolsik");
    if (check(a && b, "dubeolsik ics")) {
      bool ra = hunmin_ic_process(a, 'A');
      bool rb = hunmin_ic_process_key(b, 'A', 1);
      const char *pa = hunmin_ic_preedit(a)[0] ? hunmin_ic_preedit(a)
                                               : hunmin_ic_commit(a);
      const char *pb = hunmin_ic_preedit(b)[0] ? hunmin_ic_preedit(b)
                                               : hunmin_ic_commit(b);
      check(ra == rb, "'A' with the shift bit: same consumption as legacy");
      check(!strcmp(pa, pb), "'A' with the shift bit: same output as legacy");
      hunmin_ic_free(a);
      hunmin_ic_free(b);
    }
  }

  /* --- engine edges the sweep cannot reach --- */
  check(!hunmin_ic_process_key(NULL, 'a', 1), "NULL ic -> false, never a crash");
  {
    /* A codepoint outside the US-QWERTY keyspace arrives already resolved:
     * the sweep covers 0..127, this covers the host that still hands over a
     * wide character. dubeolsik maps nothing there, so both arms must refuse
     * it identically and leave the composition untouched (typed '하' first,
     * then the wide character must not disturb it). */
    hunmin_ic *a = hunmin_ic_new(ctx, "dubeolsik");
    hunmin_ic *b = hunmin_ic_new(ctx, "dubeolsik");
    if (check(a && b, "dubeolsik ics (wide char)")) {
      hunmin_ic_process(a, 'g');
      hunmin_ic_process(a, 'k'); /* 하 in flight */
      hunmin_ic_process(b, 'g');
      hunmin_ic_process(b, 'k');
      bool ra = hunmin_ic_process(a, 0xAC00);
      bool rb = hunmin_ic_process_key(b, 0xAC00, 1);
      check(ra == rb, "wide base: same consumption as legacy");
      check(!strcmp(hunmin_ic_preedit(a), hunmin_ic_preedit(b)),
            "wide base: composition state identical to legacy");
      hunmin_ic_free(a);
      hunmin_ic_free(b);
    }
  }

done:
  hunmin_ctx_free(ctx);
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("process_key: all assertions hold\n");
  return 0;
}
