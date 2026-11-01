/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminInputMethodService — the Android IME entry point.
 *
 * Lifecycle, per docs/TESTING.md tier E:
 *
 *   onCreate      extract the tables, build the ctx, remember the active layout
 *   onStartInput  build a fresh HunminAdapter for THIS editor  (two editors must
 *                 not share a composition buffer)
 *   onKeyDown     map the keycode, feed the engine, present commit + preedit
 *   onStartInputView  show a minimal status view -- see onEvaluateInputViewShown
 *
 * The hardware-keyboard path is the whole product. InputMethodService routes
 * key events to an IME through the IME's window token, and whether that routing
 * happens at all depends on the window being present: this is the single most
 * important thing to verify on a device, and it is verified rather than assumed
 * (see tools/android-e2e.sh and docs/FRONTEND-CONTRACT.md §3, B1).
 */
package org.hunmin.ime

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ApplicationInfo
import android.inputmethodservice.InputMethodService
import android.util.Log
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection

class HunminInputMethodService : InputMethodService() {

    companion object {
        /** SharedPreferences key for the commit unit; values [COMMIT_WORD] / [COMMIT_SYLLABLE]. */
        const val KEY_COMMIT_BEHAVIOR = "commit_behavior"
        const val COMMIT_WORD = "word"
        const val COMMIT_SYLLABLE = "syllable"

        private const val TAG = "HunminIme"
        /** hunmin.h's HUNMIN_LANGUAGE_LATIN; the judgement reports a mode as one of these. */
        private const val HUNMIN_LANGUAGE_LATIN = 1 /* hunmin.h: HANGUL 0, LATIN 1 */

        /**
         * Whether an EditorInfo's inputType marks a password editor.
         *
         * Extracted from onStartInput as a pure function for two reasons. It is
         * the whole secure-editor policy in one place, and it is testable from
         * a plain JVM: an InputMethodService cannot be constructed there, so an
         * inline check in onStartInput could only ever be exercised on a device.
         *
         * TYPE_NULL and TYPE_CLASS_TEXT with a password *class* are
         * deliberately NOT matched. Guessing at those produces a plain editor
         * that silently stops composing, which is a worse failure than the one
         * being fixed; the two variations below are what the platform actually
         * sets for a password field.
         *
         * The constants are spelled out because android.view.inputmethod
         * .EditorInfo cannot be touched from a unit test without a device
         * runtime, and the values are fixed by the framework. Verified with
         * `javap -p -v -constants` against android.jar (API 35, 2026-10-13):
         *   TYPE_MASK_VARIATION                    = 4080   (0xff0)
         *   TYPE_TEXT_VARIATION_PASSWORD           = 128    (0x80)
         *   TYPE_TEXT_VARIATION_VISIBLE_PASSWORD   = 144    (0x90)
         *
         * NOTE THE MASK IS 0xff0, NOT 0xfff. The variation field occupies bits
         * 4..11 and the mask covers exactly those; a 0xfff mask also takes bit
         * 3. Writing 0xfff here was measured wrong: SecureEditorTest failed on
         * "0x80 must survive the mask", and the production guard would have
         * mis-read a password editor as plain in the field.
         */
        private const val TYPE_MASK_VARIATION = 0xff0
        private const val VARIATION_PASSWORD = 0x80
        private const val VARIATION_VISIBLE_PASSWORD = 0x90

        internal fun isSecureEditorInputType(inputType: Int): Boolean {
            val variation = inputType and TYPE_MASK_VARIATION
            return variation == VARIATION_PASSWORD ||
                variation == VARIATION_VISIBLE_PASSWORD
        }

        /**
         * The layout to start in. Matches the fcitx5 addon's default so the two
         * frontends agree on what an unconfigured install types.
         */
        const val DEFAULT_LAYOUT = "sin3-p2"

        /**
         * The Latin half's default. The only latin layout today
         * (`docs/SPEC-SCHEMA.md`'s `mode` field), and what the service used to
         * hardcode for `latinEngine`.
         */
        const val DEFAULT_LATIN_LAYOUT = "us-qwerty"

        /** Broadcast action that toggles the tier E3 trace sink. */
        const val ACTION_TRACE = "org.hunmin.ime.TRACE"

        /** Where the trace is written, inside filesDir so `run-as` can read it. */
        const val TRACE_FILE = "e3.trace"

        private const val PREFS = "hunmin"
        /**
         * The pre-two-mode key. Read once for migration and never written
         * again: a device that stored `layout = sin3-p2-yet` under the old
         * single-layout scheme must not lose that choice to an upgrade.
         */
        private const val KEY_LAYOUT = "layout"
        private const val KEY_LAYOUT_HANGUL = "layout_hangul"
        private const val KEY_LAYOUT_LATIN = "layout_latin"
        private const val KEY_TOOLBAR_STYLE = "input_view_style"
        private const val KEY_HANGUL_MODE = "hangul_mode"

        /**
         * The platform's persistence for a value the ENGINE owns
         * (docs/SPEC-MODES.md §R13-정정): the layer is rebuilt per editor and its
         * ic starts OFF, so this is what carries the choice to the next one.
         * Only the read-back is written here, never the request.
         */
        private const val KEY_AUTOSWITCH = "autoswitch"
    }

    /** One engine per InputConnection; rebuilt on every onStartInput. */
    private var engine: HunminAdapter? = null

    /**
     * The IME's input view, held so its selection survives a view recreation.
     *
     * A fresh picker would come up showing row 0 while the engine was on some
     * other layout, and the two would disagree until the next keystroke.
     */
    private var picker: LayoutPicker? = null

    /** Non-null exactly while a voice attempt is in flight. */
    private var voiceSession: VoiceSession? = null

    /**
     * `fieldId` of the editor a voice attempt started in, so a result arriving
     * after the user switched fields is not committed into the wrong one.
     */
    private var voiceFieldId: Int = 0

    /** The engine context; 0 until hunmin_ctx_new succeeds. */
    private var ctx: Long = 0

    /** Layout picker state survives engine rebuilds; it is a service field. */
    private var layoutId: String = DEFAULT_LAYOUT

    /**
     * The layout English mode composes with, chosen independently of [layoutId].
     *
     * Separate because the two are separate: a user who types 신세벌식 in Korean
     * may type qwerty (or dvorak, once it exists) in English, and the old single
     * `layoutId` forced one answer for both.
     */
    private var latinLayoutId: String = DEFAULT_LATIN_LAYOUT

    /** Hangul/English mode. True = Hangul, False = English (Latin). */
    private var isHangulMode: Boolean = true

    /** Latin layout engine for English mode. */
    private var latinEngine: HunminAdapter? = null

    /**
     * True when the current editor is a password field.
     *
     * An IME must not compose into an editor the user cannot see: a password
     * field renders dots, so a composing span is invisible and the Hangul the
     * engine would produce is unrecoverable text. Measured 2026-10-13 on the
     * AVD (issue #13): with no guard, `p,a,s,s` on sin3-p2 in a password
     * EditText composed 퓬ㄴ instead of committing `pass`.
     *
     * Set per-editor in [onStartInput] from the EditorInfo, not once at
     * startup: the same IME session serves a plain editor and a password one
     * in turn, and the mode follows the editor.
     */
    private var isSecureEditor: Boolean = false

    /**
     * Where the Hangul/English mode choice is remembered across process
     * restarts. `by lazy` for the same reason as [keyBindings]: the service
     * context is not usable before `onCreate`, and `onStartInput` is the
     * first place that reads it back.
     */
    /**
     * The commit unit (issue #128): `word` buffers the whole word and delivers
     * it at the boundary, `syllable` delivers each syllable as the engine
     * finalises it. The TEXT is identical; only the point it reaches the client
     * changes, which is why this is a preference and not a behaviour fork.
     *
     * Read at construction time, like the layout ids: a change takes effect on
     * the next input session, and the layer is rebuilt because the policy is
     * part of its identity.
     */
    private val commitBehavior: String
        get() = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_COMMIT_BEHAVIOR, COMMIT_WORD) ?: COMMIT_WORD

    private val modeStore: HangulModeRepository by lazy {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        HangulModeRepository(
            read = { key, default -> prefs.getBoolean(key, default) },
            write = { key, value -> prefs.edit().putBoolean(key, value).apply() },
        )
    }

    /**
     * The configurable key bindings, read from the window that writes them.
     *
     * One store for the process: `getSharedPreferences` caches its map, so this
     * is a lookup per key event and not a disk read. It is a `by lazy` because
     * the service context is not usable before `onCreate`.
     */
    private val keyBindings: KeyBindingRepository by lazy { KeyBindings.from(this) }

    /** Does this event fire [action]? Shorthand for the four checks below. */
    private fun fires(action: KeyAction, keyCode: Int, metaState: Int): Boolean =
        KeyBindings.matches(keyBindings, action, keyCode, metaState)

    /**
     * What this frontend can do with the engine's special functions, and how.
     *
     * The engine asks [supports] before it performs anything; false is the
     * refusal the ruling allows ("if the feature is not supported at the
     * platform, it can just refuse to service"). On Android both are supported:
     *
     *  - VOICE drives the platform recogniser ([startVoiceInput]) and asks for
     *    RECORD_AUDIO itself, so the chord is consumed and the user gets the
     *    permission prompt (or the session) instead of a key that silently
     *    reaches the editor.
     *  - AUTOSWITCH was REFUSED until it was implemented here (issue #174); the
     *    2026-10-12 inert-shipping ruling it implemented (#2) is superseded by
     *    the engine feature landing (issues #164/#165/#169). The engine flips
     *    its own value in `hunmin_buffered_preedit_special()` and REFUSES the
     *    press itself when the modes data is absent (B3's gate), so this host
     *    says yes and does only the platform half: making the other layer and
     *    the stored preference agree, and telling the user.
     *
     * The object handed over is [HunminSpecialHost]: JNI looks its two methods up
     * by name, which is why the callbacks live there and not here.
     */
    private val specialServices = object : SpecialServices {
        override fun supports(fn: Int): Boolean = when (fn) {
            SpecialFunction.VOICE -> true
            SpecialFunction.AUTOSWITCH -> true
            else -> false
        }

        override fun perform(fn: Int) {
            when (fn) {
                SpecialFunction.VOICE -> startVoiceInput()
                SpecialFunction.AUTOSWITCH -> {
                    /* The ENGINE already flipped the value (ruled 2026-11-01,
                     * #165) -- reading it back rather than inverting it again is
                     * what keeps this from being a second copy. The platform
                     * half: the other layer holds its own ic, and the next
                     * editor's layer starts OFF, so both follow the answer.
                     *
                     * This runs INSIDE hunmin_buffered_preedit_special(), so the
                     * get/set calls below re-enter the layer that is mid-call.
                     * That is bounded and safe by construction: the option pair
                     * touches one field (`ic->autoswitch`), the engine has
                     * already finished reading its own host struct, and the
                     * remaining frame only returns the function number. Nothing
                     * here feeds a key, so no composition state is re-entered. */
                    val v = autoswitchValue() ?: return
                    applyAutoswitch(v == 1)
                    announceAutoswitch(v, wanted = v == 1)
                }
            }
        }
    }

    private val specialHost = HunminSpecialHost(specialServices)

    override fun onCreate() {
        super.onCreate()
        registerTraceReceiver()
        registerHanjaReceiver()
        val dataDir = HunminDataInstaller.install(this)
        if (dataDir == null) {
            // Without tables there is no layout and no Korean at all. Failing
            // here (rather than per keystroke) keeps the symptom one log line
            // instead of a stream of silent no-ops.
            Log.e(TAG, "hunmin: no layout tables extracted; IME is inert")
            return
        }
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        if (ctx == 0L) {
            Log.e(TAG, "hunmin: hunmin_ctx_new found no layout in ${dataDir.absolutePath}")
            return
        }
        // The stored choice wins over the default, but pickLayout() validates
        // it: a preference naming a layout this build cannot run must degrade to
        // the default rather than leave the IME with no engine.
        installSettingsBus()
        // Two layouts, one per mode, each resolved against what THIS build can
        // open. LayoutRegistry owns the fallback chain so it is testable without
        // an engine -- see its KDoc for why the mode check is not optional.
        val catalog = LayoutRegistry.sorted(availableLayouts())
        layoutId = LayoutRegistry.resolve(
            catalog, LayoutMode.HANGUL,
            readStoredLayout(LayoutMode.HANGUL), DEFAULT_LAYOUT,
        )
        latinLayoutId = LayoutRegistry.resolve(
            catalog, LayoutMode.LATIN,
            readStoredLayout(LayoutMode.LATIN), DEFAULT_LATIN_LAYOUT,
        )
        Log.i(
            TAG,
            "hunmin: ${HunminNativeBridge.nativeLayoutCount(ctx)} layouts, " +
                "using $layoutId (한글) and $latinLayoutId (영문)",
        )
    }

    override fun onDestroy() {
        // Close the trace before anything else: a sink left open past onDestroy
        // would keep appending to a file nothing can pull, because the service
        // that could answer the pull is gone.
        HunminTraceRecorder.close()
        traceReceiver?.let { runCatching { unregisterReceiver(it) } }
        traceReceiver = null
        runCatching { unregisterReceiver(hanjaReceiver) }
        engine?.close()
        engine = null
        if (ctx != 0L) {
            HunminNativeBridge.nativeCtxFree(ctx)
            ctx = 0
        }
        super.onDestroy()
    }

    /**
     * A new editor means a new engine.
     *
     * The old one is closed, which frees its hunmin_ic. Its composition is NOT
     * committed: focus moved, and the fcitx5 adapter's equivalent (flush on
     * reset) is a UX choice about focus loss, not a requirement. Dropping is the
     * Android convention -- the client has already been told a new session
     * started. Recorded here because the two frontends differ on purpose.
     */
    override fun onStartInput(info: EditorInfo?, restarting: Boolean) {
        super.onStartInput(info, restarting)
        engine?.close()
        latinEngine?.close()
        /* The hangul adapter carries TWO slots (FE7): the layer performs the
         * language action, so `latinEngine` survives only for the paths that
         * still address the latin slot directly. */
        engine = if (ctx == 0L) null else HunminAdapter(ctx, layoutId, latinLayoutId, commitBehavior)
        /* The latin adapter is ONE-language on purpose: it is addressed directly
         * by the voice/latin paths, and the pair lives on `engine` (FE7). */
        latinEngine = if (ctx == 0L) null else HunminAdapter(ctx, latinLayoutId, null, commitBehavior)
        /* The special-function host (ruled 2026-10-31) is registered on BOTH
         * layers: the chord is resolved by whichever engine is composing, and a
         * host on only the hangul one would make Alt+한/영 dead in English mode. */
        engine?.registerSpecialHost(specialHost)
        latinEngine?.registerSpecialHost(specialHost)
        /* Both layers are new, so the engine's option is at its default (OFF,
         * B3) whatever the person chose before. The preference is the platform's
         * persistence for a value the ENGINE owns (R13-정정), and a refusal --
         * the data gate -- is stored as the engine's answer rather than as the
         * request. */
        restoreAutoswitch()
        if (engine?.isUsable != true) {
            Log.e(TAG, "hunmin: layout '$layoutId' unsupported; keys will pass through")
        }
        // A password editor gets no composition at all -- see isSecureEditor.
        // Both the PASSWORD variation and the VISIBLE_PASSWORD one: the field
        // masks its content either way, and the IME cannot tell the user what
        // it composed.
        val wasSecure = isSecureEditor
        isSecureEditor = isSecureEditorInputType(info?.inputType ?: 0)
        if (isSecureEditor != wasSecure) {
            Log.i(TAG, "hunmin: secure editor=$isSecureEditor")
        }
        // Restore the last-used Hangul/English mode from prefs (Android's
        // InputMethodService is killed and recreated often), falling back to
        // Hangul when there is no stored value (first install).
        isHangulMode = modeStore.recall(KEY_HANGUL_MODE)
        currentInputConnection?.let { engine?.updatePreedit(it) }
        // The view is created once per IME window but the engine per editor, so
        // the picker's list and selection are refreshed here rather than trusted
        // to still match.
        refreshStrip()
        voiceFieldId = info?.fieldId ?: 0
    }

    override fun onFinishInput() {
        engine?.close()
        engine = null
        super.onFinishInput()
    }

    /**
     * Keep the picker in step with the engine.
     *
     * `setLayout` changes the engine from outside the picker (the service
     * restores a stored choice, and a future key chord could switch layouts), so
     * without this the spinner and the engine could disagree. Cheap, and it only
     * writes when the index actually differs.
     */
    /**
     * Put the engine's layout list and the active one into the strip.
     *
     * The strip no longer CHOOSES (that is the settings window's job), so this is
     * one-way: list in, label out.
     */
    private fun refreshStrip() {
        picker?.applyStyle(toolbarStyle())
        // The strip follows the SERVICE's mode, not its own last value.
        // onStartInput restarts every session in Hangul mode, so a strip that
        // only ever heard about toggles would keep showing the letter from the
        // previous field while the IME had already switched back. Caught by tier
        // E2 asserting the letter before a rebound keypress.
        picker?.setMode(isHangulMode)
        picker?.setLayouts(LayoutRegistry.sorted(availableLayouts()))
        // The indicator shows the layout that is composing right now, so it is
        // the English one in English mode — a strip that said "sin3-p2" while
        // qwerty was composing would be showing the wrong name.
        picker?.setActiveLayout(currentLayout())
    }

/**
     * Feed one key. Returns true when the engine consumed it.
     *
     * Two details that are easy to get wrong and produce a "the IME eats some
     * keys" bug:
     *
     *  - Release events are dropped by the ACTION check, not by keyCode. A
     *    key that goes down under one IME and up under another would otherwise
     *    compose twice.
     *  - The engine's decision, not the keycode table's, decides consumption.
     *    Space, unmapped keys and modifier chords must reach the application;
     *    swallowing them would break every shortcut behind the IME.
     */
    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (event.action != KeyEvent.ACTION_DOWN) return super.onKeyDown(keyCode, event)

        // A key the settings window is capturing comes here first and nowhere
        // else: while an IME is the current input method, hardware keys are
        // dispatched to the IME window, so the activity on top of it never sees
        // them. Checked before the space handling and the four bindings, or a
        // chord being captured would also compose and fire its action.
        if (KeyCaptureSession.feed(KeyEvent.ACTION_DOWN, keyCode, event.metaState)) return true

        /* Modifier + 한/영 is the special-function NAMESPACE (ruled 2026-10-31):
         * the engine owns which chord means which function, this service
         * implements it or refuses. Placed above the four configurable actions
         * because the table is the engine's -- a stored KeyAction chord must not
         * pre-empt the ruled assignment -- and above the toggle because the
         * single press is what falls through here.
         *
         *  >0  performed, consume the key
         *  -1  bound but refused: NOT a toggle, and the key passes to the app
         *      (silently turning it into a toggle is how Shift+한/영 would start
         *      switching modes on a platform that cannot service the function)
         *   0  not a compound press: fall through to the toggle below */
        if (KeyBindings.isLanguageKey(keyBindings, keyCode)) {
            val chordEngine = if (isHangulMode) engine else latinEngine
            val mods = HunminKeyRegistry.engineMods(event.metaState)
            val shift = if (event.isShiftPressed) 1 else 0
            if (chordEngine != null && (mods != 0 || shift != 0)) {
                val fn = chordEngine.special(0, shift, mods)
                if (fn > 0) return true
                if (fn < 0) return super.onKeyDown(keyCode, event)
            }
        }

        // Debug logging for keycodes. metaState is printed raw because the
        // binding layer normalises it -- seeing the delivered bits is the point.
        Log.d(TAG, "onKeyDown: keyCode=$keyCode, metaState=${event.metaState}")

        // Space during composition: commit syllable AND forward space
        // This is a frontend responsibility (engine declines space during composition).
        // Not in a password editor: there is no composition to flush, and the
        // secure-editor guard below would return the key anyway -- handled here
        // first so the branch reads as one policy rather than two.
        if (keyCode == KeyEvent.KEYCODE_SPACE && isHangulMode && !isSecureEditor) {
            val e = engine ?: return super.onKeyDown(keyCode, event)
            val target: InputConnection = currentInputConnection
                ?: return super.onKeyDown(keyCode, event)
            // Space is a boundary key: the WORD accumulated so far is delivered
            // once through commitWord (issue #75), then the space itself is
            // forwarded -- the space does not go through the engine. Not in a
            // password editor: no composition, the guard below returns the key.
            e.commitWord(target)
            target.commitText(" ", 1)
            return true
        }

        // The four configurable triggers, in KeyAction's declared precedence
        // order. It is a loop, not four `if`s, because the order is also what the
        // conflict warning reports: if two actions end up sharing a chord, the
        // first one here is the one that keeps working, and saying otherwise
        // would be a lie told by two files that disagree.
        for (action in KeyAction.byPrecedence()) {
            /* AUTO_TOGGLE is LIVE again (issue #174): the 2026-10-12 ruling
             * (#2) skipped it because the handler was a log-only stub, and the
             * engine feature has since landed (#164/#165/#169), so the press now
             * flips the engine's own `autoswitch` value -- and is refused by the
             * engine, with the key still consumed, on an install without the
             * modes data. Its default chord stays unbound (that part of the
             * ruling is untouched): the ruled assignment is the engine's
             * Ctrl+한/영 chord, and a stored binding from an older build is an
             * extra surface on the same state, not a second assignment. */
            if (!fires(action, keyCode, event.metaState)) continue
            when (action) {
                KeyAction.HANGUL_ENG -> {
                    Log.i(TAG, "Hangul/English toggle triggered by keyCode=$keyCode")
                    toggleHangulEnglish()
                }
                KeyAction.VOICE -> startVoiceInput()
                KeyAction.AUTO_TOGGLE -> toggleAutoHangulEnglish()
                KeyAction.HANJA -> openHanjaWindow()
            }
            return true
        }

        // Modifier keys on their own (Alt, Ctrl, Meta/Super) must not reach the
        // engine. A physical keyboard delivers the modifier as its own
        // ACTION_DOWN before the chord key (Alt alone arrives first when the
        // user presses the Hanja chord's Shift). Feeding a bare modifier to the engine
        // makes HunminAdapter.onKey take its isModifierChord path and flush the
        // in-flight syllable -- which is exactly what left the Hanja lookup
        // with an empty preedit. Handled shortcuts above have already returned;
        // anything reaching here is an unhandled modifier, so pass it through
        // without touching the composition.
        if (keyCode == KeyEvent.KEYCODE_ALT_LEFT
            || keyCode == KeyEvent.KEYCODE_ALT_RIGHT
            || keyCode == KeyEvent.KEYCODE_CTRL_LEFT
            || keyCode == KeyEvent.KEYCODE_CTRL_RIGHT
            || keyCode == KeyEvent.KEYCODE_META_LEFT
            || keyCode == KeyEvent.KEYCODE_META_RIGHT) {
            return super.onKeyDown(keyCode, event)
        }

        // Route to correct engine based on mode.
        //
        // A password editor bypasses both engines entirely: every key falls
        // through to super.onKeyDown(), which is the platform's own
        // insertion path, so the field receives the literal character. An IME
        // composing Hangul into a masked field produces text the user cannot
        // read (measured 2026-10-13, issue #13). Checked before the engines
        // rather than inside them: HunminAdapter is layout-agnostic core code
        // with no notion of an editor, and the policy belongs to the adapter.
        if (isSecureEditor) return super.onKeyDown(keyCode, event)

        val e = if (isHangulMode) engine else latinEngine
        val target: InputConnection = currentInputConnection
            ?: return super.onKeyDown(keyCode, event)

        val consumed = try {
            e?.onKey(keyCode, event.metaState, target) ?: false
        } catch (ex: UnsatisfiedLinkError) {
            Log.e(TAG, "hunmin: native library unavailable", ex)
            engine = null
            latinEngine = null
            false
        }
        /* R8's automatic switch (ruled 2026-11-01, issue #164): the engine judged
         * the finished word and has already delivered it in the judged language,
         * so what is left is the MODE half -- and here the mode is which adapter
         * receives the next key, which only this service knows. Taken on every
         * key because the judgement fires on the boundary key, which is usually
         * one the engine declined (space). */
        if (e?.takeJudgement() == HUNMIN_LANGUAGE_LATIN && isHangulMode) {
            toggleHangulEnglish()
        }
        return if (consumed) true else super.onKeyDown(keyCode, event)
    }

    /**
     * Key *release*, fed to a waiting capture for one reason only: binding a bare
     * modifier (Right Alt on its own) can only be decided on the release, because
     * while the modifier is down the service cannot yet know whether a chord is
     * coming. Everything else passes straight through.
     */
    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (KeyCaptureSession.feed(KeyEvent.ACTION_UP, keyCode, event.metaState)) return true
        return super.onKeyUp(keyCode, event)
    }

    /** Toggle between Hangul and English (Latin) mode. */
    private fun toggleHangulEnglish() {
        /* The layer's action first (FE7): it commits the pending word at the
         * boundary, switches language and attaches the other slot's layout, in
         * the same place every frontend does it. Only a one-language adapter
         * falls through to the hand-rolled swap below. */
        val target0 = currentInputConnection
        val eng = engine
        if (eng != null && eng.toggleLanguage()) {
            isHangulMode = eng.isHangulLanguage()
            modeStore.remember(KEY_HANGUL_MODE, isHangulMode)
            if (target0 != null) eng.updatePreedit(target0)
            Log.i(TAG, "hunmin: language action by the layer -> ${if (isHangulMode) "한글" else "English"}")
            showModeToast(if (isHangulMode) "한글" else "English")
            picker?.setMicState(if (isHangulMode) VoiceState.IDLE else VoiceState.ERROR)
            picker?.setMode(isHangulMode)
            picker?.setActiveLayout(currentLayout())
            return
        }
        isHangulMode = !isHangulMode
        modeStore.remember(KEY_HANGUL_MODE, isHangulMode)
        val target = currentInputConnection ?: return

        if (isHangulMode) {
            // Switching to Hangul: commit the Latin word first (boundary)
            latinEngine?.commitWord(target)
            engine?.updatePreedit(target)
            Log.i(TAG, "hunmin: switched to Hangul mode ($layoutId)")
            showModeToast("한글")
        } else {
            // Switching to English: commit the Hangul word first (boundary)
            engine?.commitWord(target)
            latinEngine?.updatePreedit(target)
            Log.i(TAG, "hunmin: switched to English mode ($latinLayoutId)")
            showModeToast("English")
        }
        picker?.setMicState(
            if (isHangulMode) VoiceState.IDLE
            else VoiceState.ERROR
        )
        picker?.setMode(isHangulMode)
        // The name follows the mode: 한 shows the 한글 array, 영 the 영문 one.
        picker?.setActiveLayout(currentLayout())
    }

    /** Show transient mode indicator (like Samsung's toast). */
    private fun showModeToast(mode: String) {
        showIndicator("($mode)")
    }

    /**
     * The transient one-line indicator OverlayActivity renders.
     *
     * Separate from [showModeToast] because the parenthesised form is the MODE
     * indicator's convention; a sentence like "자동 한/영: 꺼짐 (사전 없음)"
     * already carries its own punctuation and would read as a nesting of two
     * conventions if it were wrapped.
     */
    private fun showIndicator(text: String) {
        // Use OverlayActivity for Toast since IME Service can't show Toast directly
        val intent = android.content.Intent(this, OverlayActivity::class.java).apply {
            action = OverlayActivity.ACTION_SHOW_TOAST
            putExtra(OverlayActivity.EXTRA_TOAST_TEXT, text)
            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        startActivity(intent)
    }

    /** Toggle the engine's automatic Hangul/English switch on/off. */
    private fun toggleAutoHangulEnglish() {
        /* R13-정정: the state is ONE value the engine owns. This binding reads
         * it, asks for the inverse, and then reports what the engine ANSWERED --
         * ON is refused when the modes data is absent (B3's gate, issue #169),
         * so echoing the request would show a state the judgement cannot use. */
        val current = autoswitchValue()
        if (current == null) {
            Log.w(TAG, "hunmin: autoswitch requested but no engine can answer")
            return
        }
        val wanted = current == 0
        val answer = applyAutoswitch(wanted)
        Log.i(TAG, "hunmin: autoswitch requested=$wanted engine=$answer")
        announceAutoswitch(answer, wanted)
    }

    /**
     * The engine's automatic-switch value, or null when no live layer can answer
     * (not started, or the engine has no such option).
     *
     * The pair layer is asked first because it is the one the judgement runs on;
     * `latinEngine` is the same engine seen one-language, and it answers the
     * same unless the pair is gone.
     */
    private fun autoswitchValue(): Int? =
        engine?.autoswitch() ?: latinEngine?.autoswitch()

    /**
     * Write [on] to every live layer and record what the ENGINE answered.
     *
     * Two things this is not allowed to be. It is not a per-frontend copy of the
     * value: what it stores is the read-back, so a gate refusal is stored as
     * OFF rather than as the request (R13-정정, issue #169). And it is not
     * editor-scoped: the layer is rebuilt per editor and its ic starts OFF, so
     * the platform's prefs are what carry the preference to the next editor,
     * re-applied in [onStartInput].
     *
     * @return the engine's answer (0/1), or null when no layer answered.
     */
    private fun applyAutoswitch(on: Boolean): Int? {
        var answer: Int? = null
        for (layer in listOfNotNull(engine, latinEngine)) {
            val v = layer.setAutoswitch(on)
            if (v != null) answer = v
        }
        if (answer != null) {
            getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putBoolean(KEY_AUTOSWITCH, answer == 1).apply()
        }
        return answer
    }

    /**
     * Re-apply the stored preference to freshly built layers.
     *
     * Called from [onStartInput], where both adapters are new and their layers
     * therefore start from the engine's default (OFF, B3). Without this the
     * feature would turn itself off on every field change -- the "saved, reloaded,
     * did nothing" defect class.
     */
    private fun restoreAutoswitch() {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        if (!prefs.getBoolean(KEY_AUTOSWITCH, false)) return
        val answer = applyAutoswitch(true) ?: return
        if (answer != 1) {
            Log.i(TAG, "hunmin: stored autoswitch=on refused by the engine; stored OFF")
        }
    }

    /** Tell the user what happened, in the same transient indicator the mode
     *  switches use: a state change with no feedback reads as a dead key. */
    private fun announceAutoswitch(answer: Int?, wanted: Boolean) {
        val text = when {
            answer == null -> "자동 한/영: 사용할 수 없음"
            answer == 1 -> "자동 한/영: 켜짐"
            wanted -> "자동 한/영: 꺼짐 (사전 없음)"
            else -> "자동 한/영: 꺼짐"
        }
        showIndicator(text)
    }

    /** Open Hanja candidate window for current preedit. */
    private fun openHanjaWindow() {
        val e = engine
        if (e == null) {
            Log.w(TAG, "hunmin: Hanja window requested but engine is null")
            return
        }
        // Commit any in-flight composition first
        val preedit = e.preedit()
        if (preedit.isEmpty()) {
            Log.i(TAG, "hunmin: no preedit for Hanja lookup")
            return
        }
        val count = e.suggestCount(preedit)
        if (count == 0) {
            Log.i(TAG, "hunmin: no Hanja candidates for '$preedit'")
            return
        }

        // Build candidate list. detail is the engine's "ko | en [tag]" gloss
        // (hunmin.hanja/2); the kind label (hanja/word/idiom) is dropped —
        // the gloss itself says what the candidate is, and a "hanja:" prefix
        // in front of an already-glossed candidate was pure noise.
        val candidates = mutableListOf<String>()
        val details = mutableListOf<String>()
        for (i in 0 until count) {
            val text = e.suggestText(i)
            val detail = e.suggestDetail(i)
            candidates.add(text)
            details.add(detail)
        }

        // Launch OverlayActivity to show dialog
        val intent = android.content.Intent(this, OverlayActivity::class.java).apply {
            action = OverlayActivity.ACTION_SHOW_HANJA
            putExtra(OverlayActivity.EXTRA_HANJA_PREEDIT, preedit)
            putExtra(OverlayActivity.EXTRA_HANJA_CANDIDATES, candidates.toTypedArray())
            putExtra(OverlayActivity.EXTRA_HANJA_DETAILS, details.toTypedArray())
            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        startActivity(intent)
    }

    /**
     * Toggle the tier E3 trace sink, from `tools/e3-trace.sh`.
     *
     * A broadcast is the only channel that reaches an IME from outside: its
     * service is bound by the platform and cannot be started with `am start`,
     * and it receives neither activity results nor permission results
     * (FRONTEND-CONTRACT.md B5), so there is no callback path. Guarded on the
     * app being debuggable, because a released IME must not accept a broadcast
     * that turns on file writing.
     *
     * @suppress Registered in the manifest, not here.
     */
    private inner class TraceReceiver : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action != ACTION_TRACE) return
            when (intent.getStringExtra("op")) {
                "enable" -> {
                    HunminTraceRecorder.caseLabel =
                        intent.getStringExtra("case") ?: "manual"
                    HunminTraceRecorder.source = intent.getStringExtra("source") ?: ""
                    HunminTraceRecorder.enable(
                        java.io.File(filesDir, TRACE_FILE),
                        layoutId,
                    )
                    Log.i(TAG, "hunmin: trace enabled (case=${HunminTraceRecorder.caseLabel})")
                }
                "disable" -> {
                    HunminTraceRecorder.close()
                    Log.i(TAG, "hunmin: trace closed")
                }
            }
        }
    }

    private var traceReceiver: TraceReceiver? = null

    /**
     * Receive the Hanja picked in OverlayActivity and commit it.
     *
     * OverlayActivity is started with startActivity (not startActivityForResult)
     * and an IME is a Service, not an Activity, so it can never receive an
     * activity result. The dialog therefore broadcasts the choice back; this
     * receiver commits it through [HunminAdapter.commitHanja], which flushes the
     * composing Hangul first so the Hanja REPLACES it. commitExternalText would
     * append after it ("한恨"), because that path is invariant 5's voice
     * pass-through, not a conversion. Explicit-package broadcast, so only this
     * app hears it.
     */
    private val hanjaReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action != OverlayActivity.ACTION_HANJA_SELECTED) return
            val selected = intent.getStringExtra(OverlayActivity.EXTRA_SELECTED_HANJA)
            if (selected.isNullOrEmpty()) return
            val target = currentInputConnection
            val ok = if (target != null) {
                engine?.commitHanja(target, selected)
                true
            } else {
                false
            }
            Log.i(TAG, "hunmin: Hanja selected '$selected' committed=$ok")
        }
    }

    private fun registerHanjaReceiver() {
        @Suppress("DEPRECATION")
        if (android.os.Build.VERSION.SDK_INT >= 33) {
            registerReceiver(hanjaReceiver, IntentFilter(OverlayActivity.ACTION_HANJA_SELECTED),
                             Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(hanjaReceiver, IntentFilter(OverlayActivity.ACTION_HANJA_SELECTED))
        }
    }

    /**
     * Register the receiver only on a debuggable build.
     *
     * `applicationInfo.flags and FLAG_DEBUGGABLE` is the check, not a build
     * variant name: a release APK must not accept a broadcast that turns on
     * file writing, and that has to hold for any packaging, not just the one
     * Gradle calls "release".
     */
    private fun registerTraceReceiver() {
        val debuggable =
            (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
        if (!debuggable) return
        val r = TraceReceiver()
        // The platform overload changed at API 33 (a RECEIVER_EXPORTED flag was
        // added and the old one deprecated), so both are called by version
        // rather than through a compat shim: this module has no AndroidX
        // dependency and adding one for a single call would be worse than four
        // lines here. minSdk is 24, so the guard is genuinely needed.
        @Suppress("DEPRECATION")
        if (android.os.Build.VERSION.SDK_INT >= 33) {
            registerReceiver(r, IntentFilter(ACTION_TRACE),
                             Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(r, IntentFilter(ACTION_TRACE))
        }
        traceReceiver = r
    }

    /**
     * Ask for speech, then commit whatever comes back verbatim.
     *
     * The whole point of routing this through [HunminAdapter.commitExternalText]
     * and never through `onKey` is invariant 5: spoken Hangul fed to
     * hunmin_ic_process would be composed a second time. Every failure below
     * therefore leaves the IME typing and reports itself, because a voice
     * attempt that throws into the key path takes the whole keyboard with it.
     */
    fun startVoiceInput() {
        Log.i(TAG, "voice: startVoiceInput (hangul=$isHangulMode, inFlight=${voiceSession != null})")
        if (voiceSession != null) {
            // Already listening; a second tap cancels, which is what a mic
            // button that toggles is expected to do.
            voiceSession?.stop()
            voiceSession = null
            picker?.setMicState(VoiceState.IDLE)
            return
        }
        // Determine language based on current mode
        val language = if (isHangulMode) "ko-KR" else "en-US"
        val session = VoiceSession(
            this,
            recognizerFactory = { PlatformRecognizer(this, language) },
            permissionGate = AndroidPermissionGate(this)
        )
        // Show listening state (blue)
        picker?.setMicState(VoiceState.LISTENING)
        session.onResult = { result ->
            voiceSession = null
            picker?.setMicState(VoiceState.IDLE)
            when (result) {
                is VoiceResult.Got -> {
                    // Verify the service is still on the same editor before
                    // committing: the user can switch fields mid-utterance, and
                    // committing into the wrong one is worse than dropping it.
                    if (isCurrentEditor(currentInputEditorInfo)) {
                        commitExternalText(result.text)
                    } else {
                        Log.i(TAG, "hunmin: the editor changed during dictation; dropped")
                    }
                }
                is VoiceResult.Failed -> Log.w(TAG, "hunmin: voice failed (${result.reason})")
                VoiceResult.NothingHeard -> Log.i(TAG, "hunmin: nothing heard")

                // The prompt is up and nothing else happened. Not a failure and
                // not [PermissionDenied]: no answer exists yet, and the IME
                // cannot receive one (B5). Tapping the mic again after the
                // dialog closes re-reads checkSelfPermission and either starts
                // the recogniser or reports the denial.
                VoiceResult.PermissionAsked ->
                    Log.i(TAG, "hunmin: asked for RECORD_AUDIO; tap the mic again " +
                        "once the dialog is done")

                // Not granted, and nobody asked or the user said no. A real
                // outcome, not an error to retry: the keyboard keeps typing and
                // says where the switch is, because on this path there is no
                // settings surface of ours to send the user to.
                VoiceResult.PermissionDenied ->
                    Log.w(TAG, "hunmin: RECORD_AUDIO not granted; grant it in " +
                        "Settings > Apps > Default keyboard apps. The keyboard " +
                        "itself is unaffected.")

                VoiceResult.Unavailable ->
                    Log.w(TAG, "hunmin: no speech path on this device; IME unaffected")
            }
        }
        voiceSession = session
        session.start()
    }

    /**
     * False when [editor] is not the one this service is currently bound to.
     *
     * `currentInputEditorInfo` is null while no field is focused, and that is
     * also "not the editor we started with".
     */
    private fun isCurrentEditor(editor: android.view.inputmethod.EditorInfo?): Boolean =
        editor != null && editor.fieldId == voiceFieldId

    /**
     * Invariant 5's front door: text from outside the keyboard.
     *
     * The voice launcher (P5's remaining half) calls this with recognised text
     * and never touches hunmin_ic_process. Kept as a public method rather than
     * inlined into a future RecognizerIntent handler so tier E1 can drive it
     * with a mocked InputConnection and no recogniser at all -- recognition is
     * an external service and is deliberately untested, but the *path* into the
     * engine is ours and is testable.
     */
    fun commitExternalText(text: String): Boolean {
        val e = engine ?: return false
        val target = currentInputConnection ?: return false
        e.commitExternalText(target, text)
        return true
    }

    /**
     * The IME's only on-screen surface: the active layout, tappable to change it.
     *
     * This is not decoration. A hardware-key IME still needs an input view,
     * because key events reach an IME through the IME window and a service that
     * shows no view has nowhere for them to arrive (FRONTEND-CONTRACT.md B1) --
     * verified on the emulator, where one `input keyevent KEYCODE_K` alone puts
     * ㄱ in the field. The picker now occupies that view; a soft keyboard would
     * replace its body, not its existence.
     */
    override fun onCreateInputView(): View {
        val view = LayoutPicker(this)
        view.applyStyle(toolbarStyle())
        view.setLayouts(LayoutRegistry.sorted(availableLayouts()))
        view.setActiveLayout(currentLayout())
        // The indicator opens the settings window AT the layout section rather
        // than showing a dropdown: choosing a layout is a setting, and a second
        // place to choose it would be a second source of truth for which one is
        // active.
        view.onLayoutClicked = { OverlayActivity.startSettings(this, OverlayActivity.SECTION_LAYOUT_HANGUL) }
        view.onSettingsClicked = { OverlayActivity.startSettings(this) }
        view.onMicClicked = { startVoiceInput() }
        view.onHanjaClicked = { openHanjaWindow() }
        view.onModeClicked = { toggleHangulEnglish() }
        // Assigned last: `picker` is a mutable property, so Kotlin cannot smart-
        // cast it for the bind() call above. Binding before the field is set also
        // means the picker's first onItemSelected -- which Spinner fires as soon as
        // it has an adapter -- cannot reach a half-built service.
        picker = view
        return view
    }

    override fun onCreateExtractTextView(): View? = null

    override fun onEvaluateInputViewShown(): Boolean = true

    override fun onEvaluateFullscreenMode(): Boolean = false

    /**
     * Every layout this build can actually open, as (id, Korean label) pairs.
     *
     * Mirrors fcitx5's `refreshLayoutList()` exactly, and the two filters matter
     * for the same reason in both: an unsupported engine kind would be a row in
     * the list that fails at keystroke time, which is worse than not offering
     * it. `hunmin_ctx_layout_supported` is the authority for that, not our own
     * knowledge of which engines exist.
     *
     * The Korean name is the label because every layout carries one in its JSON
     * (SPEC-SCHEMA.md) and `hunmin-doctor` warns when it is missing; the id is
     * the fallback so a layout with no name is still selectable rather than
     * blank.
     */
    fun availableLayouts(): List<LayoutEntry> {
        if (ctx == 0L) return emptyList()
        val out = mutableListOf<LayoutEntry>()
        val n = HunminNativeBridge.nativeLayoutCount(ctx)
        for (i in 0 until n) {
            if (!HunminNativeBridge.nativeLayoutSupported(ctx, i)) continue
            val id = HunminNativeBridge.nativeLayoutId(ctx, i)
            if (id.isEmpty()) continue
            val ko = HunminNativeBridge.nativeLayoutName(ctx, i, "ko")
            val mode = LayoutMode.of(HunminNativeBridge.nativeLayoutMode(ctx, i))
            out.add(LayoutEntry(id, ko.ifEmpty { id }, mode))
        }
        return out
    }

    /**
     * Switch layouts, keeping any composition out of the new engine.
     *
     * The in-flight composition is FLUSHED, not dropped: the user typed it and
     * it should reach the app. This is the same call the fcitx5 addon's
     * `ensureIc` makes when the layout changes mid-composition.
     */
    /**
     * Switch one half of the IME to [id].
     *
     * The other half is untouched: switching the 한글 layout while in English
     * mode must not silently change what English mode composes with. The engine
     * that is not being switched is left alone even when it is the live one.
     */
    fun setLayout(mode: LayoutMode, id: String) {
        val current = if (mode == LayoutMode.LATIN) latinEngine else engine
        if (current == null) {
            if (mode == LayoutMode.LATIN) latinLayoutId = id else layoutId = id
            return
        }
        currentInputConnection?.let { current.commitWord(it) }
        current.close()
        val next = HunminAdapter(ctx, id, commitBehavior)
        if (!next.isUsable) {
            // Refuse rather than leave the IME with no engine: the user would
            // get silence instead of the layout they asked for.
            Log.e(TAG, "hunmin: refusing to switch to unsupported layout '$id'")
            next.close()
            return
        }
        if (mode == LayoutMode.LATIN) {
            latinLayoutId = id
            latinEngine = next
        } else {
            layoutId = id
            engine = next
        }
        persistLayout(mode, id)
        refreshStrip()
        // Only the engine that is composing right now needs the new preedit.
        if (!isHangulMode == (mode == LayoutMode.LATIN)) {
            currentInputConnection?.let { next.updatePreedit(it) }
        }
    }

    /** The active layout id, for the picker UI and for tier E assertions. */
    /**
     * Let the settings window read and write what only this service knows.
     *
     * The layout list comes from the engine's context, which lives here and
     * nowhere else; the strip style is stored here so the strip and the window
     * cannot read different values. One writer, one notification path.
     */
    private fun installSettingsBus() {
        ImeSettings.layoutsProvider = {
            if (ctx == 0L) null else LayoutRegistry.sorted(availableLayouts())
        }
        ImeSettings.layoutChoice = { mode ->
            when (mode) {
                LayoutMode.LATIN -> if (ctx != 0L) latinLayoutId else null
                LayoutMode.HANGUL -> if (ctx != 0L) layoutId else null
            }
        }
        ImeSettings.onLayoutChosen = { mode, id -> setLayout(mode, id) }
        ImeSettings.toolbarStyle = { readToolbarStyle() }
        ImeSettings.onToolbarStyleChosen = { style ->
            runCatching {
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                    .putString(KEY_TOOLBAR_STYLE, style.key).apply()
            }.onFailure { Log.w(TAG, "hunmin: could not persist the input view style", it) }
            // Re-render the strip now: a style the user just picked should not
            // need a field change to appear.
            picker?.applyStyle(style)
        }
        /* R13-정정's surface B: the settings window reads the value and the key
         * from the engine, and writes through the same option the chord writes.
         * Every accessor answers null when no layer is live, which the window
         * renders as "not running" rather than as OFF. */
        ImeSettings.autoSwitchValue = { autoswitchValue() }
        ImeSettings.autoSwitchChord = {
            (engine ?: latinEngine)?.specialChord(SpecialFunction.AUTOSWITCH)
        }
        ImeSettings.autoSwitchDataPresent = {
            (engine ?: latinEngine)?.autoswitchDataPresent() ?: false
        }
        ImeSettings.onAutoSwitchChosen = { on -> applyAutoswitch(on) }
    }

    /** The strip style in force, defaulting to the compact one row. */
    fun toolbarStyle(): ToolbarStyle =
        ToolbarStyle.of(readToolbarStyleRaw())

    private fun readToolbarStyleRaw(): String? = try {
        getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_TOOLBAR_STYLE, null)
    } catch (e: Exception) {
        null
    }

    private fun readToolbarStyle(): ToolbarStyle = ToolbarStyle.of(readToolbarStyleRaw())

    /** The layout the IME is composing with RIGHT NOW, whichever mode that is. */
    fun currentLayout(): String = if (isHangulMode) layoutId else latinLayoutId

    /** The 한글 layout, whether or not it is the one composing at this moment. */
    fun hangulLayoutId(): String = layoutId

    /** The 영문 layout. */
    fun latinLayoutId(): String = latinLayoutId

    /**
     * Remember the chosen layout across process death.
     *
     * Without this the IME silently returns to sin3-p2 every time the service is
     * recreated, which on Android is frequent -- it is killed whenever the
     * process is trimmed. `apply()` rather than `commit()` because the IME is
     * killed without warning and a synchronous disk write on the UI thread is
     * not worth the durability.
     */
    private fun persistLayout(mode: LayoutMode, id: String) {
        val key = if (mode == LayoutMode.LATIN) KEY_LAYOUT_LATIN else KEY_LAYOUT_HANGUL
        try {
            getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putString(key, id).apply()
        } catch (e: Exception) {
            // A preference that cannot be written is a lost preference, not a
            // broken IME. Losing it means falling back to the default, which is
            // exactly what pickLayout() already handles.
            Log.w(TAG, "hunmin: could not persist the layout choice", e)
        }
    }

    /**
     * The stored layout for [mode], or null when there is nothing usable.
     *
     * The 한글 row falls back to the pre-two-mode `layout` key once, so an
     * upgrade keeps the layout the device already had instead of silently
     * reverting to the default. That old key is never written again.
     */
    private fun readStoredLayout(mode: LayoutMode): String? = try {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        if (mode == LayoutMode.LATIN) {
            prefs.getString(KEY_LAYOUT_LATIN, null)
        } else {
            prefs.getString(KEY_LAYOUT_HANGUL, null)
                ?: prefs.getString(KEY_LAYOUT, null)
        }
    } catch (e: Exception) {
        null
    }

    

    /**
     * The preference document note that used to sit here: a stored layout this
     * build cannot run must degrade rather than brick the IME, because "I
     * downgraded and now I can't type" is the worst failure this code can
     * produce. That is LayoutRegistry.resolve()'s fallback chain now, and it has
     * tier E1 tests where this loop had none.
     */
}
