/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminDataInstaller — put the engine's layout tables where libhunmin can open them.
 *
 * libhunmin's loader is a filesystem scanner, and that is the whole constraint
 * here. hunmin_ctx_new() opendir()s `<dir>/compose` and `<dir>/layouts` and
 * fopen()s each `*.json` (libhunmin/src/data.c, hm_data_load). An Android APK
 * is a zip: there are no directories to opendir and no stable paths to fopen.
 * So the tables ship as assets -- the one packaging mechanism Android offers --
 * and are copied out to filesDir once, on first use.
 *
 * Why not assets alone? Because AssetManager can only read by name, and the
 * engine discovers layouts by scanning. Enumerating asset names in Kotlin and
 * feeding the engine a manifest would work and would also mean the engine has
 * two loading paths, one of which nothing else exercises. Extraction keeps one
 * path -- the same one the host build and the emulator tier E0 use.
 *
 * Versioning: the extraction is keyed on a hash of the extracted content, not
 * on a version string somebody has to remember to bump. A data change without a
 * bump would otherwise leave a device running stale tables with no symptom
 * except wrong composition, which is the hardest class of bug to report.
 */
package org.hunmin.ime

import android.content.Context
import android.util.Log
import java.io.File
import java.security.MessageDigest

object HunminDataInstaller {

    private const val TAG = "HunminDataInstaller"

    /** Subdirs libhunmin scans, in the order hm_data_load reads them. */
    private val DATA_SUBDIRS = listOf("layouts", "compose", "hanja", "idioms")

    /**
     * Where the tables live inside the APK.
     *
     * "data/" rather than "hunmin/data/" is arbitrary; what matters is that it
     * matches the asset paths Gradle packages, and that `hunmin-data-assets`
     * in build.gradle.kts adds the real libhunmin/data directory to the asset
     * source set.
     */
    private const val ASSET_ROOT = "data"

    /**
     * Extract the engine's tables if they are missing or stale. Returns the
     * directory to hand to hunmin_ctx_new(), or null on failure.
     *
     * Failure is not recoverable upward in any useful way: with no tables the
     * engine has no layouts and the IME cannot type Korean at all. The service
     * reports that in the UI rather than crashing on every keystroke.
     */
    fun install(context: Context): File? {
        val target = File(context.filesDir, "hunmin-data")
        val stamp = File(target, ".stamp")

        val wanted = digest(context)
        if (target.isDirectory && stamp.isFile &&
            stamp.readText() == wanted && complete(target)
        ) {
            return target
        }

        // A partial extraction from a killed process would read as complete to
        // the engine -- it only skips missing files, it has no way to tell a
        // short directory from a full one -- so start clean.
        target.deleteRecursively()
        if (!target.mkdirs()) {
            Log.e(TAG, "cannot create $target")
            return null
        }

        for (sub in DATA_SUBDIRS) {
            val files = try {
                context.assets.list("$ASSET_ROOT/$sub")?.toList().orEmpty()
            } catch (e: Exception) {
                Log.e(TAG, "cannot list assets under $ASSET_ROOT/$sub", e)
                return null
            }
            if (files.isEmpty()) {
                Log.e(TAG, "no assets under $ASSET_ROOT/$sub")
                return null
            }
            val dir = File(target, sub)
            if (!dir.mkdirs()) {
                Log.e(TAG, "cannot create $dir")
                return null
            }
            for (name in files) {
                if (!name.endsWith(".json")) continue
                try {
                    context.assets.open("$ASSET_ROOT/$sub/$name").use { input ->
                        File(dir, name).outputStream().use { output ->
                            input.copyTo(output)
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "cannot extract $sub/$name", e)
                    return null
                }
            }
        }

        stamp.writeText(wanted)
        return target
    }

    /** Every expected table actually present — the stamp alone can lie. */
    private fun complete(target: File): Boolean =
        DATA_SUBDIRS.all { sub ->
            val dir = File(target, sub)
            dir.isDirectory && (dir.listFiles { f -> f.name.endsWith(".json") }?.size ?: 0) > 0
        }

    /**
     * A content hash of the shipped tables.
     *
     * Names and sizes only, not contents: the tables are ~280 KB of JSON and
     * this runs once per process start, and a same-name same-size edit is not a
     * failure mode worth 280 KB of hashing to catch.
     */
    private fun digest(context: Context): String {
        val md = MessageDigest.getInstance("SHA-256")
        for (sub in DATA_SUBDIRS) {
            val names = context.assets.list("$ASSET_ROOT/$sub")?.sorted().orEmpty()
            for (name in names) {
                md.update("$sub/$name:".toByteArray())
                try {
                    context.assets.open("$ASSET_ROOT/$sub/$name").use { input ->
                        // Consume the stream so a truncated asset is caught here
                        // rather than at the engine's own read.
                        var total = 0L
                        val buf = ByteArray(16 * 1024)
                        while (true) {
                            val n = input.read(buf)
                            if (n < 0) break
                            total += n
                        }
                        md.update(total.toString().toByteArray())
                    }
                } catch (e: Exception) {
                    md.update("ERR".toByteArray())
                }
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }
}
