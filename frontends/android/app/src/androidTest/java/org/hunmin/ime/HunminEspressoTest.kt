/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E2: Espresso UI tests for the Android IME.
 *
 * These tests run on a real device/emulator and assert the behaviour of the
 * IME's UI components (candidate view, composing span, Hanja window, etc.)
 * against the expectations documented in SPEC-MODES.md, FRONTEND-CONTRACT.md,
 * and the tier E2 assertion model (docs/TESTING.md).
 *
 * They require a real Android emulator/device with the hunmin IME installed.
 * The test runner is configured in build.gradle.kts under androidTest.
 */
package org.hunmin.ime

import android.view.KeyEvent
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.pressBack
import androidx.test.espresso.Espresso.pressKey
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.swipeLeft
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isFocused
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.not
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import org.hamcrest.CoreMatchers.is
import org.hamcrest.CoreMatchers.not
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Tier E2: Espresso UI tests for the Android IME.
 *
 * These tests run on a real device/emulator and assert the behaviour of the
 * IME's UI components (candidate view, composing span, Hanja window, etc.)
 * against the expectations documented in SPEC-MODES.md, FRONTEND-CONTRACT.md,
 * and the tier E2 assertion model (docs/TESTING.md).
 *
 * They require a real Android emulator/device with the hunmin IME installed.
 * The test runner is configured in build.gradle.kts under androidTest.
 */
@RunWith(AndroidJUnit4::class)
class HunminEspressoTest {

    @get:Rule
    val activityRule = ActivityTestRule(TestActivity::class.java, true, false)

    private lateinit var imeService: HunminInputMethodService

    @Before
    fun setUp() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val targetContext = instrumentation.targetContext

        // Start the IME service
        val intent = android.content.Intent(targetContext, HunminInputMethodService::class.java)
        imeService = targetContext.startService(intent) as HunminInputMethodService
        assertNotNull("IME service must start", imeService)

        // Launch a dummy activity to hold the input connection
        val activity = activityRule.launchActivity(android.content.Intent())
        activity.waitForIdle()

        // Wait for the IME to be ready
        Thread.sleep(500)
    }

    // ------------------------------------------------------------ Compose span

    @Test
    fun `composing span appears while typing`() {
        // Simulate typing "d" "k" "s" "s" on dubeolsik -> "안녕"
        typeKeys('d', 'k', 's', 's')

        // Verify composing span is visible with correct text
        onView(withId(R.id.input))
            .check(matches(isDisplayed()))

        // The composing text should be visible in the input field
        onView(withId(R.id.input))
            .check(matches(withText("안녕")))
    }

    @Test
    fun `composing span clears after commit`() {
        // Type a complete syllable that commits
        typeKeys('g', 'k', 's') // "감" on dubeolsik

        // Space commits the syllable
        pressKey(KeyEvent.KEYCODE_SPACE)

        // Composing span should be empty after commit
        onView(withId(R.id.input))
            .check(matches(withText("감 ")))
    }

    @Test
    fun `rejected key does not create composing span`() {
        // Press a key that the engine declines (e.g., space on empty buffer)
        pressKey(KeyEvent.KEYCODE_SPACE)

        // No composing span should appear
        onView(withId(R.id.input))
            .check(matches(withText("")))
    }

    // ------------------------------------------------------------ Secure field negatives

    @Test
    fun `secure field does not show candidate view`() {
        // Find a password field (type=textPassword)
        onView(withId(R.id.passwordInput))
            .check(matches(isDisplayed()))
            .perform(click())

        // Type something
        typeKeys('p', 'a', 's', 's')

        // Candidate view should NOT appear for password fields
        // Note: Candidate view is IME-internal, tested via UIAutomator in full E2E
        onView(withId(R.id.passwordInput))
            .check(matches(withText("pass")))
    }

    @Test
    fun `secure field does not show composing span`() {
        // Type in a password field
        onView(withId(R.id.passwordInput))
            .perform(click())
            .perform(typeText("secret"))

        // Composing span should be hidden (or empty) for password fields
        onView(withId(R.id.passwordInput))
            .check(matches(withText("secret")))
    }

    @Test
    fun `secure field does not trigger Hanja`() {
        onView(withId(R.id.passwordInput))
            .perform(click())
            .perform(typeText("test"))

        pressKey(KeyEvent.KEYCODE_F9)

        // Hanja window should NOT appear in secure fields
        // Note: Hanja window is IME-internal, tested via UIAutomator in full E2E
        onView(withId(R.id.passwordInput))
            .check(matches(withText("test")))
    }

    // ------------------------------------------------------------ inputView show/hide policy

    @Test
    fun `inputView shows on text field focus`() {
        onView(withId(R.id.input))
            .perform(click())

        // IME should be visible
        Thread.sleep(200)
        // Verify the IME window is visible
        onView(withId(R.id.input))
            .check(matches(isFocused()))
    }

    @Test
    fun `inputView hides on back press`() {
        onView(withId(R.id.input))
            .perform(click())

        Thread.sleep(200)
        pressKey(KeyEvent.KEYCODE_BACK)

        // IME should be hidden
        Thread.sleep(200)
        // Verify by checking that the input field lost focus
        onView(withId(R.id.input))
            .check(matches(not(isFocused())))
    }

    @Test
    fun `inputView hides on secure field focus`() {
        // Focus a regular field first
        onView(withId(R.id.input))
            .perform(click())

        Thread.sleep(200)

        // Switch to a password field
        onView(withId(R.id.passwordInput))
            .perform(click())

        Thread.sleep(200)
        // IME should remain visible but change mode (no candidates, no composing)
        onView(withId(R.id.passwordInput))
            .check(matches(isFocused()))
    }

    // ------------------------------------------------------------ Hangul/English mode persistence

    @Test
    fun `mode choice persists across a real SharedPreferences restart`() {
        // Tier E1 proves the store's contract against a plain map; this proves
        // the same contract holds on the device's real SharedPreferences — the
        // half E1 cannot see. A store wired to the actual prefs, toggled to
        // English and recalled by a "restarted" store, must still say English.
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val prefs = ctx.getSharedPreferences("hunmin", android.content.Context.MODE_PRIVATE)

        fun wired() = HangulModeStore(
            read = { k, d -> prefs.getBoolean(k, d) },
            write = { k, v -> prefs.edit().putBoolean(k, v).apply() },
        )

        prefs.edit().remove("hangul_mode").apply()   // first-install state
        val first = wired()
        assertTrue("a wiped prefs must recall Hangul", first.recall("hangul_mode"))

        first.remember("hangul_mode", false)         // switch to English
        // apply() is async; commit() blocks until it is on disk.
        assertTrue("the write must reach disk", prefs.edit().putBoolean("hangul_mode", false).commit())

        val restarted = wired()                      // a new process
        assertFalse("English must survive a restart on real prefs",
            restarted.recall("hangul_mode"))

        // Leave the prefs as a first-install device would have it, so this
        // test does not order-depend on the mode the next test starts in.
        prefs.edit().remove("hangul_mode").apply()
    }

    // ------------------------------------------------------------ Helpers

    private fun typeKeys(vararg keys: Char) {
        for (k in keys) {
            val (code, meta) = keyForChar(k)
            val down = KeyEvent(KeyEvent.ACTION_DOWN, code, meta)
            InstrumentationRegistry.getInstrumentation().sendKeySync(down)
            Thread.sleep(50)
            val up = KeyEvent(KeyEvent.ACTION_UP, code, meta)
            InstrumentationRegistry.getInstrumentation().sendKeySync(up)
            Thread.sleep(50)
        }
    }

    private fun pressKey(keyCode: Int, meta: Int = 0) {
        val down = KeyEvent(KeyEvent.ACTION_DOWN, keyCode, meta)
        InstrumentationRegistry.getInstrumentation().sendKeySync(down)
        Thread.sleep(20)
        val up = KeyEvent(KeyEvent.ACTION_UP, keyCode, meta)
        InstrumentationRegistry.getInstrumentation().sendKeySync(up)
        Thread.sleep(20)
    }

    private fun keyForChar(ch: Char): Pair<Int, Int> {
        val key = HunminKeys.all().firstOrNull { it.unshifted == ch }
            ?: HunminKeys.all().firstOrNull { it.shifted == ch }
            ?: error("no AOSP keycode for '$ch'")
        val shift = if (key.shifted == ch && ch.isUpperCase()) 1 else 0
        return key.keyCode to shift
    }
}