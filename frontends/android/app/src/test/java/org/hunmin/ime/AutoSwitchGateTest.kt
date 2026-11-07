package org.hunmin.ime

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File
import java.nio.file.Files

/**
 * The other half of B3's gate (issue #169), which the shipped install cannot
 * show: an install whose modes data is absent. The value must stay OFF and the
 * refusal must be distinguishable from "off, chosen", or a settings row could
 * offer a control that never fires.
 *
 * Built as a temp dir of symlinks to the repo's other data directories and NO
 * `modes/` -- the directory search the engine runs is the same one for every
 * data table, so an absent modes directory is exactly the state the money
 * question is about. (Measured 2026-11-01: ctx builds, `draws` 15 layouts,
 * answers data-present false, refuses ON and accepts OFF.)
 */
class AutoSwitchGateTest {

    private var ctx: Long = 0
    private var root: File? = null

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }

    /* Copy fallback for hosts where JVM symlinks need a privilege the test
     * JVM does not have (Windows without Developer Mode). The engines treat
     * a copy and a link identically: the directory search is the same. */
    private fun copyTree(src: java.nio.file.Path, dst: java.nio.file.Path) {
        Files.walk(src).use { stream ->
            stream.forEach { p ->
                val t = dst.resolve(src.relativize(p))
                if (Files.isDirectory(p)) Files.createDirectories(t) else Files.copy(p, t)
            }
        }
    }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        val data = File(repoRoot, "libhunmin/data")
        assumeTrue("libhunmin data tables missing at $data", data.isDirectory)
        val tmp = runCatching {
            Files.createTempDirectory("hunmin-gate-").toFile().also { dir ->
                for (name in listOf("layouts", "compose", "hanja", "idioms", "krdict")) {
                    val src = File(data, name)
                    if (src.isDirectory) {
                        runCatching {
                            Files.createSymbolicLink(File(dir, name).toPath(), src.toPath())
                        }.onFailure {
                            copyTree(src.toPath(), File(dir, name).toPath())
                        }
                    }
                }
            }
        }.getOrNull()
        // If neither a link nor a copy could be built, skipping states
        // exactly what was measured instead of a green case on a wrong tree.
        assumeTrue("the gate fixture could not be built (link and copy both failed)", tmp != null)
        root = tmp
        ctx = HunminNativeBridge.nativeCtxNew(tmp!!.absolutePath)
        assertTrue(
            "the fixture must still be a usable install -- otherwise this asserts nothing",
            ctx != 0L,
        )
    }

    @After
    fun tearDown() {
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
        /* Symlink entries are unlinked with Files.deleteIfExists (deleting
         * the link, never its target); copy-fallback entries are real
         * directories and go with deleteRecursively instead. The split is
         * what makes both shapes clean up without ever walking through a
         * link into the repository's own data tree. */
        root?.listFiles()?.forEach { entry ->
            runCatching {
                if (Files.isSymbolicLink(entry.toPath())) Files.deleteIfExists(entry.toPath())
                else if (entry.isDirectory) entry.deleteRecursively()
                else Files.deleteIfExists(entry.toPath())
            }
        }
        root?.let { runCatching { Files.deleteIfExists(it.toPath()) } }
    }

    @Test
    fun `without the modes data ON is refused and the row can say why`() {
        assertFalse(
            "no modes/ directory: the data gate must answer absent",
            HunminNativeBridge.nativeCtxAutoswitchDataPresent(ctx),
        )
        val adapter = HunminAdapter(ctx, "sin3-p2", "us-qwerty", "word")
        assertTrue("premise: the layer is usable", adapter.isUsable)

        /* The read-back, not the request: this is what the settings row shows
         * and what the caller stores, so a refused ON must read as 0. If it read
         * as 1, the row would claim a feature the judgement cannot use. */
        assertEquals("ON is refused by the gate", 0, adapter.setAutoswitch(true))
        assertEquals("...and the engine still answers the name", 0, adapter.autoswitch())

        /* OFF is always accepted, so an install that lost its data can still
         * record the preference -- and the two answers above stay distinguishable
         * from "off, chosen" via autoswitchDataPresent(). */
        assertEquals(0, adapter.setAutoswitch(false))
        adapter.close()
    }

    @Test
    fun `the judgement stays silent without the data`() {
        /* B3's fail-closed rule: no dictionary means no `english_valid`, so the
         * judgement abstains instead of treating every word as not-English. The
         * keys and the pair are the ones AutoSwitchTest switches on with the data
         * present, so the difference between the two runs is the data and
         * nothing else. */
        val adapter = HunminAdapter(ctx, "sin3-p2", "us-qwerty", "word")
        for (ch in "computer") {
            val key = HunminKeyRegistry.all().firstOrNull { it.unshifted == ch } ?: error("no keycode for $ch")
            adapter.onKey(key.keyCode, 0, RecordingInputConnection())
        }
        val space = HunminKeyRegistry.all().first { it.unshifted == ' ' }
        adapter.onKey(space.keyCode, 0, RecordingInputConnection())
        assertEquals("nothing judged without the modes data", -1, adapter.takeJudgement())
        adapter.close()
    }
}