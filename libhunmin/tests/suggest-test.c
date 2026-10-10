/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier A: the Hanja candidate API (P3).
 *
 * These assertions are drawn from the data files themselves, not from what the
 * code happens to produce: the expected first candidate for 수 is 水 because
 * libhunmin/data/hanja/hanja.json lists it first, and 가화만사성's Hanja and
 * gloss are the ones in krdict.json. A converter regression that reordered or
 * dropped an entry therefore fails here rather than being papered over.
 *
 * The empty-string and NULL contracts matter as much as the values: a frontend
 * renders the list unconditionally, so an out-of-range index must not crash it.
 */
#include "hunmin.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(hunmin_ctx *ctx, int i, const char *kind,
                   const char *text, const char *detail) {
  const char *k = hunmin_suggest_kind(ctx, i);
  const char *t = hunmin_suggest_text(ctx, i);
  const char *d = hunmin_suggest_detail(ctx, i);
  if (strcmp(k, kind) || strcmp(t, text) || (detail && strcmp(d, detail))) {
    printf("  [%d] expected %s/%s/%s, got %s/%s/%s\n", i, kind, text,
           detail ? detail : "", k, t, d);
    failures++;
  }
}

static void check(int cond, const char *what) {
  if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

int main(void) {
  const char *dirs[] = {"libhunmin/data"};
  hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
  check(ctx != NULL, "ctx");
  if (!ctx) return 1;

  /* --- a single syllable: word senses first (they carry a meaning), then
   * the syllable's Hanja candidates in upstream frequency order --- */
  int n = hunmin_suggest_count(ctx, "수");
  check(n > 0, "수 has candidates");

  /* 水 must be the first *hanja* candidate: hanja.json lists it first. */
  int first_hanja = -1;
  for (int i = 0; i < n; i++)
    if (!strcmp(hunmin_suggest_kind(ctx, i), "hanja")) { first_hanja = i; break; }
  check(first_hanja >= 0, "수 has a hanja candidate");
  if (first_hanja >= 0) {
    expect(ctx, first_hanja, "hanja", "水", "");
    expect(ctx, first_hanja + 1, "hanja", "手", "");
    expect(ctx, first_hanja + 2, "hanja", "數", "");
  }

  /* 한 leads with 韓 in hanja.json (verified: 韓 漢 寒 限 閑 恨). */
  n = hunmin_suggest_count(ctx, "한");
  first_hanja = -1;
  for (int i = 0; i < n; i++)
    if (!strcmp(hunmin_suggest_kind(ctx, i), "hanja")) { first_hanja = i; break; }
  check(first_hanja >= 0, "한 has a hanja candidate");
  if (first_hanja >= 0) expect(ctx, first_hanja, "hanja", "韓", "");

  /* --- a word with a gloss: exactly what krdict.json says --- */
  n = hunmin_suggest_count(ctx, "가화만사성");
  check(n >= 1, "가화만사성 has a candidate");
  if (n >= 1)
    expect(ctx, 0, "word", "家和萬事成", "가정이 화목하면 모든 일이 잘 이루어짐.");

  /* 금시초문 is in both tables -- krdict carries 今始初聞 with its gloss and
   * idioms carries 今時初聞 with the other reading. Four syllables means the
   * idiom table is consulted first, so it wins and the krdict entry follows.
   * Both Hanja forms are real, which is why the data keeps them apart. */
  n = hunmin_suggest_count(ctx, "금시초문");
  check(n >= 2, "금시초문 resolves in both tables");
  if (n >= 2) {
    expect(ctx, 0, "idiom", "今時初聞", "금시초문");
    expect(ctx, 1, "word", "今始初聞", "어떤 이야기를 지금 처음으로 들음.");
  }

  /* --- a four-character idiom, keyed by its Hangul reading --- */
  n = hunmin_suggest_count(ctx, "일석이조");
  check(n >= 1, "일석이조 has a candidate");
  if (n >= 1)
    expect(ctx, 0, "idiom", "一石二鳥", "일석이조");
  /* The same idiom also sits in krdict with its own gloss; the idiom table is
   * queried first, so the idiom entry must win rather than being shadowed. */
  check(n >= 1 && !strcmp(hunmin_suggest_kind(ctx, 0), "idiom"),
        "idiom outranks the krdict entry for the same word");
  /* the same idiom under its Hanja is not how the table is keyed: a lookup
   * by Hanja must not silently resolve */
  check(hunmin_suggest_count(ctx, "一石二鳥") == 0,
        "idioms are keyed by Hangul, not Hanja");

  /* --- ordering and the idiom gate ---
   *
   * Idioms are queried before words, so a four-character idiom with its own
   * reading is not shadowed by the krdict entry for the same word (금시초문
   * is in both, and the idiom wins).
   *
   * The syllable >= 4 gate on the idiom table cannot be exercised against
   * this data: all 2,318 readings are exactly four syllables, so no lookup
   * would reach the idiom table with a shorter key even if the gate were
   * removed. It is kept as a guard for a future table, not as tested
   * behaviour, and a mutation that deletes it survives the suite -- recorded
   * here so that is a known gap rather than a surprise.
   */
  /* krdict is not length-gated: it holds 527 one-character entries and they
   * are real words -- 壽 "a long life" and 手 "a game of go" for 수. A gate at
   * two syllables would hide them. Glossed senses lead the list. */
  n = hunmin_suggest_count(ctx, "수");
  check(n > 0, "수 has candidates");
  int has_word = 0;
  for (int i = 0; i < n; i++)
    if (!strcmp(hunmin_suggest_kind(ctx, i), "word")) has_word = 1;
  check(has_word, "수 keeps its one-syllable word entries (壽, 手)");
  check(!strcmp(hunmin_suggest_kind(ctx, 0), "word"),
        "glossed word senses lead a one-syllable lookup");

  /* --- misses and the never-NULL contract --- */
  check(hunmin_suggest_count(ctx, "없는말xyz") == 0, "unknown input yields 0");
  check(hunmin_suggest_count(ctx, "") == 0, "empty input yields 0");
  check(hunmin_suggest_count(ctx, NULL) == 0, "NULL input yields 0");
  check(hunmin_suggest_count(NULL, "수") == 0, "NULL ctx yields 0");
  check(hunmin_suggest_text(ctx, -1)[0] == 0, "negative index -> empty");
  check(hunmin_suggest_text(ctx, 9999)[0] == 0, "past-the-end -> empty");
  check(hunmin_suggest_detail(ctx, 9999)[0] == 0, "past-the-end detail -> empty");
  check(hunmin_suggest_kind(ctx, 9999)[0] == 0, "past-the-end kind -> empty");

  /* --- the list is replaced, not appended to, by the next lookup --- */
  int a = hunmin_suggest_count(ctx, "수");
  check(hunmin_suggest_count(ctx, "한") != 0, "한 still resolves");
  check(hunmin_suggest_count(ctx, "수") == a, "repeat lookup is stable");
  check(hunmin_suggest_text(ctx, 0)[0] != 0, "count() re-populated the list");

  hunmin_ctx_free(ctx);
  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("suggest: ok\n");
  return 0;
}