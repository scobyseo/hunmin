/* expr.c — recursive-descent parser/evaluator for lifted expressions. */
#include "expr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OP_OR,
       OP_AND,
       OP_NOT,
       OP_EQ,
       OP_NE,
       OP_LE,
       OP_GE,
       OP_LT,
       OP_GT,
       OP_D,
       OP_E,
       OP_F,
       OP_T,
       OP_P,
       OP_J,
       OP_FLAG,
       OP_NUM };

struct hm_expr {
  int op;
  uint32_t num; /* OP_NUM value / OP_FLAG char */
  hm_expr *a, *b;
};

typedef struct {
  const char *s;
  char *err;
  size_t errlen;
  int bad;
} P;

static void perr(P *p, const char *m) {
  if (!p->bad && p->err) snprintf(p->err, p->errlen, "%s at '%.16s'", m, p->s);
  p->bad = 1;
}

static hm_expr *mk(int op) {
  hm_expr *e = calloc(1, sizeof *e);
  if (e) e->op = op;
  return e;
}

static void skip(P *p) {
  while (*p->s == ' ' || *p->s == '\t') p->s++;
}

static hm_expr *parse_or(P *p);

static hm_expr *parse_atom(P *p) {
  skip(p);
  if (*p->s == '(') {
    p->s++;
    hm_expr *e = parse_or(p);
    skip(p);
    if (!p->bad && *p->s != ')') {
      perr(p, "expected )");
      return NULL;
    }
    if (!p->bad) p->s++;
    return e;
  }
  if (p->s[0] == '0' && (p->s[1] == 'x' || p->s[1] == 'X')) {
    char *end;
    uint32_t v = (uint32_t)strtoul(p->s, &end, 16);
    p->s = end;
    hm_expr *e = mk(OP_NUM);
    if (e) e->num = v;
    return e;
  }
  if (p->s[0] >= '0' && p->s[0] <= '9') {
    char *end;
    uint32_t v = (uint32_t)strtoul(p->s, &end, 10);
    p->s = end;
    hm_expr *e = mk(OP_NUM);
    if (e) e->num = v;
    return e;
  }
  if ((p->s[0] >= 'A' && p->s[0] <= 'Z') || (p->s[0] >= 'a' && p->s[0] <= 'z')) {
    char c = *p->s++;
    hm_expr *e = mk(c == 'D' ? OP_D : c == 'E' ? OP_E
                                  : c == 'F'   ? OP_F
                                  : c == 'T'   ? OP_T
                                  : c == 'P'   ? OP_P
                                  : c == 'J'   ? OP_J
                                               : OP_FLAG);
    if (e) e->num = (unsigned char)c;
    return e;
  }
  perr(p, "bad atom");
  return NULL;
}

static int is_cmp(P *p, int *op, int *len) {
  const char *s = p->s;
  if (s[0] == '=' && s[1] == '=') {
    *op = OP_EQ;
    *len = 2;
    return 1;
  }
  if (s[0] == '!' && s[1] == '=') {
    *op = OP_NE;
    *len = 2;
    return 1;
  }
  if (s[0] == '<' && s[1] == '=') {
    *op = OP_LE;
    *len = 2;
    return 1;
  }
  if (s[0] == '>' && s[1] == '=') {
    *op = OP_GE;
    *len = 2;
    return 1;
  }
  if (s[0] == '<') {
    *op = OP_LT;
    *len = 1;
    return 1;
  }
  if (s[0] == '>') {
    *op = OP_GT;
    *len = 1;
    return 1;
  }
  return 0;
}

static hm_expr *parse_cmp(P *p) {
  hm_expr *a = parse_atom(p);
  if (!a || p->bad) return a;
  skip(p);
  int op, len;
  if (!is_cmp(p, &op, &len)) return a;
  p->s += len;
  hm_expr *b = parse_atom(p);
  if (!b || p->bad) {
    hm_expr_free(a);
    return NULL;
  }
  hm_expr *e = mk(op);
  if (!e) {
    hm_expr_free(a);
    hm_expr_free(b);
    return NULL;
  }
  e->a = a;
  e->b = b;
  return e;
}

static hm_expr *parse_unary(P *p) {
  skip(p);
  if (p->s[0] == '!' && p->s[1] != '=') {
    p->s++;
    hm_expr *a = parse_unary(p);
    if (!a || p->bad) return a;
    hm_expr *e = mk(OP_NOT);
    if (!e) {
      hm_expr_free(a);
      return NULL;
    }
    e->a = a;
    return e;
  }
  return parse_cmp(p);
}

static hm_expr *parse_and(P *p) {
  hm_expr *a = parse_unary(p);
  while (a && !p->bad) {
    skip(p);
    if (p->s[0] == '&' && p->s[1] == '&') {
      p->s += 2;
      hm_expr *b = parse_unary(p);
      if (!b || p->bad) {
        hm_expr_free(a);
        return NULL;
      }
      hm_expr *e = mk(OP_AND);
      if (!e) {
        hm_expr_free(a);
        hm_expr_free(b);
        return NULL;
      }
      e->a = a;
      e->b = b;
      a = e;
    } else break;
  }
  return a;
}

static hm_expr *parse_or(P *p) {
  hm_expr *a = parse_and(p);
  while (a && !p->bad) {
    skip(p);
    if (p->s[0] == '|' && p->s[1] == '|') {
      p->s += 2;
      hm_expr *b = parse_and(p);
      if (!b || p->bad) {
        hm_expr_free(a);
        return NULL;
      }
      hm_expr *e = mk(OP_OR);
      if (!e) {
        hm_expr_free(a);
        hm_expr_free(b);
        return NULL;
      }
      e->a = a;
      e->b = b;
      a = e;
    } else break;
  }
  return a;
}

hm_expr *hm_expr_parse(const char *s, char *err, size_t errlen) {
  P p = {s, err, errlen, 0};
  if (err && errlen) err[0] = 0;
  hm_expr *e = parse_or(&p);
  if (!e || p.bad) {
    hm_expr_free(e);
    return NULL;
  }
  skip(&p);
  if (*p.s) {
    perr(&p, "trailing");
    hm_expr_free(e);
    return NULL;
  }
  return e;
}

static uint32_t operand(const hm_expr *e, hm_state st) {
  switch (e->op) {
  case OP_NUM: return e->num;
  case OP_D: return st.D;
  case OP_E: return st.E;
  case OP_F: return st.F;
  case OP_T: return st.T;
  case OP_P: return st.P ? 1 : 0;
  case OP_FLAG: return st.flags ? st.flags[e->num] : 0;
  default: return (uint32_t)hm_expr_eval(e, st);
  }
}

int hm_expr_eval(const hm_expr *e, hm_state st) {
  if (!e) return 0;
  switch (e->op) {
  case OP_OR: return hm_expr_eval(e->a, st) || hm_expr_eval(e->b, st);
  case OP_AND: return hm_expr_eval(e->a, st) && hm_expr_eval(e->b, st);
  case OP_NOT: return !hm_expr_eval(e->a, st);
  case OP_D: return st.D != 0;
  case OP_E: return st.E != 0;
  case OP_F: return st.F != 0;
  case OP_J: return st.J != 0;
  case OP_T: return st.T != 0;
  case OP_P: return st.P;
  case OP_FLAG: return e->num < 128 && st.flags ? st.flags[e->num] : 0;
  case OP_NUM: return e->num != 0;
  case OP_EQ:
  case OP_NE:
  case OP_LE:
  case OP_GE:
  case OP_LT:
  case OP_GT: {
    uint32_t a = operand(e->a, st), b = operand(e->b, st);
    switch (e->op) {
    case OP_EQ: return a == b;
    case OP_NE: return a != b;
    case OP_LE: return a <= b;
    case OP_GE: return a >= b;
    case OP_LT: return a < b;
    default: return a > b;
    }
  }
  default: return 0;
  }
}

void hm_expr_free(hm_expr *e) {
  if (!e) return;
  hm_expr_free(e->a);
  hm_expr_free(e->b);
  free(e);
}
