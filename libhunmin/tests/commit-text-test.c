/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier A: hunmin_ic_commit_text() -- external text pass-through (PLAN.md
 * invariant 5, which until now stated the intent and named a function that did
 * not exist).
 *
 * The property that matters is negative. Voice text must arrive verbatim, and
 * the strongest way to show it was NOT re-composed is to hand it text the jamo
 * pipeline would mangle: raw conjoined jamo, ASCII that dubeolsik would turn
 * into Hangul, a lone choseong with no vowel. If any of it came back composed,
 * the pass-through has become a keystroke injection and everything here fails.
 *
 * The UTF-8 rejections are not defensive decoration. Speech APIs cut off
 * mid-character on a timeout, so a truncated tail is the expected input, not an
 * exotic one; committing the valid prefix would put a broken string in the
 * user's document with nothing to indicate it.
 */
#include "hunmin.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int cond, const char *what) {
  if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

static void expect_str(const char *got, const char *want, const char *what) {
  if (!got || strcmp(got, want)) {
    printf("FAIL: %s\n  expected [%s]\n  got      [%s]\n", what, want,
           got ? got : "(null)");
    failures++;
  }
}

static hunmin_ic *fresh(hunmin_ctx *ctx, const char *layout) {
  hunmin_ic *ic = hunmin_ic_new(ctx, layout);
  check(ic != NULL, "hunmin_ic_new");
  return ic;
}

/* A composed Hangul syllable is 3 UTF-8 bytes with a 0xE0-masked lead. */
static int has_hangul(const char *s) {
  for (const unsigned char *q = (const unsigned char *)s; *q; q++)
    if ((*q & 0xE0) == 0xE0) return 1;
  return 0;
}

int main(void) {
  const char *dirs[] = {"libhunmin/data"};
  hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
  check(ctx != NULL, "ctx_new");
  if (!ctx) return 1;

  /* --- verbatim, not composed --- */
  {
    hunmin_ic *ic = fresh(ctx, "dubeolsik");
    /* dubeolsik types "dkssud" into 안녕. Handed over as text it must stay those
     * five ASCII characters: the pipeline must not see it at all. */
    expect_str(hunmin_ic_commit_text(ic, "dkssud"), "dkssud",
               "ASCII passes through uncomposed");
    /* Conjoined jamo (U+1100 block): every key here is normally a jamo source. */
    expect_str(hunmin_ic_commit_text(ic, "\xE1\x84\x80\xE1\x85\xA1"),
               "\xE1\x84\x80\xE1\x85\xA1", "raw conjoined jamo is not recomposed");
    /* A lone choseong and a lone jungseong: both are valid text, neither is a
     * syllable, and dubeolsik would happily absorb them. */
    expect_str(hunmin_ic_commit_text(ic, "\xE1\x84\x80"), "\xE1\x84\x80",
               "lone choseong survives");
    expect_str(hunmin_ic_commit_text(ic, "\xE1\x85\xA1"), "\xE1\x85\xA1",
               "lone jungseong survives");
    /* A real sentence, punctuation, and a 4-byte sequence (U+1F600) -- the
     * widest thing the validator has to accept. */
    expect_str(hunmin_ic_commit_text(ic, "안녕하세요, 반갑습니다! \xF0\x9F\x98\x80"),
               "안녕하세요, 반갑습니다! \xF0\x9F\x98\x80",
               "a spoken sentence with punctuation and an emoji");
    expect_str(hunmin_ic_preedit(ic), "", "pass-through leaves no preedit");
    hunmin_ic_free(ic);
  }

  /* --- composes nothing, consumes no keystroke --- */
  {
    hunmin_ic *ic = fresh(ctx, "dubeolsik");
    hunmin_ic_process(ic, 'g');            /* ㄱ as cho */
    hunmin_ic_process(ic, 'k');            /* ㅏ       -> 하 */
    expect_str(hunmin_ic_preedit(ic), "하", "typed a syllable first");
    /* The typed syllable comes out first, then the spoken text, in that order. */
    expect_str(hunmin_ic_commit_text(ic, "음성"), "하음성",
               "in-flight composition precedes external text");
    expect_str(hunmin_ic_preedit(ic), "", "composition cleared, not left hanging");
    /* And the ic still works afterwards: this is a pass-through, not a reset. */
    hunmin_ic_process(ic, 'g');
    hunmin_ic_process(ic, 'k');
    expect_str(hunmin_ic_preedit(ic), "하", "engine still usable after pass-through");
    hunmin_ic_free(ic);
  }

  /* --- tier state is untouched --- */
  {
    hunmin_ic *ic = fresh(ctx, "sin3-p2");
    /* Enter the symbol tier, then hand it external text. The tier is *input*
     * state: a pass-through is not input, so it must not end the tier. Prove it
     * by finishing the tier afterwards -- if the pass-through had cleared T, the
     * tier id would be gone and this key would compose a syllable instead of
     * emitting the tier's symbol. */
    hunmin_ic_process(ic, 'j');
    hunmin_ic_process(ic, 'k');
    hunmin_ic_commit_text(ic, "가나다");
    hunmin_ic_process(ic, '1');
    const char *sym = hunmin_ic_commit(ic);
    check(sym && *sym, "tier survived the pass-through (emitted something)");
    check(!has_hangul(sym ? sym : ""),
          "tier emitted a symbol, not a composed Hangul syllable");
    hunmin_ic_free(ic);
  }

  /* --- longer than one composition buffer --- */
  {
    hunmin_ic *ic = fresh(ctx, "dubeolsik");
    /* STR_CAP is 64 ucschar, so a voice sentence comfortably exceeds whatever a
     * single keystroke commit can hold. Nothing may be truncated. */
    char big[1024];
    size_t n = 0;
    for (int i = 0; i < 200; i++) { memcpy(big + n, "가", 3); n += 3; }
    big[n] = 0;
    const char *got = hunmin_ic_commit_text(ic, big);
    check(got && strlen(got) == n, "a 600-byte sentence is not truncated");
    check(got && memcmp(got, big, n) == 0, "and arrives byte-identical");
    /* A second, shorter call must not be padded or stale-tailed by the first. */
    expect_str(hunmin_ic_commit_text(ic, "짧게"), "짧게",
               "a later call replaces, it does not append");
    hunmin_ic_free(ic);
  }

  /* --- rejections: never NULL, never mojibake --- */
  {
    hunmin_ic *ic = fresh(ctx, "dubeolsik");
    expect_str(hunmin_ic_commit_text(ic, NULL), "", "NULL text -> \"\"");
    expect_str(hunmin_ic_commit_text(ic, ""), "", "empty text -> \"\"");
    expect_str(hunmin_ic_commit_text(NULL, "가"), "", "NULL ic -> \"\", never a crash");
    /* Truncated 3-byte sequence: what a speech timeout actually produces. */
    expect_str(hunmin_ic_commit_text(ic, "가\xE1\x84"), "", "truncated tail rejected");
    /* Lone continuation byte. */
    expect_str(hunmin_ic_commit_text(ic, "\x80"), "", "stray continuation rejected");
    /* Overlong encoding of '/' (U+002F in two bytes). */
    expect_str(hunmin_ic_commit_text(ic, "\xC0\xAF"), "", "overlong rejected");
    /* Surrogate half U+D800. */
    expect_str(hunmin_ic_commit_text(ic, "\xED\xA0\x80"), "", "surrogate rejected");
    /* Beyond U+10FFFF. */
    expect_str(hunmin_ic_commit_text(ic, "\xF5\x80\x80\x80"), "", "out of range rejected");
    /* Rejected input must leave the engine exactly as it was. */
    hunmin_ic_process(ic, 'g');
    hunmin_ic_process(ic, 'k');
    expect_str(hunmin_ic_preedit(ic), "하", "rejected text does not disturb state");
    hunmin_ic_free(ic);
  }

  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("commit_text: all assertions hold\n");
  return 0;
}