/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the system settings screen's state source (issue #177).
 *
 * SettingsActivity cannot be constructed on a plain JVM, so the half that can
 * be wrong in a way a device would not show is tested here: AutoSwitchSettings
 * builds a real layer, hands it the stored read-back, and reports what the
 * ENGINE said. The screen only draws that answer.
 *
 * What this pins, and the failure each line rules out:
 *  - the value is the engine's, not the stored request (a refused ON read back
 *    as ON would make the switch claim a feature the judgement cannot use);
 *  - the key is the engine's chord, so the screen cannot advertise a different
 *    key from the one OverlayActivity and the key hook use (R13-정정);
 *  - B3's data gate is asked of the engine, and absent data disables the row
 *    rather than showing a switch that does nothing.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File
import java.nio.file.Files

class AutoSwitchSettingsTest {

    private var stored = false
    private var writes = 0

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh, which builds and " +
                "points at the host JNI shim (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
    }

    private fun open(dir: File = dataDir): AutoSwitchSettings =
        AutoSwitchSettings(
            dataDir = dir,
            readStored = { stored },
            writeStored = { value -> stored = value; writes++ },
        )

    @Test
    fun `the row shows the engine's value and the engine's key`() {
        open().use { settings ->
            val state = settings.state()
            assertEquals("OFF on a fresh install (B3, ruled 2026-10-11)", 0, state.on)
            assertEquals("the key the hook honours, named by the engine", "Ctrl+한/영", state.key)
            assertTrue("the shipped install carries the modes data", state.dataPresent)
            assertTrue("off, chosen is a usable row", state.usable)
        }
    }

    @Test
    fun `a write stores the engine's read-back, not the request`() {
        open().use { settings ->
            val on = settings.write(true)
            assertEquals(1, on.on)
            assertTrue("the read-back is what is remembered", stored)
            assertTrue(on.usable)

            val off = settings.write(false)
            assertEquals(0, off.on)
            assertFalse(stored)
        }
        assertTrue("the screen wrote through the shared store", writes >= 2)
    }

    @Test
    fun `without the modes data the engine refuses ON and the row is not usable`() {
        val data = File(repoRoot, "libhunmin/data")
        val tmp = runCatching {
            Files.createTempDirectory("hunmin-settings-gate-").toFile().also { dir ->
                for (name in listOf("layouts", "compose", "hanja", "idioms", "krdict")) {
                    val src = File(data, name)
                    if (src.isDirectory) {
                        Files.createSymbolicLink(File(dir, name).toPath(), src.toPath())
                    }
                }
            }
        }.getOrNull()
        assumeTrue("this host cannot create symlinks for the gate fixture", tmp != null)

        try {
            open(tmp!!).use { settings ->
                val state = settings.state()
                assertFalse("no modes directory: the engine must answer absent", state.dataPresent)
                assertFalse("the control cannot be offered", state.usable)
                assertEquals("the stored ON is refused", 0, state.on)

                val refused = settings.write(true)
                assertEquals("the request is never echoed", 0, refused.on)
                assertFalse("a refused ON is stored as OFF", stored)
            }
        } finally {
            // Delete the LINKS, never their targets: walking through a
            // directory symlink deletes the repository's data tree (the lesson
            // AutoSwitchGateTest records). Each entry is unlinked directly.
            tmp?.listFiles()?.forEach { runCatching { Files.deleteIfExists(it.toPath()) } }
            tmp?.let { runCatching { Files.deleteIfExists(it.toPath()) } }
        }
    }

    @Test
    fun `no readable data answers null instead of a false OFF`() {
        val missing = File(repoRoot, "does-not-exist-${System.nanoTime()}")
        open(missing).use { settings ->
            val state = settings.state()
            assertEquals("cannot answer is not the same as off", null, state.on)
            assertFalse(state.usable)
        }
    }
}
