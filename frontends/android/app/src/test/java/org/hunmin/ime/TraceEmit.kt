/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * TraceEmit — tier C trace emitter for the Android frontend.
 *
 * Reads a platform-native trace (docs/trace-spec.md, Android dialect) from a file,
 * replays it through the real Android adapter (HunminAdapter + host JNI shim),
 * and emits the observed trace in the same JSONL format.
 *
 * Usage: TraceEmit <trace.in>
 *
 * Exit codes:
 *   0  success (trace emitted to stdout)
 *   1  error (bad input, JNI failure, etc.)
 *   77 skip (host JNI shim not available)
 *
 * This is the Android counterpart of frontends/fcitx5/test/trace-emit.cpp.
 * It runs on the HOST (JVM), using the host build of the JNI shim
 * (tools/build-host-jni.sh), NOT on the emulator. The emulator is for
 * tier E2/E3; tier C is a host-only cross-frontend agreement proof.
 */
package org.hunmin.ime

import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

object TraceEmit {

    private const val TAG = "TraceEmit"

    @JvmStatic
    fun main(args: Array<String>) {
        if (args.size < 1) {
            System.err.println("usage: TraceEmit <trace.in>")
            System.exit(2)
        }

        val traceFile = File(args[0])
        if (!traceFile.exists()) {
            System.err.println("trace-emit: cannot open $traceFile")
            System.exit(2)
        }

        // Load the host JNI shim. The test JVM does not inherit the shell
        // environment, so HUNMIN_JNI_LIB and HUNMIN_REPO_ROOT must be passed
        // explicitly by the runner (tools/trace-check.py via ctest).
        val shimPath = System.getenv("HUNMIN_JNI_LIB")
            ?: "".also { System.err.println("skip: HUNMIN_JNI_LIB not set"); System.exit(77) }
        val repoRoot = System.getenv("HUNMIN_REPO_ROOT")
            ?: "".also { System.err.println("skip: HUNMIN_REPO_ROOT not set"); System.exit(77) }

        val shimFile = File(shimPath)
        if (!shimFile.exists()) {
            System.err.println("skip: host JNI shim not found at $shimPath (run tools/build-host-jni.sh)")
            System.exit(77)
        }

        System.load(shimFile.absolutePath)

        val dataDir = File(repoRoot, "libhunmin/data")
        if (!dataDir.isDirectory) {
            System.err.println("trace-emit: libhunmin data not found at $dataDir")
            System.exit(2)
        }

        val lines = traceFile.readLines().filter { it.isNotBlank() }
        if (lines.isEmpty() || !lines[0].contains("\"meta\"")) {
            System.err.println("trace-emit: line 1 must be meta (docs/trace-spec.md)")
            System.exit(2)
        }

        // Parse meta line
        val meta: Map<String, Any> = parseJson(lines[0]) as Map<String, Any>
        val metaObjNullable = meta["meta"] as? Map<String, Any>
        if (metaObjNullable == null) { System.err.println("trace-emit: meta object missing"); System.exit(2) }
        val metaObj = metaObjNullable!!
        val layout = (metaObj["layout"] as? String) ?: { System.err.println("trace-emit: meta.layout missing"); System.exit(2); "" }()
        val platform = (metaObj["platform"] as? String) ?: { System.err.println("trace-emit: meta.platform missing"); System.exit(2); "" }()
        if (platform != "android") {
            System.err.println("trace-emit: platform '$platform' not supported by this runner")
            System.exit(2)
        }

        // Create engine context
        val ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        if (ctx == 0L) {
            System.err.println("trace-emit: hunmin_ctx_new found no layout in ${dataDir.absolutePath}")
            System.exit(2)
        }

        // Check layout is supported
        val n = HunminNativeBridge.nativeLayoutCount(ctx)
        var layoutSupported = false
        for (i in 0 until n) {
            if (HunminNativeBridge.nativeLayoutSupported(ctx, i) &&
                HunminNativeBridge.nativeLayoutId(ctx, i) == layout) {
                layoutSupported = true
                break
            }
        }
        if (!layoutSupported) {
            System.err.println("trace-emit: layout '$layout' not supported")
            System.exit(2)
        }

        // Create input context. `var`, because a batch file resets it at every
        // {"meta": {"case": ...}} separator (docs/trace-spec.md "Multi-case
        // batch files"): tier A replays each case from a clean engine, and a
        // runner that keeps one context would carry case N's composition into
        // case N+1 -- the TSF emitter has done this since issue #80.
        var ic = HunminNativeBridge.nativeIcNew(ctx, layout)
        if (ic == 0L) {
            System.err.println("trace-emit: hunmin_ic_new failed for layout '$layout'")
            System.exit(2)
        }

        // Echo the meta line
        println(lines[0])

        // Pre-load expected commits from the trace (for validation, like fcitx5 does)
        val expectedCommits = mutableListOf<String>()
        for (i in 1 until lines.size) {
            val line = lines[i]
            if (line.contains("\"meta\"")) continue
            val rec = parseJson(line)
            val commit = rec["commit"] as? String ?: ""
            expectedCommits.add(commit)
        }

        // Replay events
        var emitted = 0
        var previousPreedit = ""

        for (i in 1 until lines.size) {
            val line = lines[i]
            if (line.contains("\"meta\"")) {
                // Case separator: a fresh engine, exactly as tier A replays it.
                HunminNativeBridge.nativeIcFree(ic)
                ic = HunminNativeBridge.nativeIcNew(ctx, layout)
                if (ic == 0L) {
                    System.err.println("trace-emit: hunmin_ic_new failed at a case separator")
                    System.exit(2)
                }
                previousPreedit = ""
                println(line)
                continue
            }

            val rec = parseJson(line)
            val inObj = rec["in"] as? Map<String, Any>
                ?: continue

            val keyCode = (inObj["keycode"] as? Number)?.toInt() ?: 0
            val mods = (inObj["mods"] as? Number)?.toInt() ?: 0
            // A shifted SYMBOL has no case to carry the shift (`{"ch": ",",
            // "shift": true}` is Shift+, on dvorak's W), so the event names it:
            // either the `shift` key or mods bit 0x01, the same bit win32 uses.
            val shift = (inObj["shift"] as? Boolean) ?: ((mods and 0x01) != 0)

            // Feed key to engine. Shift goes through hunmin_ic_process_key, the
            // explicit-modifier entry the adapter itself uses (issue #90);
            // hunmin_ic_process alone derives shift from the key id's case,
            // which a punctuation key does not have.
            val used = if (shift) HunminNativeBridge.nativeProcessKey(ic, keyCode, 1)
                       else HunminNativeBridge.nativeProcess(ic, keyCode)

            var actualCommit = ""
            var preedit = ""

            if (used) {
                // Read commit (destructive)
                actualCommit = HunminNativeBridge.nativeCommit(ic) ?: ""
                if (actualCommit.isNotEmpty()) {
                    // In real IME this would go to InputConnection.commitText
                }
            } else {
                // Engine declined: flush (destructive read of commit + preedit)
                actualCommit = HunminNativeBridge.nativeFlush(ic) ?: ""
            }

            // Read preedit (non-destructive)
            preedit = HunminNativeBridge.nativePreedit(ic) ?: ""

            // Validate against expected commit (like fcitx5 TestFrontend does)
            val expected = if (emitted < expectedCommits.size) expectedCommits[emitted] else ""
            emitted++

            if (actualCommit != expected) {
                System.err.println("trace-emit: commit mismatch at event $emitted")
                System.err.println("  expected: $expected")
                System.err.println("  got:      $actualCommit")
                System.exit(1)
            }

            // Emit trace line in the same format as input (docs/trace-spec.md)
            // Use EXPECTED commit (from trace file) and ACTUAL preedit (from engine)
            // This matches fcitx5 trace-emit.cpp behavior
            // Echo the `in` the trace gave us, field for field: a trace may
            // encode Shift as mods bit 0x01 (the arrow-selection corpus always
            // has), and adding a `shift` key it did not carry made the emitted
            // line differ from its own .in -- measured on
            // tests/conformance/android/shift-arrow-selection.trace.in.
            val inEcho = if (inObj.containsKey("shift"))
                mapOf("keycode" to keyCode, "mods" to mods, "shift" to true)
            else mapOf("keycode" to keyCode, "mods" to mods)
            val out = mapOf(
                "in" to inEcho,
                "commit" to expected,
                "preedit" to preedit,
                "consumed" to used
            )
            println(jsonEncode(out))

            previousPreedit = preedit
        }

        // Teardown: validate any remaining composition is flushed (like fcitx5 TestFrontend does)
        // But do NOT output it as a trace line - trace has one line per input event
        if (previousPreedit.isNotEmpty()) {
            val tail = HunminNativeBridge.nativeFlush(ic) ?: ""
            if (tail.isNotEmpty()) {
                // Validate tail commit matches what would be expected if there were another event
                // The trace file doesn't record this, so we just validate it doesn't crash
            }
        }

        HunminNativeBridge.nativeIcFree(ic)
        HunminNativeBridge.nativeCtxFree(ctx)
    }

    /** Minimal JSON parser for the exact trace format. */
    private fun parseJson(line: String): Map<String, Any> {
        // Use a simple parser since we control the format exactly
        // This avoids pulling in a JSON library for a test tool
        return parseJsonValue(line) as Map<String, Any>
    }

    private fun parseJsonValue(s: String): Any {
        val trimmed = s.trim()
        if (trimmed.startsWith("{")) {
            return parseJsonObject(trimmed)
        } else if (trimmed.startsWith("[")) {
            return parseJsonArray(trimmed)
        } else if (trimmed.startsWith("\"")) {
            return parseJsonString(trimmed)
        } else if (trimmed == "true" || trimmed == "false") {
            return trimmed.toBoolean()
        } else if (trimmed == "null") {
            return Unit
        } else {
            return try {
                trimmed.toLong()
            } catch (e: NumberFormatException) {
                try {
                    trimmed.toDouble()
                } catch (e2: NumberFormatException) {
                    trimmed
                }
            }
        }
    }

    private fun parseJsonObject(s: String): Map<String, Any> {
        val content = s.substring(1, s.length - 1).trim()
        val map = mutableMapOf<String, Any>()
        var i = 0
        while (i < content.length) {
            // Skip whitespace
            while (i < content.length && content[i].isWhitespace()) i++
            if (i >= content.length) break

            // Parse key
            require(content[i] == '"') { "Expected '\"' at $i in $content" }
            i++
            val keyStart = i
            while (i < content.length && content[i] != '"') {
                if (content[i] == '\\' && i + 1 < content.length) i += 2 else i++
            }
            val key = content.substring(keyStart, i)
            i++ // skip closing "

            // Skip whitespace and colon
            while (i < content.length && content[i].isWhitespace()) i++
            require(i < content.length && content[i] == ':') { "Expected ':' at $i in $content" }
            i++

            // Skip whitespace
            while (i < content.length && content[i].isWhitespace()) i++

            // Parse value
            val valueStart = i
            var depth = 0
            var inString = false
            var escaped = false
            while (i < content.length) {
                val c = content[i]
                if (!escaped && c == '"') {
                    inString = !inString
                } else if (!escaped && c == '\\') {
                    escaped = true
                    i++
                    continue
                } else if (!inString && (c == '{' || c == '[')) {
                    depth++
                } else if (!inString && (c == '}' || c == ']')) {
                    depth--
                } else if (!inString && depth == 0 && c == ',') {
                    break
                }
                escaped = false
                i++
            }
            val valueStr = content.substring(valueStart, i).trim()
            map[key] = parseJsonValue(valueStr)

            // Skip comma
            while (i < content.length && content[i].isWhitespace()) i++
            if (i < content.length && content[i] == ',') i++
        }
        return map
    }

    private fun parseJsonArray(s: String): List<Any> {
        // Not used in trace format but included for completeness
        return emptyList()
    }

    private fun parseJsonString(s: String): String {
        val content = s.substring(1, s.length - 1)
        val sb = StringBuilder()
        var i = 0
        while (i < content.length) {
            val c = content[i]
            if (c == '\\' && i + 1 < content.length) {
                val next = content[i + 1]
                when (next) {
                    '"' -> sb.append('"')
                    '\\' -> sb.append('\\')
                    'n' -> sb.append('\n')
                    'r' -> sb.append('\r')
                    't' -> sb.append('\t')
                    'u' -> {
                        if (i + 5 < content.length) {
                            val hex = content.substring(i + 2, i + 6)
                            sb.append(hex.toInt(16).toChar())
                            i += 5
                        } else {
                            sb.append(next)
                        }
                    }
                    else -> sb.append(next)
                }
                i += 2
            } else {
                sb.append(c)
                i++
            }
        }
        return sb.toString()
    }

    /** Minimal JSON encoder for the trace output format. */
    private fun jsonEncode(map: Map<String, Any>): String {
        val sb = StringBuilder()
        sb.append('{')
        var first = true
        for ((k, v) in map) {
            if (!first) sb.append(',')
            first = false
            sb.append('"').append(k).append("\":")
            jsonEncodeValue(sb, v)
        }
        sb.append('}')
        return sb.toString()
    }

    private fun jsonEncodeValue(sb: StringBuilder, value: Any) {
        when (value) {
            is String -> {
                sb.append('"')
                for (c in value) {
                    when (c) {
                        '"' -> sb.append("\\\"")
                        '\\' -> sb.append("\\\\")
                        '\n' -> sb.append("\\n")
                        '\r' -> sb.append("\\r")
                        '\t' -> sb.append("\\t")
                        else -> {
                            if (c < ' ') {
                                sb.append(String.format("\\u%04x", c.toInt()))
                            } else {
                                sb.append(c)
                            }
                        }
                    }
                }
                sb.append('"')
            }
            is Boolean -> sb.append(value)
            is Number -> sb.append(value)
            is Map<*, *> -> {
                sb.append('{')
                var first = true
                for ((k, v) in value) {
                    if (!first) sb.append(',')
                    first = false
                    sb.append('"').append(k).append("\":")
                    jsonEncodeValue(sb, v as Any)
                }
                sb.append('}')
            }
            else -> sb.append("null")
        }
    }
}