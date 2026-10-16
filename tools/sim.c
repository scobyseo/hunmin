/* hunmin-sim — replay/inspect/check layouts. Usage:
 *   hunmin-sim --data DIR --layout ID --keys 'gqqw' [--trace]
 *   hunmin-sim --data DIR --check tests/golden/ref-dubeolsik.jsonl
 * --check drives the engine with each event's "in" and compares commit and
 * preedit exactly (tier A parity gate). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "hunmin.h"

/* Escape a string into a JSON string body (no surrounding quotes).
 *
 * Writes at most cap-1 bytes plus a NUL and stops early rather than truncating
 * mid-escape: a half-written \u sequence would be worse than a shortened
 * string, because it would still be invalid JSON. */
static void json_escape(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 7 < cap; p++) {
    switch (*p) {
      case '"':  out[o++] = '\\'; out[o++] = '"';  break;
      case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
      case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
      case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
      case '\t': out[o++] = '\\'; out[o++] = 't';  break;
      default:
        if (*p < 0x20) o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p);
        else out[o++] = (char)*p;
    }
  }
  out[o] = 0;
}
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, long *szp) {
  FILE *f = fopen(path, "rb");
  if (!f) { perror(path); return NULL; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)sz + 1);
  if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
  buf[sz] = 0;
  fclose(f);
  if (szp) *szp = sz;
  return buf;
}

/* expand Hangul syllables to jamo (canonical equivalence within our alphabet:
 * NFC "가" and jamo "가" compare equal — patim assertions mix the two) */
static void put_cp(char *o, size_t *n, size_t cap, uint32_t cp) {
  char b[4];
  int l;
  if (cp < 0x80) { b[0] = (char)cp; l = 1; }
  else if (cp < 0x800) { b[0] = (char)(0xC0 | cp >> 6); b[1] = (char)(0x80 | (cp & 0x3F)); l = 2; }
  else if (cp < 0x10000) { b[0] = (char)(0xE0 | cp >> 12); b[1] = (char)(0x80 | (cp >> 6 & 0x3F));
                            b[2] = (char)(0x80 | (cp & 0x3F)); l = 3; }
  else { b[0] = (char)(0xF0 | cp >> 18); b[1] = (char)(0x80 | (cp >> 12 & 0x3F));
         b[2] = (char)(0x80 | (cp >> 6 & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); l = 4; }
  if (*n + (size_t)l < cap - 1) { for (int i = 0; i < l; i++) o[(*n)++] = b[i]; }
}

static void hangul_canon(const char *s, char *out, size_t cap) {
  size_t o = 0;
  while (*s && o + 12 < cap) {
    unsigned char c = (unsigned char)*s;
    uint32_t cp;
    int l;
    if (c < 0x80) { cp = c; l = 1; }
    else if ((c >> 5) == 6) { cp = c & 0x1F; l = 2; }
    else if ((c >> 4) == 0xE) { cp = c & 0xF; l = 3; }
    else { cp = c & 7; l = 4; }
    for (int i = 1; i < l; i++) cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
    s += l;
    if (cp >= 0xAC00 && cp <= 0xD7A3) {
      uint32_t x = cp - 0xAC00, t = x % 28;
      put_cp(out, &o, cap, 0x1100 + x / 588);
      put_cp(out, &o, cap, 0x1161 + x % 588 / 28);
      if (t) put_cp(out, &o, cap, 0x11A8 + t - 1);
    } else {
      put_cp(out, &o, cap, cp);
    }
  }
  out[o] = 0;
}

static int check_file(hunmin_ctx *ctx, const char *path, const char *force_layout) {
  char *txt = read_file(path, NULL);
  if (!txt) return 2;
  char *save = NULL;
  int line = 0, fails = 0;
  hunmin_ic *ic = NULL;

  for (char *s = strtok(txt, "\n"); s; s = strtok(NULL, "\n")) {
    line++;
    if (!*s) continue;
    cJSON *j = cJSON_Parse(s);
    if (!j) { fprintf(stderr, "%s:%d: bad JSON\n", path, line); fails++; continue; }
    const cJSON *meta = cJSON_GetObjectItemCaseSensitive(j, "meta");
    if (meta) {
      const cJSON *ca = cJSON_GetObjectItemCaseSensitive(meta, "caps");
      static const char *ovrs[] = {"w", "W", "z", "h", "H"};
      if (ic) {
        hunmin_ic_reset(ic);  /* multi-case goldens: reset between cases */
        if (cJSON_IsNumber(ca)) hunmin_ic_set_option(ic, "caps", ca->valueint);
        for (int k = 0; k < 5; k++) {
          const cJSON *cv = cJSON_GetObjectItemCaseSensitive(meta, ovrs[k]);
          if (cJSON_IsNumber(cv)) hunmin_ic_set_option(ic, ovrs[k], cv->valueint);
        }
      }
      if (!ic) {
        const cJSON *lay = cJSON_GetObjectItemCaseSensitive(meta, "layout");
        const char *use = force_layout;
        if (!use && cJSON_IsString(lay) && lay->valuestring[0]) use = lay->valuestring;
        if (use) {
          ic = hunmin_ic_new(ctx, use);
          const cJSON *caps = cJSON_GetObjectItemCaseSensitive(meta, "caps");
          if (ic && cJSON_IsNumber(caps))
            hunmin_ic_set_option(ic, "caps", caps->valueint);
          if (ic)
            for (int k = 0; k < 5; k++) {
              const cJSON *cv = cJSON_GetObjectItemCaseSensitive(meta, ovrs[k]);
              if (cJSON_IsNumber(cv)) hunmin_ic_set_option(ic, ovrs[k], cv->valueint);
            }
        }
        if (!use || !ic) {
          fprintf(stderr, "%s: cannot resolve layout from meta/--layout\n", path);
          cJSON_Delete(j); fails++; break;
        }
      }
      cJSON_Delete(j);
      continue;
    }
    const cJSON *in = cJSON_GetObjectItemCaseSensitive(j, "in");
    const cJSON *wcommit = cJSON_GetObjectItemCaseSensitive(j, "commit");
    const cJSON *wpre = cJSON_GetObjectItemCaseSensitive(j, "preedit");
    if (!ic) {
      fprintf(stderr, "%s: missing meta.layout (line 1) or --layout\n", path);
      cJSON_Delete(j); fails++; break;
    }
    uint32_t ch = 0x08; /* default backspace */
    const cJSON *jc = cJSON_GetObjectItemCaseSensitive(in, "ch");
    const cJSON *jk = cJSON_GetObjectItemCaseSensitive(in, "key");
    if (cJSON_IsString(jc) && jc->valuestring[0]) ch = (unsigned char)jc->valuestring[0];
    else if (cJSON_IsString(jk) && !strcmp(jk->valuestring, "backspace")) ch = 0x08;
    else { fprintf(stderr, "%s:%d: bad in\n", path, line); cJSON_Delete(j); fails++; continue; }

    hunmin_ic_process(ic, ch);
    const char *ac = hunmin_ic_commit(ic), *ap = hunmin_ic_preedit(ic);
    const cJSON *wtotal = cJSON_GetObjectItemCaseSensitive(j, "total");
    if (cJSON_IsString(wtotal)) {   /* oracle mode: assert the rendered result of
                                     * this step: composing buffer, else the commit
                                     * (patim getComposed() semantics, canonicalized:
                                     * NFC syllables == jamo sequences here) */
      const char *whole = ap[0] ? ap : ac;
      char a_c[1200], b_c[1200];
      hangul_canon(whole, a_c, sizeof a_c);
      hangul_canon(wtotal->valuestring, b_c, sizeof b_c);
      if (strcmp(a_c, b_c)) {
        fprintf(stderr, "%s:%d: TOTAL FAIL in=%s expected=%s got=%s\n",
                path, line, jc && cJSON_IsString(jc) ? jc->valuestring : "bksp",
                b_c, a_c);
        fails++;
        if (fails > 8) { cJSON_Delete(j); break; }
      }
      cJSON_Delete(j);
      continue;
    }
    const char *ec = cJSON_IsString(wcommit) ? wcommit->valuestring : "";
    const char *ep = cJSON_IsString(wpre) ? wpre->valuestring : "";
    if (strcmp(ac, ec) || strcmp(ap, ep)) {
      fprintf(stderr, "%s:%d: MISMATCH in=%s expected commit='%s' preedit='%s' got commit='%s' preedit='%s'\n",
              path, line, jc && cJSON_IsString(jc) ? jc->valuestring : "backspace",
              ec, ep, ac, ap);
      fails++;
      if (fails > 5) { cJSON_Delete(j); break; }
    }
    cJSON_Delete(j);
  }
  if (ic) hunmin_ic_free(ic);
  free(txt);
  if (!fails) printf("OK  %s (%d events)\n", path, line - 1);
  return fails ? 1 : 0;
}

int main(int argc, char **argv) {
  const char *data = NULL, *layout = NULL, *keys = NULL, *checkp = NULL;
  int trace = 0, caps = 0, batch = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--data") && ++i < argc) data = argv[i];
    else if (!strcmp(argv[i], "--layout") && ++i < argc) layout = argv[i];
    else if (!strcmp(argv[i], "--keys") && ++i < argc) keys = argv[i];
    else if (!strcmp(argv[i], "--check") && ++i < argc) checkp = argv[i];
    else if (!strcmp(argv[i], "--caps")) caps = 1;
    else if (!strcmp(argv[i], "--trace")) trace = 1;
    else if (!strcmp(argv[i], "--batch")) batch = 1;
  }
  if (!data) { fprintf(stderr, "usage: hunmin-sim --data DIR (--layout ID --keys STR | --batch | --check FILE)\n"); return 2; }
  const char *dirs[1] = { data };
  hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
  if (!ctx) { fprintf(stderr, "no layouts loaded from %s\n", data); return 2; }

  if (checkp) return check_file(ctx, checkp, layout);

  if (batch) {
    /* One keystroke string per input line; print the rendered result as JSON.
     *
     * A keystroke search needs hundreds of thousands of engine evaluations,
     * and one process spawn per keystroke costs ~3.9 ms -- process startup,
     * not the engine, dominates and a search takes hours. Reading many
     * keystrokes per process makes that cost amortise away. Context is
     * loaded once; each line gets a fresh ic so lines cannot influence each
     * other, and an empty line resets. */
    if (!layout) { fprintf(stderr, "need --layout for --batch\n"); return 2; }
    char linebuf[4096];
    char *line = linebuf;
    size_t n;
    while (fgets(line, (int)sizeof(linebuf), stdin)) {
      n = strlen(line);
      while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
      hunmin_ic *bic = hunmin_ic_new(ctx, layout);
      if (!bic) { fprintf(stderr, "layout %s unavailable\n", layout); return 2; }
      if (caps) hunmin_ic_set_option(bic, "caps", 1);
      /* Accumulate the commit after EVERY key, not once at the end:
       * hunmin_ic_process() clears ic->commit on entry, so reading it once
       * after the loop silently dropped every commit except the last key's.
       * For nkf that reported preedit '가' with an empty commit, hiding the
       * ㅅ that key 2 had already committed -- which then made --batch
       * disagree with --trace. */
      char acc[1024];
      size_t acclen = 0;
      for (ssize_t i = 0; i < n; i++) {
        hunmin_ic_process(bic, (unsigned char)line[i]);
        const char *c = hunmin_ic_commit(bic);
        size_t cl = strlen(c);
        if (cl && acclen + cl < sizeof acc) {
          memcpy(acc + acclen, c, cl);
          acclen += cl;
          acc[acclen] = 0;
        }
      }
      char kbuf[1024], cbuf[1024], pbuf[1024];
      json_escape(line, kbuf, sizeof kbuf);
      json_escape(acc, cbuf, sizeof cbuf);
      json_escape(hunmin_ic_preedit(bic), pbuf, sizeof pbuf);
      printf("{\"keys\":\"%s\",\"commit\":\"%s\",\"preedit\":\"%s\"}\n", kbuf, cbuf, pbuf);
      hunmin_ic_free(bic);
    }
    free(line);
    return 0;
  }

  if (!layout || !keys) { fprintf(stderr, "need --layout and --keys\n"); return 2; }
  hunmin_ic *ic = hunmin_ic_new(ctx, layout);
  if (!ic) { fprintf(stderr, "layout %s unavailable\n", layout); return 2; }
  if (caps) hunmin_ic_set_option(ic, "caps", 1);
  for (const char *s = keys; *s; s++) {
    /* The return value is the engine's CONSUMPTION decision, and --trace threw
     * it away. That made the trace unable to say anything about a key the
     * engine declined -- space above all, which is the one key whose whole
     * point is that it is not consumed. Consumers had to re-derive it from the
     * layout's key table (tools/gen-conformance.py:221 does exactly that),
     * which is a second, weaker source for something the engine already
     * answered. Emitting it here makes tier A the single source for
     * consumption as well as for commit/preedit, which is what the tier-D3
     * expectation needs: a declined key is forwarded to the client, and the
     * client is where the space character has to come from. */
    bool consumed = hunmin_ic_process(ic, (unsigned char)*s);
    if (trace) {
      /* Every string in the line is escaped, not just commit and preedit.
       * The input key went through raw `%c`, so sin3-p2's shifted apostrophe
       * (shift+; , which commits '"') produced
       *   {"in":{"ch":"""},"commit":"ㄱ"","preedit":""}
       * -- invalid JSON, which breaks every consumer of --trace, including
       * tools/gen-conformance.py (tier C corpus) and tools/sin3p2-ksgen.py.
       * The engine was right; the emitter was not. Found while building a
       * keystroke generator that searches the layout's key table, whose
       * alphabet necessarily includes '"' and '\\'. */
      char kbuf[8], cbuf[256], pbuf[256];
      char kraw[2] = { (char)*s, 0 };
      json_escape(kraw, kbuf, sizeof kbuf);
      json_escape(hunmin_ic_commit(ic), cbuf, sizeof cbuf);
      json_escape(hunmin_ic_preedit(ic), pbuf, sizeof pbuf);
      printf("{\"in\":{\"ch\":\"%s\"},\"commit\":\"%s\",\"preedit\":\"%s\","
             "\"consumed\":%s}\n",
             kbuf, cbuf, pbuf, consumed ? "true" : "false");
    }
    else printf("[%c] commit='%s' preedit='%s'\n", *s, hunmin_ic_commit(ic), hunmin_ic_preedit(ic));
  }
  return 0;
}
