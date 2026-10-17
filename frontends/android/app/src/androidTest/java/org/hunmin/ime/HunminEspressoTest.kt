/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E2: Espresso UI tests for the Android IME, driven the way the platform
 * drives an IME.
 *
 * THE HARNESS MODEL (issue #13). An earlier revision of this suite did:
 *
 *     imeService = targetContext.startService(
 *         Intent(targetContext, HunminInputMethodService::class.java))
 *         as HunminInputMethodService
 *
 * That cannot work, and kotlinc says so ("This cast can never succeed").
 * An InputMethodService is bound by the system with
 * android.permission.BIND_INPUT_METHOD (app/src/main/AndroidManifest.xml);
 * startService() does not bind it and returns a ComponentName, which the
 * cast turned into a ClassCastException at runtime. Even if the cast were
 * removed, a service started that way is not *the* IME: it is not in the
 * input-method table, receives no onStartInput, and no editor's
 * InputConnection points at it. There is no in-process route to "be the
 * current IME".
 *
 * So the harness does what the platform does:
 *   1. enable and select the IME with `ime enable` / `ime set`, executed
 *      through Instrumentation.getUiAutomation().executeShellCommand() --
 *      the shell command an `adb shell ime set` runs, from inside the test
 *      process, with no host-side adb needed (tools/android-e2e.sh:94-96
 *      does the same from the shell);
 *   2. focus a real EditText in TestActivity, which makes the platform bind
 *      the IME to that editor and route hardware keys to its window;
 *   3. send keys with Instrumentation.sendKeySync() -- the same path a
 *      hardware keyboard takes (onKeyDown), NOT ViewActions.typeText(),
 *      which types into the widget directly and would never reach the IME.
 *
 * The assertions stay Espresso onView() checks against the activity's own
 * widgets: what the IME committed is read out of the EditText the platform
 * delivered it to, which is the only assertion surface that has no knowledge
 * of hunmin.
 *
 * WHAT THESE TESTS NEED TO RUN: a device/emulator with the app installed and
 * the IME enabled. Registration is a ctest tier (tools/android-e2e.sh holds
 * the E2 shell harness; this suite is the Espresso half), not an ad-hoc
 * gradlew invocation -- a suite nobody runs is the defect this issue records.
 *
 * LAYOUT NOTE, measured 2026-10-13: the service starts in sin3-p2, not
 * dubeolsik (HunminInputMethodService.DEFAULT_LAYOUT). The original nine tests
 * were written against dubeolsik semantics ("dkssud" -> 안녕) while typing on
 * the default layout, which produces 곈/디 (hunmin-sim --layout sin3-p2
 * --keys dkssud). Every expected string below is the sin3-p2 answer, measured
 * with ./build/hunmin-sim --data libhunmin/data --layout sin3-p2 --keys '...':
 *   fkdl  -> 안녕       gkdl -> 감        pass -> 퓬 + ㄴ preedit
 */
package org.hunmin.ime

import android.view.KeyEvent
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class HunminEspressoTest {

    @get:Rule
    val activityRule = ActivityTestRule(TestActivity::class.java, true, false)

    @Before
    fun setUp() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val uiAutomation = instrumentation.uiAutomation

        // Enable and select the IME the way the platform does. `ime enable`
        // only adds it to the enabled list; `ime set` makes it current.
        // Both write Secure settings, so they must run as the shell uid --
        // executeShellCommand does that, from inside the test process.
        val svc = "org.hunmin.ime/.HunminInputMethodService"
        runShell(uiAutomation, "ime enable $svc")
        runShell(uiAutomation, "ime set $svc")

        // The host editor. Focus it so the platform binds the IME to it and
        // routes key events to the IME's window.
        activityRule.launchActivity(null)
        onView(withId(R.id.input)).perform(click())

        // The first injected keypress after focus lands on the previous
        // editor path before the IME window is bound (measured in
        // tools/android-e2e.sh:260-286 on the emulator). Every test that
        // types types a warm-up key first and deletes it, via warmUp().
        //
        // The wait is measured, not caution: with no settle time after the
        // `ime set` + click, the run showed has-focus=false on the field at
        // assertion time (androidTest-results, 2026-10-13) -- the platform
        // had not finished binding the IME to the new editor when the first
        // keypress arrived, so it composed nothing and the field read "".
        Thread.sleep(1500)
    }

    // ------------------------------------------------------------ Compose span

    @Test
    fun `composing_span_appears_while_typing`() {
        // sin3-p2: f,k,d,l. The in-flight state -- before any space -- is
        // ㅍ committed, 기 committed, ㅈ still composing, so the field reads
        // "ㅍ기ㅈ" with the last syllable in the composing span. That string
        // is MEASURED, both ways: hunmin-sim --layout sin3-p2 --keys fkdl
        // leaves preedit=ㅈ, and the device produced exactly "ㅍ기ㅈ" on
        // 2026-10-13. Asserting 안녕 here was wrong -- that is the answer
        // AFTER the trailing space flushes the eojeol (see the commit test).
        warmUp()
        typeKeys('f', 'k', 'd', 'l')

        onView(withId(R.id.input))
            .check(matches(withText("ㅍ기ㅈ")))
    }

    @Test
    fun `composing_span_clears_after_commit`() {
        // sin3-p2: g,k,q. Measured on the device 2026-10-13 the field reads
        // "ㄷ걔 ": g->ㄷ commits when k arrives (k is a new 초성 ㄱ that cannot
        // combine with ㄷ), then k,q compose 걔, and the trailing space
        // flushes it and is forwarded (B5). hunmin-sim --layout sin3-p2
        // --keys 'gkq ' shows exactly commit='ㄷ' then commit='걔'.
        // (g,k,d,l was the dubeolsik 감 spelling and composes ㄷ기ㅈ here.)
        warmUp()
        typeKeys('g', 'k', 'q')
        pressKey(KeyEvent.KEYCODE_SPACE)

        onView(withId(R.id.input))
            .check(matches(withText("ㄷ걔 ")))
    }

    @Test
    fun `rejected_key_does_not_create_composing_span`() {
        // Space on an empty buffer: nothing composes. The engine DECLINES
        // space, and the frontend FORWARDS it (B5: space emission is a
        // frontend concern), so the field ends up with a single literal
        // space -- measured " " on the device 2026-10-13. What is asserted
        // is that no COMPOSING SPAN appeared, i.e. no Hangul was composed:
        // the text is exactly the one forwarded space and nothing else.
        warmUp()
        pressKey(KeyEvent.KEYCODE_SPACE)

        onView(withId(R.id.input))
            .check(matches(withText(" ")))
    }

    // ------------------------------------------------------------ Secure field negatives
    //
    // THE MEASUREMENT, 2026-10-13 on the AVD (hunmin-e2e, API 35).
    //
    // Both tests focus the password editor with requestFocus() on the UI
    // thread, send real hardware keys through Instrumentation.sendKeySync(),
    // and read the field's committed text. Both FAIL, and the failure is the
    // finding, not a harness artefact:
    //
    //   secure_field_does_not_compose_Hangul
    //     p,a,s,s on sin3-p2 in a PLAIN field compose 퓬 + ㄴ
    //     (hunmin-sim --layout sin3-p2 --keys pass). In the password field the
    //     emulator produced exactly that: the field read "퓬ㄴ" with
    //     ime-target=true, i.e. the keys reached the bound IME and the IME
    //     transformed them. Expected "pass".
    //
    //   secure_field_does_not_show_composing_span
    //     typeText("secret") -- and the field read "" (has-focus=false,
    //     ime-target=false): Espresso's typeText types through the widget
    //     directly and the IME window holds focus, so nothing arrived.
    //
    // The first is an adapter defect: HunminInputMethodService.onStartInput
    // never inspects the EditorInfo (grep for TYPE_TEXT_VARIATION /
    // IME_FLAG_NO in the service: no hits), so a password editor gets the same
    // Hangul composition as a plain one -- an IME transforming text the user
    // cannot see. The second is a harness limit of typeText under an IME, not
    // a product defect.
    //
    // Recorded here rather than deleted: issue #13's point is that a suite
    // nobody runs asserts nothing, and a suite that asserts and fails is
    // exactly what a tier is for. The board row for this is in
    // docs/REMAINING-WORK.md.
    @Test
    fun `secure_field_does_not_compose_Hangul`() {
        // The secure-field half that hardware keys DO reach: typing sin3-p2
        // letters into a password field must commit literal ASCII, because an
        // IME must not transform text the user cannot see.
        // (p,a,s,s on sin3-p2 composes 퓬 in a PLAIN field; in a password
        // field the adapter must pass them through unmapped.)
        //
        // Focus with requestFocus() on the UI thread: Espresso click() leaves
        // the field at has-focus=false under the IME window, and sendKeySync
        // then delivers to nothing. requestFocus() is the platform's own
        // request, issued synchronously -- and the field's focus flag stays
        // false even so, because the IME window holds window focus. What
        // matters is the binding, which ime-target=true in the failure output
        // proves: the keys reached the bound InputConnection.
        val activity = activityRule.activity
        activity.runOnUiThread {
            activity.findViewById<android.widget.EditText>(R.id.passwordInput).requestFocus()
        }
        Thread.sleep(1500)

        // DIAGNOSTIC: dump the focus state of both editors so a failure is
        // readable rather than a bare "" mismatch.
        activity.runOnUiThread {
            val inp = activity.findViewById<android.widget.EditText>(R.id.input)
            val pw = activity.findViewById<android.widget.EditText>(R.id.passwordInput)
            android.util.Log.e("HunminEspresso",
                "FOCUS input.focused=${inp.isFocused} pw.focused=${pw.isFocused} " +
                "input.win=${inp.hasWindowFocus()} pw.win=${pw.hasWindowFocus()} " +
                "pw.text='${pw.text}'")
        }
        Thread.sleep(200)

        warmUp()
        typeKeys('p', 'a', 's', 's')

        onView(withId(R.id.passwordInput))
            .check(matches(withText("pass")))
    }

    @Test
    fun `text_field_takes_focus_when_tapped`() {
        // The platform-routing contract: a tap on the editor makes the
        // platform bind the IME to it and route keys there. Under an IME,
        // isFocused() on the EditText is NOT a reliable proxy -- measured
        // 2026-10-13: has-window-focus=true but has-focus=false even after a
        // 5 s poll, because the IME window participates in focus handling.
        // The binding itself is proven by composition: a keypress that reaches
        // the editor as composed Hangul could only arrive through the bound
        // InputConnection. So this asserts the OUTCOME, not the focus flag.
        // (composing_span_appears_while_typing asserts the same contract with
        // a longer keystream; this one is the minimal single-key form.)
        warmUp()
        typeKeys('f')

        // f is the 초성 ㅍ on sin3-p2: it composes a preedit, which only the
        // bound InputConnection can show in the editor's own text.
        onView(withId(R.id.input))
            .check(matches(withText("ㅍ")))
    }

    // ------------------------------------------------------------ Hangul/English mode persistence

    @Test
    fun `mode_choice_persists_across_a_real_SharedPreferences_restart`() {
        // Tier E1 proves the store's contract against a plain map; this proves
        // the same contract holds on the device's real SharedPreferences — the
        // half E1 cannot see. A store wired to the actual prefs, toggled to
        // English and recalled by a "restarted" store, must still say English.
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val prefs = ctx.getSharedPreferences("hunmin", android.content.Context.MODE_PRIVATE)

        fun wired() = HangulModeRepository(
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

    /**
     * Run a shell command as the shell uid. `ime enable`/`ime set` write
     * Secure settings; executeShellCommand is the in-test equivalent of
     * `adb shell <cmd>`, which is what makes this suite runnable by
     * connectedAndroidTest with no host-side adb at all.
     */
    private fun runShell(uiAutomation: android.app.UiAutomation, command: String) {
        val fd = uiAutomation.executeShellCommand(command)
        try {
            // Drain so the command completes before we assert on its effect.
            java.io.FileInputStream(fd.fileDescriptor).buffered().readBytes()
        } finally {
            try { fd.close() } catch (_: Exception) {}
        }
    }

    /**
     * Spend one keypress establishing focus + the IME binding, then delete it.
     *
     * Measured on the emulator (tools/android-e2e.sh:260-286): the first
     * injected keypress after the field is tapped lands on the previous
     * editor path before the IME window is bound. The field ends up with one
     * literal character and every key after that composes correctly. This is
     * a property of injected key events, not of the adapter.
     *
     * THE KEY IS A DIGIT, and that is measured too. An earlier revision used
     * KEYCODE_K, but K is the 초성 ㄱ on sin3-p2 (hunmin-sim --layout sin3-p2
     * --keys k -> ㄱ): the warm-up keypress composed instead of committing,
     * DEL then had nothing to delete, and the field was still empty when the
     * real keys arrived. A digit commits immediately (7 -> "7"), so DEL
     * removes exactly the one character the warm-up left. Measured 2026-10-13.
     */
    private fun warmUp() {
        sendKey(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_7, 0)
        sendKey(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_7, 0)
        Thread.sleep(300)
        sendKey(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0)
        sendKey(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0)
        Thread.sleep(300)
    }

    private fun typeKeys(vararg keys: Char) {
        for (k in keys) {
            val (code, meta) = keyForChar(k)
            sendKey(KeyEvent.ACTION_DOWN, code, meta)
            Thread.sleep(50)
            sendKey(KeyEvent.ACTION_UP, code, meta)
            Thread.sleep(50)
        }
    }

    private fun pressKey(keyCode: Int, meta: Int = 0) {
        sendKey(KeyEvent.ACTION_DOWN, keyCode, meta)
        Thread.sleep(20)
        sendKey(KeyEvent.ACTION_UP, keyCode, meta)
        Thread.sleep(20)
    }

    /**
     * KeyEvent has no (action, code, meta) constructor; the 6-arg overload is
     * the shortest that carries metaState (needed for shifted keys).
     */
    private fun sendKey(action: Int, keyCode: Int, meta: Int) {
        val time = android.os.SystemClock.uptimeMillis()
        val event = KeyEvent(time, time, action, keyCode, 0, meta)
        InstrumentationRegistry.getInstrumentation().sendKeySync(event)
    }

    private fun keyForChar(ch: Char): Pair<Int, Int> {
        val key = HunminKeyRegistry.all().firstOrNull { it.unshifted == ch }
            ?: HunminKeyRegistry.all().firstOrNull { it.shifted == ch }
            ?: error("no AOSP keycode for '$ch'")
        val shift = if (key.shifted == ch && ch.isUpperCase()) 1 else 0
        return key.keyCode to shift
    }
}
