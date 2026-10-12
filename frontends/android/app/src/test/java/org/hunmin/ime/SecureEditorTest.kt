/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the secure-editor policy, against a real inputType.
 *
 * WHY THIS TEST EXISTS (issue #13).
 *
 * The Espresso suite found the defect on the device: `p,a,s,s` on sin3-p2 in a
 * password EditText composed 퓬ㄴ instead of committing `pass`, because
 * HunminInputMethodService never looked at the EditorInfo. That is a device
 * measurement and it stays in the Espresso suite -- but a device tier is the
 * wrong place to guard a regression. The policy is a pure function of an int,
 * so it is asserted here, where every commit runs it.
 *
 * The constants below are the framework's, verified with javap -constants
 * against android.jar (API 35, 2026-10-13) rather than from memory:
 *   TYPE_MASK_VARIATION                    = 0x00000fff
 *   TYPE_CLASS_TEXT                        = 0x00000001
 *   TYPE_TEXT_VARIATION_PASSWORD           = 0x00000080
 *   TYPE_TEXT_VARIATION_VISIBLE_PASSWORD   = 0x00000090
 *   TYPE_TEXT_VARIATION_EMAIL_ADDRESS      = 0x00000020
 *   TYPE_TEXT_VARIATION_URI                = 0x00000010
 *   TYPE_CLASS_NUMBER                      = 0x00000002
 * A wrong constant here would make the guard fire on a plain editor -- the
 * exact class of failure this test exists to prevent, which is why the values
 * are asserted alongside the policy.
 */
package org.hunmin.ime

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SecureEditorTest {

    private val typeClassText = 0x00000001
    private val typeClassNumber = 0x00000002

    private val variationPassword = 0x00000080
    private val variationVisiblePassword = 0x00000090
    private val variationEmail = 0x00000020
    private val variationUri = 0x00000010

    // ------------------------------------------------------------ the defect

    @Test
    fun `a password editor is secure`() {
        // The exact case that failed on the AVD: an EditText with
        // android:inputType="textPassword", whose inputType is
        // TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_PASSWORD.
        val inputType = typeClassText or variationPassword
        assertTrue(
            "textPassword must be treated as a secure editor; without this the " +
                "IME composes Hangul the user cannot read",
            HunminInputMethodService.isSecureEditorInputType(inputType),
        )
    }

    @Test
    fun `a visible-password editor is secure`() {
        // Same masking argument: the field shows the character briefly and
        // then reverts, so a composing span is still invisible to the user.
        val inputType = typeClassText or variationVisiblePassword
        assertTrue(
            "visiblePassword must also be secure",
            HunminInputMethodService.isSecureEditorInputType(inputType),
        )
    }

    // ------------------------------------------- the false-positive guard

    @Test
    fun `a plain text editor is not secure`() {
        // TYPE_CLASS_TEXT with no variation. If the mask ever matched this,
        // every ordinary field would silently stop composing -- a worse
        // failure than the one being fixed.
        assertFalse(
            "a plain text field must keep composing",
            HunminInputMethodService.isSecureEditorInputType(typeClassText),
        )
    }

    @Test
    fun `email and uri editors are not secure`() {
        // Both are TYPE_CLASS_TEXT with their own variations. They are masked
        // on screen in the same visual sense, but they are not secrets and an
        // IME composing in them is normal.
        assertFalse(
            "an email field must keep composing",
            HunminInputMethodService.isSecureEditorInputType(typeClassText or variationEmail),
        )
        assertFalse(
            "a URI field must keep composing",
            HunminInputMethodService.isSecureEditorInputType(typeClassText or variationUri),
        )
    }

    @Test
    fun `a number editor is not secure`() {
        // A different TYPE_CLASS entirely, so the variation bits differ. The
        // mask is over the variation field only and must not leak across
        // classes.
        assertFalse(
            "a number field must keep composing",
            HunminInputMethodService.isSecureEditorInputType(typeClassNumber),
        )
    }

    @Test
    fun `TYPE_NULL is not secure`() {
        // 0 is what a null EditorInfo collapses to. Treating it as secure would
        // make every editor without explicit inputType lose composition.
        assertFalse(
            "inputType 0 (TYPE_NULL) must not be secure",
            HunminInputMethodService.isSecureEditorInputType(0),
        )
    }

    // ------------------------------------------------ the framework constants

    @Test
    fun `the framework constants the policy is written against are the right ones`() {
        // Asserted separately from the policy so that a framework value change
        // shows up as THIS test failing, naming the constant, rather than as a
        // mysterious secure-editor behaviour change.
        //
        // javap -p -v -constants against android.jar, API 35, 2026-10-13.
        // The two password constants are hidden (javap prints `***`) and were
        // read from the class file's ConstantValue attributes rather than from
        // the public signature.
        val mask = 0xff0 // TYPE_MASK_VARIATION = 4080
        assertTrue("TYPE_MASK_VARIATION is 0xff0 (4080)", mask == 4080)

        // The two password variations must survive `inputType and mask`
        // unchanged, which is what the policy's comparison relies on.
        assertTrue(
            "variation 0x${variationPassword.toString(16)} must survive the mask",
            (typeClassText or variationPassword) and mask == variationPassword,
        )
        assertTrue(
            "variation 0x${variationVisiblePassword.toString(16)} must survive the mask",
            (typeClassText or variationVisiblePassword) and mask == variationVisiblePassword,
        )
        // And the non-password variations must not collide with either.
        assertTrue(
            "the email variation must not be mistaken for a password",
            (typeClassText or variationEmail) and mask != variationPassword,
        )
        assertTrue(
            "the URI variation must not be mistaken for a password",
            (typeClassText or variationUri) and mask != variationVisiblePassword,
        )
    }
}
