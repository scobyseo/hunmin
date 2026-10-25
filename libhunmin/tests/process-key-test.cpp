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
 *
 * GoogleTest form (issue #109, option 1b): one original section = one TEST_F
 * case, and the executable is still ONE ctest entry (engine-process-key),
 * because registration shape -- not the framework -- is what the 30 s
 * inner-loop budget is sensitive to (docs/TEST-FRAMEWORK-OPTIONS.md §2).
 */
#include "hunmin.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace {

/* Legacy (case-encoding) character an (base, shift) event resolves to —
 * the loader's own US-QWERTY map (schema_loader.c BASE/SHIFTED, 2026-10-18 tree),
 * spelled out independently. */
uint32_t legacy_ch(uint32_t base, uint32_t shift) {
    static const char *BASE[] = {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[", "]", "\\", ";", "'", ",", ".", "/", NULL};
    static const char *SHIFTED[] = {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "{", "}", "|", ":", "\"", "<", ">", "?", NULL};
    if (!shift) return base;
    if (base >= 'a' && base <= 'z') return base - 'a' + 'A';
    if (base < 128)
        for (int i = 0; BASE[i]; i++)
            if (BASE[i][0] == (char)base) return (uint32_t)SHIFTED[i][0];
    return base; /* already-resolved rule: nothing to shift (SPEC-P2) */
}

class ProcessKeyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const char *dirs[] = {"libhunmin/data"};
        ctx_ = hunmin_ctx_new(dirs, 1);
        ASSERT_NE(ctx_, nullptr);
    }
    void TearDown() override { hunmin_ctx_free(ctx_); }

    hunmin_ctx *ctx_ = nullptr;
};

/* One event through both front doors; every difference names WHERE it
 * diverged, not just that it did. */
void equate(hunmin_ic *legacy, hunmin_ic *key, uint32_t base, uint32_t shift, const char *layout) {
    uint32_t want = legacy_ch(base, shift);
    bool rl = hunmin_ic_process(legacy, want);
    bool rk = hunmin_ic_process_key(key, base, shift);
    const char *lc = hunmin_ic_commit(legacy), *lp = hunmin_ic_preedit(legacy);
    const char *kc = hunmin_ic_commit(key), *kp = hunmin_ic_preedit(key);
    EXPECT_STREQ(lc, kc) << layout << ": process_key(base=" << base << ", shift=" << shift << ") commit diverges from process(" << want << ")";
    EXPECT_STREQ(lp, kp) << layout << ": process_key(base=" << base << ", shift=" << shift << ") preedit diverges from process(" << want << ")";
    EXPECT_EQ(rl, rk) << layout << ": process_key(base=" << base << ", shift=" << shift << ") consumption diverges from process(" << want << ")";
}

/* --- equality sweep: every layout, every reachable (base, shift) row --- */
TEST_F(ProcessKeyTest, EqualitySweepOverEveryLayoutAndRow) {
    int n = hunmin_ctx_layout_count(ctx_);
    ASSERT_GT(n, 0) << "the data set ships at least one layout";
    for (int i = 0; i < n; i++) {
        const char *id = hunmin_ctx_layout_id(ctx_, i);
        hunmin_ic *a = hunmin_ic_new(ctx_, id), *b = hunmin_ic_new(ctx_, id);
        ASSERT_TRUE(a != NULL && b != NULL) << (id ? id : "?");
        for (uint32_t base = 0; base < 128; base++)
            for (uint32_t shift = 0; shift <= 1; shift++) equate(a, b, base, shift, id ? id : "?");
        hunmin_ic_free(a);
        hunmin_ic_free(b);
    }
}

/* --- already-resolved rule, named so a red run says WHAT was violated ---
 * A shifted form ('A', '!') passed as base must reach the engine unchanged,
 * and the shift bit must select nothing further on it — the fcitx5-shaped
 * host case. On dubeolsik, 'A' is the shifted row of 'g' (ㄲ); both arms
 * must therefore type the same thing. */
TEST_F(ProcessKeyTest, AlreadyResolvedShiftedBaseReachesTheEngineUnchanged) {
    hunmin_ic *a = hunmin_ic_new(ctx_, "dubeolsik");
    hunmin_ic *b = hunmin_ic_new(ctx_, "dubeolsik");
    ASSERT_TRUE(a && b) << "dubeolsik ics";
    bool ra = hunmin_ic_process(a, 'A');
    bool rb = hunmin_ic_process_key(b, 'A', 1);
    const char *pa = hunmin_ic_preedit(a)[0] ? hunmin_ic_preedit(a) : hunmin_ic_commit(a);
    const char *pb = hunmin_ic_preedit(b)[0] ? hunmin_ic_preedit(b) : hunmin_ic_commit(b);
    EXPECT_EQ(ra, rb) << "'A' with the shift bit: same consumption as legacy";
    EXPECT_STREQ(pa, pb) << "'A' with the shift bit: same output as legacy";
    hunmin_ic_free(a);
    hunmin_ic_free(b);
}

/* A test-phase prediction must equal the real call and change nothing. The
 * TSF adapter uses this to answer OnTestKeyDown without eating a key the
 * engine would decline; a peek that mutated commit/preedit/tier would make
 * the following real key diverge from the twin that was never peeked. */
TEST_F(ProcessKeyTest, PeekKeyMatchesProcessWithoutChangingState) {
    const char *ids[] = {"dubeolsik", "sin3-p2"};
    const uint32_t keys[] = {'1', 'd'};
    for (const char *id : ids) {
        for (uint32_t base : keys) {
            hunmin_ic *a = hunmin_ic_new(ctx_, id);
            hunmin_ic *b = hunmin_ic_new(ctx_, id);
            ASSERT_TRUE(a && b) << id;
            for (const char *p = "dkssud"; *p; ++p) {
                ASSERT_EQ(hunmin_ic_process(a, (uint32_t)*p), hunmin_ic_process(b, (uint32_t)*p)) << id << ": prefix diverged";
            }
            std::string preA = hunmin_ic_preedit(a);
            bool peeked = hunmin_ic_peek_key(a, base, 0);
            EXPECT_STREQ(hunmin_ic_preedit(a), preA.c_str()) << id << ": peek(" << base << ") changed the preedit";
            bool real = hunmin_ic_process_key(b, base, 0);
            EXPECT_EQ(peeked, real) << id << ": peek(" << base << ") disagrees with process_key";
            EXPECT_STREQ(hunmin_ic_preedit(a), preA.c_str()) << id << ": peek(" << base << ") changed the state before the real key";

            bool processed = hunmin_ic_process_key(a, base, 0);
            EXPECT_EQ(processed, real) << id << ": post-peek process_key diverged";
            EXPECT_STREQ(hunmin_ic_preedit(a), hunmin_ic_preedit(b)) << id << ": post-peek preedit diverged";
            EXPECT_STREQ(hunmin_ic_commit(a), hunmin_ic_commit(b)) << id << ": post-peek commit diverged";
            hunmin_ic_free(a);
            hunmin_ic_free(b);
        }
    }
    EXPECT_FALSE(hunmin_ic_peek_key(NULL, 'a', 0)) << "NULL ic -> false, never a crash";
}

/* --- engine edges the sweep cannot reach --- */
TEST_F(ProcessKeyTest, EngineEdgesTheSweepCannotReach) {
    EXPECT_FALSE(hunmin_ic_process_key(NULL, 'a', 1)) << "NULL ic -> false, never a crash";
    {
        /* A codepoint outside the US-QWERTY keyspace arrives already resolved:
         * the sweep covers 0..127, this covers the host that still hands over a
         * wide character. dubeolsik maps nothing there, so both arms must refuse
         * it identically and leave the composition untouched (typed '하' first,
         * then the wide character must not disturb it). */
        hunmin_ic *a = hunmin_ic_new(ctx_, "dubeolsik");
        hunmin_ic *b = hunmin_ic_new(ctx_, "dubeolsik");
        ASSERT_TRUE(a && b) << "dubeolsik ics (wide char)";
        hunmin_ic_process(a, 'g');
        hunmin_ic_process(a, 'k'); /* 하 in flight */
        hunmin_ic_process(b, 'g');
        hunmin_ic_process(b, 'k');
        bool ra = hunmin_ic_process(a, 0xAC00);
        bool rb = hunmin_ic_process_key(b, 0xAC00, 1);
        EXPECT_EQ(ra, rb) << "wide base: same consumption as legacy";
        EXPECT_STREQ(hunmin_ic_preedit(a), hunmin_ic_preedit(b)) << "wide base: composition state identical to legacy";
        hunmin_ic_free(a);
        hunmin_ic_free(b);
    }
}

} // namespace
