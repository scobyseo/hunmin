/* suggest.c — Hanja candidate lookup over data/hanja, data/krdict, data/idioms.
 *
 * Lookup is by whole Hangul: one syllable (hanja/), a word or phrase
 * (krdict/), or a four-character idiom (idioms/). Nothing here reads
 * surrounding text — see PLAN.md "P3" for why that constraint exists.
 *
 * The three tables are loaded on first use and kept for the process, since a
 * session that asks once will ask again. krdict.json is 6 MB, so it is read
 * and parsed once into a flat record array rather than kept as text.
 *
 * Candidates are copied into a small ctx-owned list on lookup and handed out
 * as pointers, matching hunmin_ic_commit()'s lifetime rule: valid until the
 * next call on this ctx.
 */
#include "hunmin.h"
#include "hunmin_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

typedef enum { SK_HANJA,
               SK_WORD,
               SK_IDIOM,
               SK_NONE } sug_kind;

/* One record, three strings, each its own allocation. They are not stored as
 * three parallel blobs under a shared offset: the strings have different
 * lengths, so a common offset into separate buffers points at the wrong byte
 * as soon as one of them is shorter than its key. That is not hypothetical --
 * it made 가화만사성 resolve to 까뿔. */
typedef struct {
  char *key;
  char *text;
  char *detail;
} sug_rec;

typedef struct {
  sug_rec *v;
  int n, cap;
  int loaded; /* 0 untried, 1 loaded, -1 absent */
} sug_table;

static sug_table g_hanja, g_word, g_idiom;

/* The list handed to the caller; serial, like the engine itself. */
typedef struct {
  sug_kind kind;
  char text[64];
  char detail[192];
} sug_item;

typedef struct {
  sug_item *v;
  int n, cap;
} sug_list;

static sug_list g_list;

#define SUG_MAX 512

static char *dupn(const char *s) {
  size_t n = strlen(s ? s : "") + 1;
  char *p = malloc(n);
  if (p) memcpy(p, s ? s : "", n);
  return p;
}

static void table_free(sug_table *t) {
  for (int i = 0; i < t->n; i++) {
    free(t->v[i].key);
    free(t->v[i].text);
    free(t->v[i].detail);
  }
  free(t->v);
  memset(t, 0, sizeof *t);
}

static sug_rec *table_push(sug_table *t) {
  if (t->n >= t->cap) {
    int cap = t->cap ? t->cap * 2 : 256;
    sug_rec *v = realloc(t->v, (size_t)cap * sizeof *v);
    if (!v) return NULL;
    t->v = v;
    t->cap = cap;
  }
  sug_rec *r = &t->v[t->n];
  memset(r, 0, sizeof *r);
  return r;
}

static char *slurp(const char *path, long cap, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return NULL;
  }
  long sz = ftell(f);
  if (sz < 0 || sz > cap) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    free(buf);
    fclose(f);
    return NULL;
  }
  buf[sz] = 0;
  fclose(f);
  if (out_len) *out_len = (size_t)sz;
  return buf;
}

/* --- hanja/: hunmin.hanja/2, {"entries":{"한":[{"v":"韓","ko":"…","en":"…"},…]}}
 *
 * detail is built as "ko | en" (+ " [JP]"/" [CN]" usage tag when assigned) so
 * the frontend can render it verbatim without a new ABI symbol. Korean gloss
 * first, English after the bar; the tag is omitted unless the converter could
 * assign it without a false positive (docs/HANJA-GLOSS-DESIGN.md). --- */
static int load_hanja(const char *path) {
  char *buf = slurp(path, 8L << 20, NULL);
  if (!buf) return -1;
  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) return -1;

  int rc = -1;
  const cJSON *ent = cJSON_GetObjectItemCaseSensitive(root, "entries");
  if (cJSON_IsObject(ent)) {
    table_free(&g_hanja);
    const cJSON *k = NULL;
    cJSON_ArrayForEach(k, ent) {
      if (!k->string) continue;
      for (const cJSON *c = k->child; c; c = c->next) {
        if (!cJSON_IsObject(c)) continue;
        const cJSON *jv = cJSON_GetObjectItemCaseSensitive(c, "v");
        if (!cJSON_IsString(jv) || !jv->valuestring) continue;
        const cJSON *jko = cJSON_GetObjectItemCaseSensitive(c, "ko");
        const cJSON *jen = cJSON_GetObjectItemCaseSensitive(c, "en");
        const cJSON *jjp = cJSON_GetObjectItemCaseSensitive(c, "jp");
        const cJSON *jcn = cJSON_GetObjectItemCaseSensitive(c, "cn");
        const char *ko = cJSON_IsString(jko) ? jko->valuestring : "";
        const char *en = cJSON_IsString(jen) ? jen->valuestring : "";
        const char *tag = "";
        if (cJSON_IsTrue(jjp)) tag = " [JP]";
        else if (cJSON_IsTrue(jcn)) tag = " [CN]";

        sug_rec *r = table_push(&g_hanja);
        if (!r) goto done;
        r->key = dupn(k->string);
        r->text = dupn(jv->valuestring);
        /* "ko | en [tag]" — drop an empty half and a dangling bar so a
         * Korean-only or English-only candidate reads cleanly. */
        char detail[1024];
        if (*ko && *en)
          snprintf(detail, sizeof detail, "%s | %s%s", ko, en, tag);
        else if (*ko)
          snprintf(detail, sizeof detail, "%s%s", ko, tag);
        else if (*en)
          snprintf(detail, sizeof detail, "%s%s", en, tag);
        else
          snprintf(detail, sizeof detail, "%s", *tag ? tag + 1 : "");
        r->detail = dupn(detail);
        if (!r->key || !r->text || !r->detail) goto done;
        g_hanja.n++;
      }
    }
    rc = 1;
  }
done:
  cJSON_Delete(root);
  return rc;
}

/* --- krdict/ and idioms/: {"entries":[{...}, ...]} --- */
static int load_records(const char *path, sug_kind kind, sug_table *t) {
  char *buf = slurp(path, 64L << 20, NULL);
  if (!buf) return -1;
  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) return -1;

  int rc = -1;
  const cJSON *ent = cJSON_GetObjectItemCaseSensitive(root, "entries");
  if (cJSON_IsArray(ent)) {
    table_free(t);
    const cJSON *e = NULL;
    cJSON_ArrayForEach(e, ent) {
      const cJSON *hanja = cJSON_GetObjectItemCaseSensitive(e, "hanja");
      if (!cJSON_IsString(hanja)) continue;

      const char *detail = "";
      const char *key;
      const cJSON *rd = cJSON_GetObjectItemCaseSensitive(e, "reading");
      if (kind == SK_IDIOM) {
        /* An idiom record carries no ko field at all: it is keyed by its
         * Hangul reading, which is what a caller searches, and looked up in
         * the reading's own order. */
        if (!cJSON_IsString(rd)) continue;
        key = rd->valuestring;
        detail = rd->valuestring;
      } else {
        const cJSON *ko = cJSON_GetObjectItemCaseSensitive(e, "ko");
        if (!cJSON_IsString(ko)) continue;
        key = ko->valuestring;
        const cJSON *mean = cJSON_GetObjectItemCaseSensitive(e, "meanings");
        if (cJSON_IsArray(mean) && cJSON_IsString(mean->child))
          detail = mean->child->valuestring;
        else if (cJSON_IsString(mean))
          detail = mean->valuestring;
      }
      sug_rec *r = table_push(t);
      if (!r) goto done;
      r->key = dupn(key);
      r->text = dupn(hanja->valuestring);
      r->detail = dupn(detail);
      if (!r->key || !r->text || !r->detail) goto done;
      t->n++;
    }
    rc = 1;
  }
done:
  cJSON_Delete(root);
  return rc;
}

/* <dir>/<sub>/<name>.json, first dir that has it. */
static int find_table(const char *const *dirs, int ndirs,
                      const char *sub, const char *name,
                      char *out, size_t outsz) {
  for (int d = 0; d < ndirs; d++) {
    snprintf(out, outsz, "%s/%s/%s.json", dirs[d], sub, name);
    FILE *f = fopen(out, "rb");
    if (f) {
      fclose(f);
      return 1;
    }
  }
  return 0;
}

static int ensure(sug_table *t, const char *const *dirs, int ndirs,
                  const char *sub, const char *name, sug_kind kind) {
  if (t->loaded) return t->loaded;
  char path[4608];
  if (!find_table(dirs, ndirs, sub, name, path, sizeof path)) {
    t->loaded = -1;
    return -1;
  }
  int rc = kind == SK_HANJA ? load_hanja(path)
                            : load_records(path, kind, t);
  t->loaded = rc > 0 ? 1 : -1;
  return rc;
}

/* Binary search over keys, which every table holds sorted by key -- hanja/
 * by syllable, krdict/ and idioms/ by ko, all in UTF-8 byte order because
 * that is what the converters sorted with. 34,143 records rules out a scan. */
static int lower_bound(const sug_table *t, const char *key) {
  int lo = 0, hi = t->n;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (strcmp(t->v[mid].key, key) < 0) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

static void push(sug_kind kind, const char *text, const char *detail) {
  if (g_list.n >= SUG_MAX) return;
  if (g_list.n >= g_list.cap) {
    int cap = g_list.cap ? g_list.cap * 2 : 32;
    if (cap > SUG_MAX) cap = SUG_MAX;
    sug_item *v = realloc(g_list.v, (size_t)cap * sizeof *v);
    if (!v) return;
    g_list.v = v;
    g_list.cap = cap;
  }
  sug_item *it = &g_list.v[g_list.n++];
  it->kind = kind;
  snprintf(it->text, sizeof it->text, "%s", text ? text : "");
  snprintf(it->detail, sizeof it->detail, "%s", detail ? detail : "");
}

static void list_reset(void) {
  g_list.n = 0; /* the buffer is reused; nothing to free */
}

/* Resolve the search dirs for the suggest tables. Prefers the dirs recorded
 * on the context (see hunmin_ctx.data_dir); falls back to the env var / build
 * default, which is how the fcitx5 addon and the test harness reach the data.
 * Colon-separated, exactly as hm_data_load's caller spelled them. */
static int resolve_dirs(const char *const **out, const hunmin_ctx *ctx) {
  static char envbuf[4096];
  static const char *mydirs[16];
  /* Prefer the directory the context was built from: a frontend that passes
   * an explicit path (the Android IME, in an app sandbox with no
   * HUNMIN_DATA_PATH) must get its Hanja/idiom/krdict tables from the same
   * place as its layouts. Fall back to the env var / build default only when
   * the context carries no explicit dir, so the fcitx5 addon and the test
   * harness -- which set the env var -- keep their existing behaviour. */
  const char *env = (ctx && ctx->data_dir && *ctx->data_dir)
                        ? ctx->data_dir
                        : getenv("HUNMIN_DATA_PATH");
  if (!env || !*env) {
#ifdef HUNMIN_DEFAULT_DATA_DIR
    env = HUNMIN_DEFAULT_DATA_DIR;
#else
    env = "./libhunmin/data";
#endif
  }
  snprintf(envbuf, sizeof envbuf, "%s", env);
  int n = 0;
  for (char *tok = strtok(envbuf, ":"); tok && n < 16; tok = strtok(NULL, ":"))
    mydirs[n++] = tok;
  *out = mydirs;
  return n;
}

/* Hangul syllables and the Jamo block are all >= 0x80 in UTF-8, and a
 * three-byte syllable has exactly one lead byte. Count UTF-8 sequences rather
 * than bytes: counting the high bytes of a single syllable gives 3, not 1,
 * which would open every gate for every syllable. */
static int syllables(const char *s) {
  int n = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p < 0x80) continue;
    n++;
    if ((*p & 0xE0) == 0xC0) p++; /* 2-byte: one continuation */
    else if ((*p & 0xF0) == 0xE0) p += 2; /* 3-byte: two continuations */
    else if ((*p & 0xF8) == 0xF0) p += 3; /* 4-byte: three */
  }
  return n;
}

int hunmin_suggest_count(hunmin_ctx *ctx, const char *hangul) {
  list_reset();
  /* ctx selects the data dirs (ctx->data_dir when set) and a NULL one is a
   * miss rather than a lookup: a frontend that has torn its context down
   * should see no candidates, not a list served from the cache. The tables
   * themselves stay process-wide and are loaded on first use. */
  if (!ctx || !hangul || !*hangul) return 0;

  const char *const *dirs = NULL;
  int n = resolve_dirs(&dirs, ctx);
  if (n <= 0) return 0;

  /* Idioms first: a four-character idiom carries a meaning and should not be
   * buried under syllable candidates. The idiom table alone is gated on
   * length; krdict is not, because it holds 527 one-character entries that
   * are real words (壽 "a long life", 手 "a game of go" for 수). */
  int syl = syllables(hangul);

  if (syl >= 4 && ensure(&g_idiom, dirs, n, "idioms", "idioms", SK_IDIOM) > 0) {
    for (int i = lower_bound(&g_idiom, hangul);
         i < g_idiom.n && !strcmp(g_idiom.v[i].key, hangul); i++)
      push(SK_IDIOM, g_idiom.v[i].text, g_idiom.v[i].detail);
  }
  if (ensure(&g_word, dirs, n, "krdict", "krdict", SK_WORD) > 0) {
    for (int i = lower_bound(&g_word, hangul);
         i < g_word.n && !strcmp(g_word.v[i].key, hangul); i++)
      push(SK_WORD, g_word.v[i].text, g_word.v[i].detail);
  }
  if (ensure(&g_hanja, dirs, n, "hanja", "hanja", SK_HANJA) > 0) {
    for (int i = lower_bound(&g_hanja, hangul);
         i < g_hanja.n && !strcmp(g_hanja.v[i].key, hangul); i++)
      push(SK_HANJA, g_hanja.v[i].text, g_hanja.v[i].detail);
  }
  return g_list.n;
}

const char *hunmin_suggest_text(hunmin_ctx *ctx, int i) {
  if (!ctx) return "";
  return (g_list.v && i >= 0 && i < g_list.n) ? g_list.v[i].text : "";
}

const char *hunmin_suggest_detail(hunmin_ctx *ctx, int i) {
  if (!ctx) return "";
  return (g_list.v && i >= 0 && i < g_list.n) ? g_list.v[i].detail : "";
}

const char *hunmin_suggest_kind(hunmin_ctx *ctx, int i) {
  if (!ctx) return "";
  if (!g_list.v || i < 0 || i >= g_list.n) return "";
  switch (g_list.v[i].kind) {
  case SK_HANJA: return "hanja";
  case SK_WORD: return "word";
  case SK_IDIOM: return "idiom";
  default: return "";
  }
}