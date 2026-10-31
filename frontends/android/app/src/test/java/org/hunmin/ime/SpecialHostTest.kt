/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the special-function host, through the REAL JNI shim and the real
 * engine -- no emulator, no stub.
 *
 * What this pins, and each one is a way the ruling can silently not work:
 *
 *  - the ENGINE owns the chord table: with no host registered, the defaults
 *    (VOICE = Alt) are still BOUND, so the answer is the refusal (-1) and not
 *    "not a chord" (0). Those two are different keys on the wire: 0 means the
 *    single press is a toggle, -1 means the key must pass through.
 *  - the host is asked BEFORE anything happens, and a refusal performs nothing.
 *  - libhunmin calls back into Kotlin at all: the callbacks are upcalls through
 *    hunmin-jni.c, found by method name, so a rename makes the shim register no
 *    host and every chord answer -1 -- which is why the performing case is
 *    asserted through the shim rather than on a Kotlin fake.
 *  - peek performs nothing (the platform test phase must not have side effects).
 */
package org.hunmin.ime

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class SpecialHostTest {

    /* hunmin.h HUNMIN_MOD_* -- contract values. */
    private val modCtrl = 0x1
    private val modAlt = 0x2
    private val modGui = 0x4

    private var ctx: Long = 0
    private var layer: Long = 0

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }

    /** Records what the engine asked and performed. */
    private class Recorder(private val canDo: Set<Int> = setOf(SpecialFunction.VOICE)) : SpecialServices {
        val asked = mutableListOf<Int>()
        val performed = mutableListOf<Int>()
        override fun supports(fn: Int): Boolean {
            asked += fn
            return fn in canDo
        }
        override fun perform(fn: Int) {
            performed += fn
        }
    }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh, which builds and " +
                "points at the host JNI shim (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        assertTrue("hunmin_ctx_new found no layout", ctx != 0L)
        layer = HunminNativeBridge.nativeLayerNew(ctx, "sin3-p2", "qwerty", "word")
        assertTrue("the layer could not be created", layer != 0L)
    }

    @After
    fun tearDown() {
        if (layer != 0L) HunminNativeBridge.nativeLayerSpecialHost(layer, null)
        if (layer != 0L) HunminNativeBridge.nativeLayerFree(layer)
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
    }

    private fun register(services: SpecialServices) =
        HunminNativeBridge.nativeLayerSpecialHost(layer, HunminSpecialHost(services))

    @Test
    fun `with no host a bound chord is refused, not treated as a single press`() {
        // The engine's default table (VOICE = Alt, AUTOSWITCH = Ctrl) is in force
        // with nothing registered. -1 and 0 are different keys: -1 says "this IS a
        // compound press, but nothing here can service it", 0 says "not a chord,
        // the single press is a toggle".
        assertEquals(-1, HunminNativeBridge.nativeLayerSpecialPeek(layer, 0, 0, modAlt))
        assertEquals(-1, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt))
    }

    @Test
    fun `the single press is not a special function`() {
        register(Recorder())
        assertEquals("no modifier: the toggle, not a chord", 0, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, 0))
    }

    @Test
    fun `an unbound chord is not a special function`() {
        register(Recorder())
        assertEquals("GUI is not in the default table", 0, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modGui))
    }

    @Test
    fun `the engine asks the host and the host's yes performs once`() {
        val rec = Recorder()
        register(rec)
        val fn = HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt)
        assertEquals(SpecialFunction.VOICE, fn)
        assertEquals(listOf(SpecialFunction.VOICE), rec.performed)
        assertEquals("asked before performing", listOf(SpecialFunction.VOICE), rec.asked)
    }

    @Test
    fun `a host that refuses performs nothing and leaves the key unconsumed`() {
        val rec = Recorder(canDo = emptySet())
        register(rec)
        assertEquals(-1, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt))
        assertTrue("a refused chord must not be performed", rec.performed.isEmpty())
    }

    @Test
    fun `peek answers without performing or asking the host`() {
        val rec = Recorder()
        register(rec)
        assertEquals(SpecialFunction.VOICE, HunminNativeBridge.nativeLayerSpecialPeek(layer, 0, 0, modAlt))
        assertTrue("peek is the test phase: no side effects", rec.performed.isEmpty())
    }

    @Test
    fun `binding moves a function's chord, and the table reads back what the dialog shows`() {
        val rec = Recorder()
        register(rec)
        assertEquals(modAlt, HunminNativeBridge.nativeLayerSpecialChord(layer, SpecialFunction.VOICE))

        assertTrue(HunminNativeBridge.nativeLayerSpecialBind(layer, SpecialFunction.VOICE, modCtrl))
        assertEquals(modCtrl, HunminNativeBridge.nativeLayerSpecialChord(layer, SpecialFunction.VOICE))
        assertEquals("Alt is no longer voice", 0, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt))
        assertEquals(SpecialFunction.VOICE, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modCtrl))
    }

    @Test
    fun `unregistering the host turns every chord back into a refusal`() {
        val rec = Recorder()
        register(rec)
        assertEquals(SpecialFunction.VOICE, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt))

        HunminNativeBridge.nativeLayerSpecialHost(layer, null)
        assertEquals(-1, HunminNativeBridge.nativeLayerSpecial(layer, 0, 0, modAlt))
        assertFalse("nothing may be performed after unregistering", rec.performed.size > 1)
    }

    @Test
    fun `shift is a special modifier only on the language key`() {
        // Ruled 2026-10-31: Shift must stay a capitalisation row for characters
        // and a special combination with 한/영. The engine admits SHIFT only for an
        // event flagged as the language key, which these entry points always set;
        // an unbound Shift is then "not a chord", never a dead key.
        val rec = Recorder(canDo = setOf(SpecialFunction.VOICE))
        register(rec)
        assertEquals(0, HunminNativeBridge.nativeLayerSpecial(layer, 0, 1, 0))

        assertTrue(HunminNativeBridge.nativeLayerSpecialBind(layer, SpecialFunction.VOICE, 0x8))
        assertEquals(0x8, HunminNativeBridge.nativeLayerSpecialChord(layer, SpecialFunction.VOICE))
        assertEquals(SpecialFunction.VOICE, HunminNativeBridge.nativeLayerSpecial(layer, 0, 1, 0))
    }
}
