/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminInputMethodService — the Android IME entry point.
 *
 * Lifecycle, per docs/TESTING.md tier E:
 *
 *   onCreate      extract the tables, build the ctx, remember the active layout
 *   onStartInput  build a fresh HunminEngine for THIS editor  (two editors must
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

    private companion object {
        const val TAG = "HunminIme"

        /**
         * The layout to start in. Matches the fcitx5 addon's default so the two
         * frontends agree on what an unconfigured install types.
         */
        const val DEFAULT_LAYOUT = "sin3-p2"

        /** Broadcast action that toggles the tier E3 trace sink. */
        const val ACTION_TRACE = "org.hunmin.ime.TRACE"

        /** Where the trace is written, inside filesDir so `run-as` can read it. */
        const val TRACE_FILE = "e3.trace"

        private const val PREFS = "hunmin"
        private const val KEY_LAYOUT = "layout"
    }

    /** One engine per InputConnection; rebuilt on every onStartInput. */
    private var engine: HunminEngine? = null

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

    /** Hangul/English mode. True = Hangul, False = English (Latin). */
    private var isHangulMode: Boolean = true

    /** Latin layout engine for English mode. */
    private var latinEngine: HunminEngine? = null

    override fun onCreate() {
        super.onCreate()
        registerTraceReceiver()
        registerHanjaReceiver()
        val dataDir = HunminData.install(this)
        if (dataDir == null) {
            // Without tables there is no layout and no Korean at all. Failing
            // here (rather than per keystroke) keeps the symptom one log line
            // instead of a stream of silent no-ops.
            Log.e(TAG, "hunmin: no layout tables extracted; IME is inert")
            return
        }
        ctx = HunminNative.nativeCtxNew(dataDir.absolutePath)
        if (ctx == 0L) {
            Log.e(TAG, "hunmin: hunmin_ctx_new found no layout in ${dataDir.absolutePath}")
            return
        }
        // The stored choice wins over the default, but pickLayout() validates
        // it: a preference naming a layout this build cannot run must degrade to
        // the default rather than leave the IME with no engine.
        layoutId = pickLayout(readStoredLayout() ?: DEFAULT_LAYOUT)
        Log.i(TAG, "hunmin: ${HunminNative.nativeLayoutCount(ctx)} layouts, using $layoutId")
    }

    override fun onDestroy() {
        // Close the trace before anything else: a sink left open past onDestroy
        // would keep appending to a file nothing can pull, because the service
        // that could answer the pull is gone.
        HunminTrace.close()
        traceReceiver?.let { runCatching { unregisterReceiver(it) } }
        traceReceiver = null
        runCatching { unregisterReceiver(hanjaReceiver) }
        engine?.close()
        engine = null
        if (ctx != 0L) {
            HunminNative.nativeCtxFree(ctx)
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
        engine = if (ctx == 0L) null else HunminEngine(ctx, layoutId)
        latinEngine = if (ctx == 0L) null else HunminEngine(ctx, "us-qwerty")
        if (engine?.isUsable != true) {
            Log.e(TAG, "hunmin: layout '$layoutId' unsupported; keys will pass through")
        }
        // Start in Hangul mode by default
        isHangulMode = true
        currentInputConnection?.let { engine?.updatePreedit(it) }
        // The view is created once per IME window but the engine per editor, so
        // the picker's list and selection are refreshed here rather than trusted
        // to still match.
        picker?.bind(availableLayouts(), layoutId) { id -> setLayout(id) }
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
    private fun syncPicker() {
        picker?.select(layoutId)
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

        val altRight = (event.metaState and KeyEvent.META_ALT_RIGHT_ON) != 0
        val altLeft = (event.metaState and KeyEvent.META_ALT_LEFT_ON) != 0
        val shiftOn = (event.metaState and KeyEvent.META_SHIFT_ON) != 0

        // Debug logging for keycodes
        Log.d(TAG, "onKeyDown: keyCode=$keyCode, metaState=${event.metaState}, altRight=$altRight, altLeft=$altLeft, shiftOn=$shiftOn")

        // Space during composition: commit syllable AND forward space
        // This is a frontend responsibility (engine declines space during composition)
        if (keyCode == KeyEvent.KEYCODE_SPACE && isHangulMode) {
            val e = engine ?: return super.onKeyDown(keyCode, event)
            val target: InputConnection = currentInputConnection
                ?: return super.onKeyDown(keyCode, event)
            // Flush/commit the in-flight syllable first
            val committed = e.flush(target)
            // Then forward the space to the application
            target.commitText(" ", 1)
            return true
        }

        // Hangul/English toggle: Left Alt + Z, Right Alt, or Language Switch key
        if (keyCode == KeyEvent.KEYCODE_Z && altLeft) {
            toggleHangulEnglish()
            return true
        }
        // Hangul/English toggle: Right Alt, Language Switch, or Samsung 한/영 key (keyCode 218)
        if (keyCode == KeyEvent.KEYCODE_ALT_RIGHT
            || keyCode == KeyEvent.KEYCODE_LANGUAGE_SWITCH
            || keyCode == 218) {  // Samsung 한/영 key
            Log.i(TAG, "Hangul/English toggle triggered by keyCode=$keyCode")
            toggleHangulEnglish()
            return true
        }

        // Voice input trigger: Right Alt + V or Left Alt + V
        if (keyCode == KeyEvent.KEYCODE_V && (altRight || altLeft)) {
            startVoiceInput()
            return true
        }

        // Hangul/English auto-toggle on/off: Right Alt + Enter
        if (keyCode == KeyEvent.KEYCODE_ENTER && altRight) {
            toggleAutoHangulEnglish()
            return true
        }

        // Hanja window: Left Alt + X, or Shift + Right Alt, or Shift + Language Switch
        if (keyCode == KeyEvent.KEYCODE_X && altLeft) {
            openHanjaWindow()
            return true
        }
        if (keyCode == KeyEvent.KEYCODE_ALT_RIGHT && shiftOn) {
            openHanjaWindow()
            return true
        }
        if (keyCode == KeyEvent.KEYCODE_LANGUAGE_SWITCH && shiftOn) {
            openHanjaWindow()
            return true
        }

        // Modifier keys on their own (Alt, Ctrl, Meta/Super) must not reach the
        // engine. A physical keyboard delivers the modifier as its own
        // ACTION_DOWN before the chord key (Alt alone arrives first when the
        // user presses Alt+X for Hanja). Feeding a bare modifier to the engine
        // makes HunminEngine.onKey take its isModifierChord path and flush the
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

        // Route to correct engine based on mode
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
        return if (consumed) true else super.onKeyDown(keyCode, event)
    }

    /** Toggle between Hangul and English (Latin) mode. */
    private fun toggleHangulEnglish() {
        isHangulMode = !isHangulMode
        val target = currentInputConnection ?: return

        if (isHangulMode) {
            // Switching to Hangul: flush any Latin composition first
            latinEngine?.flush(target)
            engine?.updatePreedit(target)
            Log.i(TAG, "hunmin: switched to Hangul mode ($layoutId)")
            showModeToast("한글")
        } else {
            // Switching to English: flush Hangul composition first
            engine?.flush(target)
            latinEngine?.updatePreedit(target)
            Log.i(TAG, "hunmin: switched to English mode (us-qwerty)")
            showModeToast("English")
        }
        picker?.setMicState(
            if (isHangulMode) LayoutPicker.MicState.IDLE
            else LayoutPicker.MicState.ERROR
        )
        picker?.setMode(isHangulMode)
    }

    /** Show transient mode indicator (like Samsung's toast). */
    private fun showModeToast(mode: String) {
        // Use OverlayActivity for Toast since IME Service can't show Toast directly
        val intent = android.content.Intent(this, OverlayActivity::class.java).apply {
            action = OverlayActivity.ACTION_SHOW_TOAST
            putExtra(OverlayActivity.EXTRA_TOAST_TEXT, "($mode)")
            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        startActivity(intent)
    }

    /** Toggle auto Hangul/English detection on/off. */
    private fun toggleAutoHangulEnglish() {
        // TODO: persist this setting
        // For now, just log. The actual auto-detection logic is in the engine/fcitx5 adapter.
        Log.i(TAG, "hunmin: auto Hangul/English toggle requested (not yet implemented)")
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
                    HunminTrace.caseLabel =
                        intent.getStringExtra("case") ?: "manual"
                    HunminTrace.source = intent.getStringExtra("source") ?: ""
                    HunminTrace.enable(
                        java.io.File(filesDir, TRACE_FILE),
                        layoutId,
                    )
                    Log.i(TAG, "hunmin: trace enabled (case=${HunminTrace.caseLabel})")
                }
                "disable" -> {
                    HunminTrace.close()
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
     * receiver commits it through commitExternalText so the selected Hanja
     * replaces the Hangul composition. Explicit-package broadcast, so only
     * this app hears it.
     */
    private val hanjaReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action != OverlayActivity.ACTION_HANJA_SELECTED) return
            val selected = intent.getStringExtra(OverlayActivity.EXTRA_SELECTED_HANJA)
            if (selected.isNullOrEmpty()) return
            val ok = commitExternalText(selected)
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
     * The whole point of routing this through [HunminEngine.commitExternalText]
     * and never through `onKey` is invariant 5: spoken Hangul fed to
     * hunmin_ic_process would be composed a second time. Every failure below
     * therefore leaves the IME typing and reports itself, because a voice
     * attempt that throws into the key path takes the whole keyboard with it.
     */
    fun startVoiceInput() {
        if (voiceSession != null) {
            // Already listening; a second tap cancels, which is what a mic
            // button that toggles is expected to do.
            voiceSession?.stop()
            voiceSession = null
            picker?.setMicState(LayoutPicker.MicState.IDLE)
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
        picker?.setMicState(LayoutPicker.MicState.LISTENING)
        session.onResult = { result ->
            voiceSession = null
            picker?.setMicState(LayoutPicker.MicState.IDLE)
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
        view.bind(availableLayouts(), layoutId) { id -> setLayout(id) }
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
        val n = HunminNative.nativeLayoutCount(ctx)
        for (i in 0 until n) {
            if (!HunminNative.nativeLayoutSupported(ctx, i)) continue
            val id = HunminNative.nativeLayoutId(ctx, i)
            if (id.isEmpty()) continue
            val ko = HunminNative.nativeLayoutName(ctx, i, "ko")
            out.add(LayoutEntry(id, ko.ifEmpty { id }))
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
    fun setLayout(id: String) {
        val e = engine
        if (e == null) { layoutId = id; return }
        currentInputConnection?.let { e.flush(it) }
        e.close()
        val next = HunminEngine(ctx, id)
        if (!next.isUsable) {
            // Refuse rather than leave the IME with no engine: the user would
            // get silence instead of the layout they asked for.
            Log.e(TAG, "hunmin: refusing to switch to unsupported layout '$id'")
            next.close()
            return
        }
        layoutId = id
        engine = next
        persistLayout(id)
        syncPicker()
        currentInputConnection?.let { next.updatePreedit(it) }
    }

    /** The active layout id, for the picker UI and for tier E assertions. */
    fun currentLayout(): String = layoutId

    /**
     * Remember the chosen layout across process death.
     *
     * Without this the IME silently returns to sin3-p2 every time the service is
     * recreated, which on Android is frequent -- it is killed whenever the
     * process is trimmed. `apply()` rather than `commit()` because the IME is
     * killed without warning and a synchronous disk write on the UI thread is
     * not worth the durability.
     */
    private fun persistLayout(id: String) {
        try {
            getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putString(KEY_LAYOUT, id).apply()
        } catch (e: Exception) {
            // A preference that cannot be written is a lost preference, not a
            // broken IME. Losing it means falling back to the default, which is
            // exactly what pickLayout() already handles.
            Log.w(TAG, "hunmin: could not persist the layout choice", e)
        }
    }

    private fun readStoredLayout(): String? = try {
        getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_LAYOUT, null)
    } catch (e: Exception) {
        null
    }

    

    /**
     * Use [preferred] if this build can instantiate it, else the default, else
     * the first layout the engine does support.
     *
     * A stored preference pointing at a layout this build cannot run must
     * degrade rather than brick the IME, because "I downgraded and now I can't
     * type" is the worst failure this code can produce.
     */
    private fun pickLayout(preferred: String): String {
        val n = HunminNative.nativeLayoutCount(ctx)
        for (i in 0 until n) {
            val id = HunminNative.nativeLayoutId(ctx, i)
            if (id == preferred && HunminNative.nativeLayoutSupported(ctx, i)) return id
        }
        // Then the documented default, and only then anything at all: a stored
        // preference for a layout this build cannot open should land on the
        // layout the user is documented to get, not on whichever sorts first.
        for (i in 0 until n) {
            val id = HunminNative.nativeLayoutId(ctx, i)
            if (id == DEFAULT_LAYOUT && HunminNative.nativeLayoutSupported(ctx, i)) return id
        }
        for (i in 0 until n) {
            val id = HunminNative.nativeLayoutId(ctx, i)
            if (HunminNative.nativeLayoutSupported(ctx, i)) return id
        }
        return preferred
    }
}


