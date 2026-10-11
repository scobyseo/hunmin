/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the layout catalog — mode split and the 가나다순 order the
 * settings window shows.
 *
 * Pure Kotlin, no framework: the ordering is the one rule both frontends must
 * agree on, so it is pinned here rather than in whatever the dialog happens to
 * do with the list it is handed.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LayoutCatalogTest {

    private fun entry(id: String, ko: String, mode: LayoutMode = LayoutMode.HANGUL) =
        LayoutEntry(id, ko, mode)

    // The real names, in the order the engine happens to scan the directory,
    // so this is the same list the device produces.
    private val scanned = listOf(
        entry("ahnmatmae", "안마태"),
        entry("dubeolsik-yet", "두벌식 옛글"),
        entry("dubeolsik", "두벌식"),
        entry("romaja", "로마자"),
        entry("sebeolsik-3-2", "세벌식 두벌자판"),
        entry("sebeolsik-3-90", "세벌식 390"),
        entry("sebeolsik-final", "세벌식 최종"),
        entry("sebeolsik-noshift", "세벌식 순아래"),
        entry("sebeolsik-yet", "세벌식 옛글"),
        entry("sin3-p2-basic", "신세벌식 P2"),
        entry("sin3-p2-yet", "신세벌식 P2 옛한글 조합 + 기호 확장"),
        entry("sin3-p2", "신세벌식 P2 + 기호 확장"),
        entry("us-qwerty", "영어 모드 (qwerty 자판)", LayoutMode.LATIN),
    )

    @Test
    fun `hangul list is in hangul order`() {
        // Measured, not reasoned: this list is what the sort produced from the
        // real names, and the two places a reader's instinct is wrong are
        // pinned by it.
        //   로마자 comes AFTER 두벌식. ㄹ follows ㄷ in the 14-consonant order,
        //   and 로 (U+B85C) is above 두 (U+B450) — which is exactly why
        //   code-point order gives 가나다순 for Hangul at all.
        //   세벌식 390 comes before 세벌식 두벌자판: after the shared prefix,
        //   '3' (U+0033) is below 두 (U+B450). Latin sorts before Hangul.
        val ids = LayoutCatalog.byMode(scanned, LayoutMode.HANGUL).map { it.id }
        assertEquals(
            listOf(
                "dubeolsik",         // 두
                "dubeolsik-yet",     // 두벌식 옛글
                "romaja",            // 로
                "sebeolsik-3-90",    // 세벌식 3…
                "sebeolsik-3-2",     // 세벌식 두…
                "sebeolsik-noshift", // 순
                "sebeolsik-yet",     // 옛
                "sebeolsik-final",   // 최
                "sin3-p2-basic",
                "sin3-p2",
                "sin3-p2-yet",
                "ahnmatmae",         // 안
            ),
            ids,
        )
    }

    @Test
    fun `latin list holds only latin layouts`() {
        val latin = LayoutCatalog.byMode(scanned, LayoutMode.LATIN)
        assertEquals(listOf("us-qwerty"), latin.map { it.id })
    }

    @Test
    fun `the order does not depend on what the engine scanned first`() {
        // Reversed input, same output: the sort is the only thing deciding.
        assertEquals(
            LayoutCatalog.byMode(scanned, LayoutMode.HANGUL),
            LayoutCatalog.byMode(scanned.reversed(), LayoutMode.HANGUL),
        )
    }

    @Test
    fun `equal labels are ordered by id so the list is a total order`() {
        val twin = listOf(entry("b", "같은 이름"), entry("a", "같은 이름"))
        assertEquals(listOf("a", "b"), LayoutCatalog.sorted(twin).map { it.id })
    }

    @Test
    fun `a name with latin in it sorts by the hangul it starts with`() {
        // "신세벌식 P2 + 기호 확장" before "영어 모드 (qwerty 자판)": 신 (U+C2DC)
        // < 영 (U+C601). This is why the rule is code-point order and not a
        // Collator — see the file header.
        val mixed = listOf(
            entry("b", "영어 모드 (qwerty 자판)"),
            entry("a", "신세벌식 P2 + 기호 확장"),
        )
        assertEquals(listOf("a", "b"), LayoutCatalog.sorted(mixed).map { it.id })
    }

    @Test
    fun `an unknown mode string is hangul, like the engine`() {
        assertEquals(LayoutMode.HANGUL, LayoutMode.of(""))
        assertEquals(LayoutMode.HANGUL, LayoutMode.of("hangul"))
        assertEquals(LayoutMode.HANGUL, LayoutMode.of("HANGUL"))
        assertEquals(LayoutMode.HANGUL, LayoutMode.of("hanguel"))
        assertEquals(LayoutMode.LATIN, LayoutMode.of("latin"))
    }

    @Test
    fun `choose keeps the active layout when it is in the list`() {
        assertEquals(
            "sebeolsik-3-2",
            LayoutCatalog.choose(scanned, LayoutMode.HANGUL, "sebeolsik-3-2", "sin3-p2"),
        )
    }

    @Test
    fun `choose falls back when the stored layout is not offered`() {
        // A layout removed from the data, or one from the other half of the IME:
        // sin3-p2 must not answer a question about the Latin list.
        assertEquals(
            "us-qwerty",
            LayoutCatalog.choose(scanned, LayoutMode.LATIN, "sin3-p2", "us-qwerty"),
        )
        // The documented default, not the first row after sorting: a stored
        // layout that no longer exists must land where the user is documented to
        // get sin3-p2, not on whichever name happens to sort first.
        assertEquals(
            "sin3-p2",
            LayoutCatalog.choose(scanned, LayoutMode.HANGUL, "gone-from-the-data", "sin3-p2"),
        )
    }

    @Test
    fun `choose returns something when there is nothing to choose from`() {
        assertEquals("fallback", LayoutCatalog.choose(emptyList(), LayoutMode.LATIN, null, "fallback"))
        assertTrue(LayoutCatalog.byMode(emptyList(), LayoutMode.HANGUL).isEmpty())
    }
}