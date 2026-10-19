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
 *
 * GoogleTest form (issue #109, option 1b): one original section = one TEST_F
 * case, and the executable is still ONE ctest entry (engine-commit-text),
 * because registration shape -- not the framework -- is what the 30 s
 * inner-loop budget is sensitive to (docs/TEST-FRAMEWORK-OPTIONS.md §2).
 */
#include "hunmin.h"

#include <gtest/gtest.h>

#include <cstring>

namespace {

/* A composed Hangul syllable is 3 UTF-8 bytes with a 0xE0-masked lead. */
bool has_hangul(const char *s) {
  for (const unsigned char *q = (const unsigned char *)s; *q; q++)
    if ((*q & 0xE0) == 0xE0) return true;
  return false;
}

class CommitTextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char *dirs[] = {"libhunmin/data"};
    ctx_ = hunmin_ctx_new(dirs, 1);
    ASSERT_NE(ctx_, nullptr);
  }
  void TearDown() override { hunmin_ctx_free(ctx_); }

  hunmin_ic *fresh(const char *layout) {
    hunmin_ic *ic = hunmin_ic_new(ctx_, layout);
    EXPECT_NE(ic, nullptr) << "hunmin_ic_new(" << layout << ")";
    return ic;
  }

  hunmin_ctx *ctx_ = nullptr;
};

/* --- verbatim, not composed --- */
TEST_F(CommitTextTest, VerbatimNotComposed) {
  hunmin_ic *ic = fresh("dubeolsik");
  ASSERT_NE(ic, nullptr);
  /* dubeolsik types "dkssud" into 안녕. Handed over as text it must stay those
   * five ASCII characters: the pipeline must not see it at all. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "dkssud"), "dkssud")
      << "ASCII passes through uncomposed";
  /* Conjoined jamo (U+1100 block): every key here is normally a jamo source. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xE1\x84\x80\xE1\x85\xA1"),
               "\xE1\x84\x80\xE1\x85\xA1")
      << "raw conjoined jamo is not recomposed";
  /* A lone choseong and a lone jungseong: both are valid text, neither is a
   * syllable, and dubeolsik would happily absorb them. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xE1\x84\x80"), "\xE1\x84\x80")
      << "lone choseong survives";
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xE1\x85\xA1"), "\xE1\x85\xA1")
      << "lone jungseong survives";
  /* A real sentence, punctuation, and a 4-byte sequence (U+1F600) -- the
   * widest thing the validator has to accept. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "안녕하세요, 반갑습니다! \xF0\x9F\x98\x80"),
               "안녕하세요, 반갑습니다! \xF0\x9F\x98\x80")
      << "a spoken sentence with punctuation and an emoji";
  EXPECT_STREQ(hunmin_ic_preedit(ic), "") << "pass-through leaves no preedit";
  hunmin_ic_free(ic);
}

/* --- composes nothing, consumes no keystroke --- */
TEST_F(CommitTextTest, ComposesNothingConsumesNoKeystroke) {
  hunmin_ic *ic = fresh("dubeolsik");
  ASSERT_NE(ic, nullptr);
  hunmin_ic_process(ic, 'g'); /* ㄱ as cho */
  hunmin_ic_process(ic, 'k'); /* ㅏ       -> 하 */
  EXPECT_STREQ(hunmin_ic_preedit(ic), "하") << "typed a syllable first";
  /* The typed syllable comes out first, then the spoken text, in that order. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "음성"), "하음성")
      << "in-flight composition precedes external text";
  EXPECT_STREQ(hunmin_ic_preedit(ic), "") << "composition cleared, not left hanging";
  /* And the ic still works afterwards: this is a pass-through, not a reset. */
  hunmin_ic_process(ic, 'g');
  hunmin_ic_process(ic, 'k');
  EXPECT_STREQ(hunmin_ic_preedit(ic), "하") << "engine still usable after pass-through";
  hunmin_ic_free(ic);
}

/* --- tier state is untouched --- */
TEST_F(CommitTextTest, TierStateUntouched) {
  hunmin_ic *ic = fresh("sin3-p2");
  ASSERT_NE(ic, nullptr);
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
  ASSERT_NE(sym, nullptr);
  EXPECT_NE(*sym, '\0') << "tier survived the pass-through (emitted something)";
  EXPECT_FALSE(has_hangul(sym))
      << "tier emitted a symbol, not a composed Hangul syllable";
  hunmin_ic_free(ic);
}

/* --- longer than one composition buffer --- */
TEST_F(CommitTextTest, LongerThanOneCompositionBuffer) {
  hunmin_ic *ic = fresh("dubeolsik");
  ASSERT_NE(ic, nullptr);
  /* STR_CAP is 64 ucschar, so a voice sentence comfortably exceeds whatever a
   * single keystroke commit can hold. Nothing may be truncated. */
  char big[1024];
  size_t n = 0;
  for (int i = 0; i < 200; i++) {
    memcpy(big + n, "가", 3);
    n += 3;
  }
  big[n] = 0;
  const char *got = hunmin_ic_commit_text(ic, big);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(strlen(got), n) << "a 600-byte sentence is not truncated";
  EXPECT_EQ(memcmp(got, big, n), 0) << "and arrives byte-identical";
  /* A second, shorter call must not be padded or stale-tailed by the first. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "짧게"), "짧게")
      << "a later call replaces, it does not append";
  hunmin_ic_free(ic);
}

/* --- rejections: never NULL, never mojibake --- */
TEST_F(CommitTextTest, RejectionsNeverNullNeverMojibake) {
  hunmin_ic *ic = fresh("dubeolsik");
  ASSERT_NE(ic, nullptr);
  EXPECT_STREQ(hunmin_ic_commit_text(ic, nullptr), "") << "NULL text -> \"\"";
  EXPECT_STREQ(hunmin_ic_commit_text(ic, ""), "") << "empty text -> \"\"";
  EXPECT_STREQ(hunmin_ic_commit_text(nullptr, "가"), "") << "NULL ic -> \"\", never a crash";
  /* Truncated 3-byte sequence: what a speech timeout actually produces. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "가\xE1\x84"), "") << "truncated tail rejected";
  /* Lone continuation byte. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\x80"), "") << "stray continuation rejected";
  /* Overlong encoding of '/' (U+002F in two bytes). */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xC0\xAF"), "") << "overlong rejected";
  /* Surrogate half U+D800. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xED\xA0\x80"), "") << "surrogate rejected";
  /* Beyond U+10FFFF. */
  EXPECT_STREQ(hunmin_ic_commit_text(ic, "\xF5\x80\x80\x80"), "") << "out of range rejected";
  /* Rejected input must leave the engine exactly as it was. */
  hunmin_ic_process(ic, 'g');
  hunmin_ic_process(ic, 'k');
  EXPECT_STREQ(hunmin_ic_preedit(ic), "하") << "rejected text does not disturb state";
  hunmin_ic_free(ic);
}

}  // namespace
