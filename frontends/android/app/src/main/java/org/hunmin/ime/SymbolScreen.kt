/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * SymbolScreen — the app-read symbol screen data (issue #297, ruling KP11).
 *
 * The maintainer's ruling (2026-11-22, verbatim): "앱이 데이터 파일로 그리기.
 * 엔진은 관여 안함." So this file is the whole engine-side story: there is none.
 * The symbol inventory lives in assets/keypad/symbols.json — a committed
 * frontend asset, deliberately NOT under assets/data/ (which is synced from
 * libhunmin/data and is the engine's). The engine never loads it, no layout id
 * appears in any settings list, and no ABI symbol was added.
 *
 * The data contract (ruled arrangement, closed function-id vocabulary, KP9's
 * no-space/enter) is owned by tools/symbol-data-check.py, ctest
 * `keypad-symbol-data`. This parser validates only what drawing safely needs —
 * known kinds, non-empty single-character symbols, no duplicates — so a
 * corrupted asset fails loudly at load instead of rendering a blank key.
 *
 * A symbol key commits its own character directly (KP11: engine uninvolved).
 * [commitSymbol] is that seam. The caller (the soft-keyboard UI sub-issue)
 * must have flushed any pending composition BEFORE calling it: a direct
 * commit while the engine still holds a preedit would deliver the symbol
 * ahead of the buffered word (docs/DESIGN-ANDROID-SYMBOLS.md §4, FE1/FE2).
 * Function keys carry ids only; what an id does is the UI's wiring, not data.
 *
 * The variants section (issue #312, KP10) is parsed and exposed here so the
 * checker and the app agree on what the file carries — a section the checker
 * accepts but the app silently ignores is the recorded doctor/loader failure
 * mode (docs/DESIGN-ANDROID-SYMBOLS.md §1.2). Acting on it (swipe-up, the
 * long-press popup, KP5's defences) is gesture-UI wiring, still open; the
 * mapping itself is provisional (docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md).
 */
package org.hunmin.ime

import android.view.inputmethod.InputConnection
import org.json.JSONObject

/** One parsed symbol screen: rows of symbol characters or function-key ids. */
data class SymbolScreen(
    val rows: List<Row>,
    /** Gesture variants by host symbol (issue #312); empty when absent. */
    val variants: Map<String, Variants> = emptyMap(),
) {

    enum class Kind { SYMBOLS, FUNCTION }

    data class Row(val kind: Kind, val keys: List<String>)

    /**
     * One host key's gesture family: [up] is the swipe-up target and the
     * first popup member; [popup] is the long-press list in the file's order
     * (frequency order — the ranking's home is the data, not the app).
     */
    data class Variants(val up: String, val popup: List<String>)

    /** Every symbol character, in screen order. */
    val symbols: List<String> =
        rows.filter { it.kind == Kind.SYMBOLS }.flatMap { it.keys }

    /**
     * The app path for a symbol key: commit exactly the character, nothing
     * composing, engine uninvolved (KP11). Refuses a key that is not in this
     * screen's symbol inventory — a function id or an unknown string is a
     * caller bug, not something to silently type.
     */
    fun commitSymbol(key: String, ic: InputConnection) {
        require(key in symbols) { "not a symbol key of this screen: $key" }
        ic.commitText(key, 1)
    }

    companion object {
        /**
         * Parse the asset. Throws [IllegalArgumentException] on anything the
         * drawing code could not safely show; the content contract beyond
         * that is the Python checker's, run in ctest before this ever ships.
         */
        fun parse(text: String): SymbolScreen {
            val root = try {
                JSONObject(text)
            } catch (e: org.json.JSONException) {
                throw IllegalArgumentException("symbols.json is not valid JSON", e)
            }
            require(root.optInt("schemaVersion", -1) == 1) {
                "unsupported schemaVersion"
            }
            val rawRows = root.optJSONArray("rows")
            require(rawRows != null && rawRows.length() > 0) { "rows missing or empty" }
            val rows = ArrayList<Row>(rawRows.length())
            val seen = HashSet<String>()
            for (i in 0 until rawRows.length()) {
                val raw = rawRows.getJSONObject(i)
                val kind = when (val k = raw.optString("kind")) {
                    "symbols" -> Kind.SYMBOLS
                    "function" -> Kind.FUNCTION
                    else -> throw IllegalArgumentException("unknown row kind: $k")
                }
                val rawKeys = raw.optJSONArray("keys")
                require(rawKeys != null && rawKeys.length() > 0) {
                    "rows[$i]: keys missing or empty"
                }
                val keys = ArrayList<String>(rawKeys.length())
                for (j in 0 until rawKeys.length()) {
                    val key = rawKeys.getString(j)
                    require(key.isNotEmpty()) { "rows[$i]: empty key" }
                    if (kind == Kind.SYMBOLS) {
                        require(key.length == 1) {
                            "rows[$i]: symbol key must be one character: $key"
                        }
                    }
                    require(seen.add(key)) { "duplicate key: $key" }
                    keys.add(key)
                }
                rows.add(Row(kind, keys))
            }
            val symbolKeys = rows.filter { it.kind == Kind.SYMBOLS }
                .flatMap { it.keys }.toSet()
            val variants = HashMap<String, Variants>()
            val rawVariants = root.optJSONObject("variants")
            if (rawVariants != null) {
                for (host in rawVariants.keys()) {
                    require(host in symbolKeys) {
                        "variants: host is not a symbol key of this screen: $host"
                    }
                    val spec = rawVariants.getJSONObject(host)
                    val up = spec.optString("up")
                    val rawPopup = spec.optJSONArray("popup")
                    require(up.isNotEmpty() && rawPopup != null && rawPopup.length() > 0) {
                        "variants[$host]: up/popup missing or empty"
                    }
                    val popup = ArrayList<String>(rawPopup.length())
                    for (j in 0 until rawPopup.length()) {
                        val ch = rawPopup.getString(j)
                        require(ch.length == 1) {
                            "variants[$host]: variant must be one character: $ch"
                        }
                        popup.add(ch)
                    }
                    require(up == popup[0]) {
                        "variants[$host]: up must be the first popup member"
                    }
                    variants[host] = Variants(up, popup)
                }
            }
            return SymbolScreen(rows, variants)
        }
    }
}
