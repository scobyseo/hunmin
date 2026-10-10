/* gen-golden-libhangul — deterministic keystroke transcripts from the official
 * libhangul engine, used as reference goldens for hunmin's shared layouts.
 * Links against system libhangul; output must be byte-stable across runs. */
#include <hangul.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t s; } rng_t;
static uint64_t next(rng_t *r) {
  r->s += 0x9E3779B97F4A7C15ull;
  uint64_t z = r->s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

static int utf8(uint32_t cp, char *o) {
  if (cp < 0x80) { o[0] = (char)cp; return 1; }
  if (cp < 0x800) { o[0] = 0xC0 | cp >> 6; o[1] = 0x80 | (cp & 0x3F); return 2; }
  if (cp < 0x10000) { o[0] = 0xE0 | cp >> 12; o[1] = 0x80 | (cp >> 6 & 0x3F);
                      o[2] = 0x80 | (cp & 0x3F); return 3; }
  o[0] = 0xF0 | cp >> 18; o[1] = 0x80 | (cp >> 12 & 0x3F);
  o[2] = 0x80 | (cp >> 6 & 0x3F); o[3] = 0x80 | (cp & 0x3F); return 4;
}

static void put_json_str(FILE *f, const ucschar *s) {
  fputc('"', f);
  for (; s && *s; s++) {
    uint32_t c = *s;
    char b[4];
    if (c == '"' || c == '\\') { fputc('\\', f); fputc((int)c, f); continue; }
    if (c == '\n') { fputs("\\n", f); continue; }
    int n = utf8(c, b);
    for (int i = 0; i < n; i++) fputc(b[i], f);
  }
  fputc('"', f);
}

int main(int argc, char **argv) {
  const char *kbd = NULL, *out = NULL, *lay = NULL;
  uint64_t seed = 1;
  int steps = 400;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--kbd") && ++i < argc) kbd = argv[i];
    else if (!strcmp(argv[i], "--seed") && ++i < argc) seed = strtoull(argv[i], NULL, 10);
    else if (!strcmp(argv[i], "--steps") && ++i < argc) steps = atoi(argv[i]);
    else if (!strcmp(argv[i], "--out") && ++i < argc) out = argv[i];
    else if (!strcmp(argv[i], "--layout") && ++i < argc) lay = argv[i];
  }
  if (!kbd || !out) { fprintf(stderr, "usage: %s --kbd ID --seed N --steps N --out FILE\n", argv[0]); return 2; }

  HangulInputContext *ic = hangul_ic_new(kbd);
  if (!ic) { fprintf(stderr, "unknown keyboard %s\n", kbd); return 1; }
  rng_t r = { seed * 2654435761u + 7 };
  FILE *f = fopen(out, "w");
  if (!f) { perror(out); return 1; }

  fprintf(f, "{\"meta\":{\"layout\":\"%s\",\"kbd\":\"%s\",\"seed\":%llu, \"steps\":%d,\"tool\":\"gen-golden-libhangul\",\"lib\":\"0.2.0\"}}\n",
          lay ? lay : "", kbd, (unsigned long long)seed, steps);

  for (int i = 0; i < steps; i++) {
    bool bs = (i % 13) == 12;
    int ch = bs ? 0 : 0x20 + (int)(next(&r) % 95);
    bool consumed = bs ? hangul_ic_backspace(ic) : hangul_ic_process(ic, ch);
    (void)consumed;
    fputs("{\"in\":", f);
    if (bs) fputs("{\"key\":\"backspace\"}", f);
    else {
      fputs("{\"ch\":\"", f);
      if (ch == '"' || ch == '\\') fputc('\\', f);
      fputc(ch, f);
      fputs("\"}", f);
    }
    fputs(",\"commit\":", f);
    put_json_str(f, hangul_ic_get_commit_string(ic));
    fputs(",\"preedit\":", f);
    put_json_str(f, hangul_ic_get_preedit_string(ic));
    fputs("}\n", f);
  }
  fclose(f);
  hangul_ic_delete(ic);
  return 0;
}
