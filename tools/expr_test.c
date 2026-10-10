/* expr_test — vectors for the lifted-expression evaluator (docs/TESTING.md tier A).
 * Guards refactors of expr.c/ic.c resolution (CODE_RULES.md rule 9). */
#include "expr.h"
#include <stdio.h>
#include <string.h>

static int fails;
static int flags[128];

static hm_state mk(uint32_t D, uint32_t E, uint32_t F, uint32_t T,
                   int P, int J) {
  hm_state s = { .D = D, .E = E, .F = F, .T = T, .P = P, .J = J, .flags = flags };
  return s;
}

static void ev(const char *expr, hm_state st, int want) {
  char err[128];
  hm_expr *e = hm_expr_parse(expr, err, sizeof err);
  if (!e) { printf("PARSE FAIL %s: %s\n", expr, err); fails++; return; }
  int got = hm_expr_eval(e, st);
  if (got != want) {
    printf("EVAL FAIL %s st{D=%X E=%X F=%X T=%u P=%d J=%d} got %d want %d\n",
           expr, st.D, st.E, st.F, st.T, st.P, st.J, got, want);
    fails++;
  }
  hm_expr_free(e);
}

static void bad(const char *expr) {
  char err[128];
  if (hm_expr_parse(expr, err, sizeof err)) { printf("should reject: %s\n", expr); fails++; }
}

int main(void) {
  flags['z'] = 1;

  /* slot truthiness + comparisons (Unicode codepoints post-conversion) */
  ev("D&&!E", mk(0x1100, 0, 0, 0, 0, 0), 1);
  ev("D&&!E", mk(0x1100, 0x1169, 0, 0, 0, 0), 0);
  ev("D&&!E", mk(0, 0, 0x11A8, 0, 0, 0), 0);
  ev("D==0x1100||D==0x110B", mk(0x1100, 0, 0, 0, 0, 0), 1);
  ev("!D&&!E&&F", mk(0, 0, 0x11A8, 0, 0, 0), 1);

  /* E range/== rewrites emitted by ist2data (variant vowels = J predicate) */
  ev("E&&J", mk(0x1100, 0x1169, 0, 0, 0, 1), 1);
  ev("E==0x1169&&J", mk(0, 0x1169, 0, 0, 0, 1), 1);
  ev("E==0x1169&&J", mk(0, 0x1169, 0, 0, 0, 0), 0);   /* plain ㅗ from v: NOT absorbable */
  ev("E&&!J", mk(0, 0x1161, 0, 0, 0, 0), 1);
  ev("E&&!J", mk(0, 0x1169, 0, 0, 0, 1), 0);

  /* tier trigger (z && cho==ieung) */
  ev("z&&D==0x110B&&!E&&!F", mk(0x110B, 0, 0, 0, 0, 0), 1);
  ev("z&&D==0x110B&&!E&&!F", mk(0x110C, 0, 0, 0, 0, 0), 0);

  /* tier + caps + flags */
  ev("T==101", mk(0, 0, 0, 101, 0, 0), 1);
  ev("T == 102", mk(0, 0, 0, 101, 0, 0), 0);
  ev("(!P||!w)&&D&&!E&&!F", mk(0x1100, 0, 0, 0, 0, 0), 1);   /* caps off, w=0 */
  flags['w'] = 1;
  ev("(!P||!w)&&D&&!E&&!F", mk(0x1100, 0, 0, 0, 1, 0), 0);   /* caps on + w=1 → suppress */
  ev("(!P||!w)&&D&&!E&&!F", mk(0x1100, 0, 0, 0, 0, 0), 1);

  /* precedence mirrors C: && above ||; decimal numbers */
  ev("D||E&&F", mk(0, 0x1161, 0, 0, 0, 0), 0);
  ev("D==138", mk(138, 0, 0, 0, 0, 0), 1);
  ev("!(D&&E)&&F", mk(0, 0, 0x11A8, 0, 0, 0), 1);

  bad("D&&"); bad("(D"); bad("D % E"); bad("");
  printf(fails ? "expr vectors: %d FAILURES\n" : "expr vectors: OK\n", fails);
  return fails ? 1 : 0;
}
