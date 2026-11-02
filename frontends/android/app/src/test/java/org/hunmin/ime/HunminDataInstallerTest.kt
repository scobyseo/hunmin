/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: every data directory the APK ships is also a directory the installer
 * extracts (issue #177).
 *
 * WHY THIS EXISTS. Measured on the emulator 2026-11-03: `assets/data/modes/
 * en-US.json` was packaged into the APK by the asset sync, and
 * `HunminDataInstaller.DATA_SUBDIRS` did not name `modes`, so the file was
 * never written to filesDir. The engine therefore answered "modes absent" on
 * every device, the settings screen showed 사용할 수 없음 with the switch
 * disabled, and no surface could turn the automatic switch on -- while the APK
 * carried the data all along. Both tiers that existed were green: the engine
 * tests read `libhunmin/data` on the host (the directory is there), and the
 * installer has no Context on a JVM.
 *
 * So the assertion is the RELATION, not a copy of the list: the set of data
 * directories in the tree, minus the ones deliberately left out of the APK,
 * must equal what the installer copies. Adding a data directory without
 * deciding which side it belongs on now fails here instead of on a device.
 *
 * It needs no JNI shim and no Android Context, so it cannot be skipped away.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class HunminDataInstallerTest {

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }

    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }

    /**
     * Directories the asset sync leaves out of the APK, so the installer must
     * not expect them. `krdict` is excluded for size and for its CC-BY-SA
     * terms (frontends/android/app/build.gradle.kts, `syncHunminDataInstaller`).
     */
    private val notInTheApk = setOf("krdict")

    @Test
    fun `every data directory in the tree is one the installer extracts`() {
        assertTrue("no data tree at $dataDir", dataDir.isDirectory)

        val shipped = (dataDir.listFiles() ?: emptyArray())
            .filter { it.isDirectory }
            .filter { dir ->
                // A directory with no .json is not an engine table (the layout
                // and compose subdirs both carry files the loader fopen()s).
                (dir.listFiles() ?: emptyArray()).any { it.name.endsWith(".json") }
            }
            .map { it.name }
            .toSet() - notInTheApk

        assertEquals(
            "a data directory the APK ships but the installer does not extract " +
                "is invisible to the engine on a device -- that is how the modes " +
                "data came to be missing (#177). Add it to " +
                "HunminDataInstaller.DATA_SUBDIRS, or to `notInTheApk` here AND " +
                "to the asset sync's `exclude(...)` in build.gradle.kts",
            shipped,
            HunminDataInstaller.DATA_SUBDIRS.toSet(),
        )
    }

    @Test
    fun `the modes data is extracted, because the automatic switch cannot work without it`() {
        // The concrete regression, spelled out: the generic assertion above
        // would also pass if BOTH sides dropped `modes`.
        assertTrue(
            "modes/en-US.json is the judgement's word list; without it B3's gate " +
                "is closed on every device",
            HunminDataInstaller.DATA_SUBDIRS.contains("modes"),
        )
    }
}