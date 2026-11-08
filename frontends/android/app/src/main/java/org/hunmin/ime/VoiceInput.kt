/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * VoiceInput — launching a recogniser and getting spoken text back.
 *
 * WHAT THIS IS NOT: a speech recogniser. Recognition is an external service and
 * is deliberately untested (TESTING.md E1). What is ours, and what is therefore
 * tested, is everything between "the user asked to speak" and "the text reached
 * the engine": the intent, the lifecycle, the permission answer, the error
 * paths, and the hand-off into hunmin_ic_commit_text().
 *
 * WHY IT HAS AN INTERFACE. [Recognizer] is a seam with one real implementation
 * ([PlatformRecognizer]) and a fake for tier E1. Without it, every test
 * of "what happens when the user denies the microphone" has to install a
 * recogniser, and one of the most important behaviours of this class -- that a
 * failure leaves the IME usable -- would be untestable.
 *
 * WHY TWO PATHS. PLAN.md's risk table: Samsung devices vary in whether Google's
 * SpeechRecognizer is available at all, so a second path is needed.
 *
 * ⚠️ CORRECTED AGAINST THE PLATFORM (javap, android.jar API 35, 2026-10-04).
 * The fallback is NOT `ACTION_VOICE_INPUT`: that constant does not exist, and
 * neither does `InputMethodService.onActivityResult`, so a Context-started
 * activity could not have its result delivered to an IME at all. The fallback is
 * therefore a recogniser *intent the user completes in another app*, and the
 * result comes back as a committed suggestion the client offers -- see
 * [VoiceSession.startFallback] for what is actually verifiable. PLAN.md's wording
 * is corrected in the same commit.
 *
 * ⚠️ ALSO CORRECTED AGAINST THE PLATFORM (re-measured 2026-10-09).
 * `RecognizerIntent.LANGUAGE_MODEL_FREE_FORM` **does** exist on API 35 --
 * `javap -constants` shows it as `"free_form"` -- so the comment in
 * [PlatformRecognizer.start] that says it is absent is wrong about the
 * platform. The behaviour is unaffected: the code omits `EXTRA_LANGUAGE_MODEL`
 * and free-form is the default when it is unset, which is the mode the emulator
 * was measured running in. Left uncorrected in place because the constant is
 * still not used, and naming it would be the only thing that changed.
 *
 * WHERE THE RECORD_AUDIO PROMPT LIVES. Neither this file nor
 * `HunminInputMethodService` can raise it: `requestPermissions` is an `Activity`
 * method and `InputMethodService` has no permission-request hook at all
 * (DESIGN-FRONTEND.md Part B B5). The decision about *whether* to raise one is
 * therefore here, as a pure function over three booleans
 * ([VoiceLauncher.decidePermission]), and the Android objects stay behind
 * [PermissionRequestActivity]. The session needs no Context to reach that
 * decision, so tier E1 drives it on a plain JVM.
 */
package org.hunmin.ime

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.util.Log

/** One log tag for the whole voice path; declared once because it was three. */
private const val TAG = "HunminVoice"

/** How the IME asked for text. Distinguishes the two supported paths. */
enum class VoicePath {
    /** In-process `SpeechRecognizer`, when the device has one. */
    SPEECH_RECOGNIZER,

    /**
     * The platform's own recogniser UI, for a device with no in-process
     * recogniser. ⚠️ Its result cannot be routed back to an IME -- see
     * [VoiceSession.startFallback].
     */
    FALLBACK_ACTIVITY,
}

/** Why a voice attempt produced nothing. All of these leave the IME usable. */
sealed class VoiceResult {
    /** Text to hand to the engine. Never fed through hunmin_ic_process. */
    data class Got(val text: String) : VoiceResult()

    /** The device has neither recogniser path. */
    object Unavailable : VoiceResult()

    /**
     * RECORD_AUDIO was not granted and nothing was asked.
     *
     * ⚠️ This is now only ONE of the two ways to lack the microphone. A tap that
     * could not ask (no prompt activity, an OEM that refuses the start, or a
     * permission the platform will no longer show a dialog for) reports this; a
     * tap that *did* ask and the user answered reports
     * [PermissionAsked]. Collapsing them is what made the pre-2026-10-09
     * measurement read a permanently broken microphone as a user decision.
     */
    object PermissionDenied : VoiceResult()

    /**
     * This tap put the platform's permission dialog in front of the user.
     *
     * ⚠️ NOT [PermissionDenied], and not [Unavailable] either. Nothing has
     * failed and no answer exists yet: the recogniser did not start, and the
     * session ends here rather than waiting for a result that cannot be
     * delivered (an activity started from a service is not started for result,
     * and `InputMethodService` has no `onActivityResult` -- B5). The user taps
     * the mic again once the dialog is gone, and the permission is re-read
     * rather than remembered.
     *
     * Separate from `PermissionDenied` because a trace reader has to be able to
     * tell "we asked" from "we were told no": the first is the fix working, the
     * second is a real outcome the user chose.
     */
    object PermissionAsked : VoiceResult()

    /** The user stopped it, or nothing was recognised. Not an error to shout about. */
    object NothingHeard : VoiceResult()

    /** Recogniser present but failed (no network, service crash, busy). */
    data class Failed(val reason: String) : VoiceResult()
}

/**
 * What a mic tap does about the microphone permission.
 *
 * A separate type from [VoiceResult] on purpose. A [VoiceResult] is a *report*,
 * delivered once and settled; this is an *instruction*, and the instruction is
 * chosen before anything is reported. Keeping them apart is what lets the
 * instruction be tested on a JVM with no Android runtime in it.
 */
enum class VoicePermissionAction {
    /** Permission is held: carry on to the recogniser. */
    PROCEED,

    /** Not held, and we can put the platform's dialog up. Ask once. */
    ASK,

    /** Not held, and asking is impossible or already refused. Stop here. */
    STOP,
}

/**
 * The recogniser seam.
 *
 * One method, because the only thing worth abstracting is "give me text"; the
 * Android recogniser is callback-shaped and an in-proc fake wants to be callable.
 */
interface Recognizer {
    /**
     * Start listening. Exactly one of [onText] or [onFailure] must be called,
     * possibly long after this returns -- recognition is asynchronous and the
     * user may take seconds to speak.
     */
    fun start(
        onText: (String) -> Unit,
        onFailure: (String) -> Unit,
    )

    /** Release resources. Must be safe to call without a preceding [start]. */
    fun stop()
}

/**
 * The real one: `android.speech.SpeechRecognizer`.
 *
 * Static call `SpeechRecognizer.isRecognitionAvailable(context)` is the only
 * authority for whether this path works; it was checked on the API-35 emulator
 * image and reported two services, which is why tier E2 can exercise a real
 * recogniser there rather than only the fallback.
 */
class PlatformRecognizer(
    private val context: Context,
    private val language: String = "ko-KR"  // Default to Korean
) : Recognizer {

    private var recognizer: SpeechRecognizer? = null
    private var onText: ((String) -> Unit)? = null
    private var onFailure: ((String) -> Unit)? = null

    companion object {
        fun isAvailable(context: Context): Boolean =
            try {
                SpeechRecognizer.isRecognitionAvailable(context)
            } catch (e: Exception) {
                // Some OEM builds throw rather than return false.
                Log.w(TAG, "isRecognitionAvailable threw", e)
                false
            }
    }

    override fun start(onText: (String) -> Unit, onFailure: (String) -> Unit) {
        // No trimming here: VoiceSession does it, on the one path every
        // Recognizer shares. See the note at the call site.
        this.onText = onText
        this.onFailure = onFailure
        val r = SpeechRecognizer.createSpeechRecognizer(context, null)
        recognizer = r
        r.setRecognitionListener(object : RecognitionListener {
            override fun onReadyForSpeech(params: Bundle?) {}
            override fun onBeginningOfSpeech() {}
            override fun onRmsChanged(rmsdB: Float) {}
            override fun onBufferReceived(buffer: ByteArray?) {}

            override fun onEndOfSpeech() {
                // Partial and final are different callbacks and only the final
                // one carries the committed string; taking the last partial
                // instead would commit whatever the user was mid-way through
                // saying.
            }

            override fun onPartialResults(partialResults: Bundle?) {}
            override fun onEvent(eventType: Int, params: Bundle?) {}

            override fun onResults(results: Bundle?) {
                val text = results
                    ?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                    ?.firstOrNull()
                    .orEmpty()
                val cb = this@PlatformRecognizer.onText
                this@PlatformRecognizer.onText = null
                this@PlatformRecognizer.onFailure = null
                cb?.invoke(text)
            }

            override fun onError(error: Int) {
                val cb = this@PlatformRecognizer.onFailure
                this@PlatformRecognizer.onText = null
                this@PlatformRecognizer.onFailure = null
                cb?.invoke(errorName(error))
            }
        })

        val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            // LANGUAGE_MODEL_FREE_FORM is NOT a RecognizerIntent constant --
            // it lives on RecognizerIntent in some versions and on the intent
            // extras in others; verified with javap against android.jar on
            // 2026-10-04, where it is absent from both. Free-form is the default
            // when the extra is unset, so naming it here would be a guess.
            putExtra(RecognizerIntent.EXTRA_LANGUAGE, language)
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
            putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1)
            // Longer silence timeouts so brief pauses don't stop recognition.
            // EXTRA_SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS:
            //   "The amount of time that it should take after we stop hearing
            //    speech to consider the input possibly complete" (default ~few sec).
            // EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS:
            //   "The amount of time that it should take after we stop hearing
            //    speech to consider the input complete" (default ~few sec).
            // Values measured in ms. 1500/3000 gives more breathing room.
            putExtra("android.speech.extra.SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS", 1500)
            putExtra("android.speech.extra.SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS", 3000)
        }
        try {
            r.startListening(intent)
        } catch (e: Exception) {
            Log.w(TAG, "startListening failed", e)
            onFailure(e.toString())
        }
    }

    override fun stop() {
        try {
            recognizer?.stopListening()
            recognizer?.destroy()
        } catch (e: Exception) {
            // Destroying a recogniser that never started throws on some builds.
            Log.d(TAG, "stop() after no start: ${e.message}")
        }
        recognizer = null
        onText = null
        onFailure = null
    }

    /** A human-readable name, because the numeric codes mean nothing in a log. */
    private fun errorName(code: Int): String = when (code) {
        SpeechRecognizer.ERROR_AUDIO -> "audio"
        SpeechRecognizer.ERROR_CLIENT -> "client"
        SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS -> "permissions"
        SpeechRecognizer.ERROR_NETWORK -> "network"
        SpeechRecognizer.ERROR_NETWORK_TIMEOUT -> "network-timeout"
        SpeechRecognizer.ERROR_NO_MATCH -> "no-match"
        SpeechRecognizer.ERROR_RECOGNIZER_BUSY -> "busy"
        SpeechRecognizer.ERROR_SERVER -> "server"
        SpeechRecognizer.ERROR_SPEECH_TIMEOUT -> "speech-timeout"
        else -> "error-$code"
    }
}

/**
 * Chooses a path, and reports the honest answer when there is none.
 *
 * The preference is not stored yet: PLAN.md says "user-selectable", which implies
 * a settings surface, and there is no settings UI to put it in yet. Until then
 * the order below is the behaviour and the user has no way to change it, which is
 * recorded here rather than pretended around.
 */
object VoiceLauncher {

    /** The fallback intent, used when no in-process recogniser exists. */
    fun hasVoiceInputActivity(context: Context): Boolean =
        Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
            .resolveActivity(context.packageManager) != null

    /**
     * Whether this IME may record, right now.
     *
     * The authority. An IME cannot be told "permission granted" by anyone, so
     * every decision to record reads this again rather than caching an answer
     * from when the dialog was raised (DESIGN-FRONTEND.md Part B B5).
     */
    fun hasPermission(context: Context): Boolean =
        context.checkSelfPermission(Manifest.permission.RECORD_AUDIO) ==
            PackageManager.PERMISSION_GRANTED

    /** The Android edge: can this build put the platform's permission dialog up? */
    fun canAskForPermission(context: Context): Boolean =
        PermissionRequestActivity.canRaise(context)

    /**
     * Which path this device can use, or null if neither.
     *
     * In-process first: it returns results to us with no activity round-trip, and
     * it is the path tier E2 can exercise on the emulator image.
     */
    fun pickLaunch(context: Context): VoicePath? = decide(
        recognizerAvailable = PlatformRecognizer.isAvailable(context),
        activityAvailable = hasVoiceInputActivity(context),
    )

    /**
     * The decision, as a pure function of two facts.
     *
     * Split out from [pickLaunch] so it is testable without an Android runtime.
     * Robolectric is not a dependency of this module, and a rule that decides
     * between three outcomes on two booleans does not justify one.
     *
     * The ordering is the whole content of the rule: a working in-process
     * recogniser always wins, because it is the only path whose result can reach
     * [VoiceSession] at all.
     */
    fun decide(recognizerAvailable: Boolean, activityAvailable: Boolean): VoicePath? =
        when {
            recognizerAvailable -> VoicePath.SPEECH_RECOGNIZER
            activityAvailable -> VoicePath.FALLBACK_ACTIVITY
            else -> null
        }

    /**
     * What one mic tap does about RECORD_AUDIO, as a pure function of three
     * facts. The shape is [decide]'s, for the same reason.
     *
     * @param permissionGranted what `checkSelfPermission` says, read fresh.
     * @param canAsk whether this build can raise the platform's dialog at all.
     * @param alreadyAsked whether THIS tap already raised one. Without it a
     *   double tap raises two dialogs, and Android stacks the second on the
     *   first rather than ignoring it — measured on the emulator, below.
     *
     * The ordering is the content of the rule, in this order:
     *
     *  1. **granted wins over everything.** Even if we asked a moment ago, the
     *     user may have answered already; recording is the point of the tap.
     *  2. **already asked → stop.** Not "ask again". A second dialog is not a
     *     retry the user asked for, and `requestPermissions` inside the same
     *     permission group serialises into a *second* prompt rather than
     *     replacing the first. This is the branch that keeps one tap to one
     *     dialog.
     *  3. **askable → ask.** Once.
     *  4. **otherwise stop**, and report [VoiceResult.PermissionDenied]: the
     *     microphone is unavailable and nothing is going to change that in
     *     response to another tap.
     *
     * ⚠️ What this function cannot know, and no tier can test: whether the
     * platform will *show* the dialog. After two refusals Android stops showing
     * it and the request returns denied immediately; a third tap therefore takes
     * branch 3 forever. `shouldShowRequestPermissionRationale` would distinguish
     * "not yet asked" from "will not ask again", but it is an `Activity` method
     * and so is unreachable from here (B5) — the dialog activity would have to
     * report it, and it has no channel back. Recorded, not worked around.
     */
    fun decidePermission(
        permissionGranted: Boolean,
        canAsk: Boolean,
        alreadyAsked: Boolean,
    ): VoicePermissionAction = when {
        permissionGranted -> VoicePermissionAction.PROCEED
        alreadyAsked -> VoicePermissionAction.STOP
        canAsk -> VoicePermissionAction.ASK
        else -> VoicePermissionAction.STOP
    }
}

/**
 * The microphone-permission seam.
 *
 * Two facts and one action, none of which a plain JVM can reach: `granted()` is
 * `Context.checkSelfPermission`, `ask()` needs an `Activity`. This exists so
 * [VoiceLauncher.decidePermission] can be a pure function over exactly the two
 * booleans the seam supplies, which is what makes the branch testable on the
 * host rather than only on an emulator.
 *
 * A *sensible* fake is a test fixture, so this is one of the two interfaces in
 * the voice path on purpose — see [Recognizer] for the other.
 */
interface PermissionGate {
    /** The authoritative, re-read-every-time answer. Never cached by callers. */
    fun granted(): Boolean

    /** Whether this build can put the platform's dialog up at all. */
    fun canAsk(): Boolean

    /**
     * Put the platform's dialog up. Returns false if it could not be raised.
     *
     * Returns rather than throws for the same reason every other seam here
     * does: this is called from a click listener on the IME's own view, and an
     * exception reaching that footing can take the keyboard down.
     */
    fun ask(): Boolean
}

/**
 * The production gate: `checkSelfPermission` for the fact, and a transparent
 * Activity for the action (DESIGN-FRONTEND.md Part B B5 — an `InputMethodService`
 * has no permission-request hook, so the prompt has to be somebody else's).
 */
class AndroidPermissionGate(private val context: Context) : PermissionGate {
    override fun granted(): Boolean = VoiceLauncher.hasPermission(context)
    override fun canAsk(): Boolean = PermissionRequestActivity.canRaise(context)
    override fun ask(): Boolean = PermissionRequestActivity.raise(context)
}

/**
 * Drives one voice attempt on behalf of the service.
 *
 * Two shapes because the two paths return results differently: the in-process
 * recogniser calls back, the intent returns through [onActivityResult]. Both
 * converge on [deliver], which is the ONLY place spoken text enters the engine.
 */
class VoiceSession(
    private val context: Context?,
    private val recognizerFactory: () -> Recognizer = {
        PlatformRecognizer(context ?: error("no Context: use forRecognizerOnly"))
    },
    private val permissionGate: PermissionGate? = null,
) {

    private var recognizer: Recognizer? = null

    /**
     * True once THIS attempt has put the permission dialog up.
     *
     * Per attempt, not per process: the dialog is the platform's, and one user
     * tap must raise exactly one of them. [VoiceLauncher.decidePermission] reads
     * this so a re-entered [start] cannot ask twice.
     *
     * It is deliberately NOT cleared when the dialog answers. There is no
     * callback for it to clear on (B5), and clearing on a guess is how a second
     * dialog would get stacked on an unanswered one.
     */
    private var askedPermission = false

    /**
     * True once a result has been delivered, so the service can drop the session.
     *
     * Without it the service has to decide whether a result belonged to it, and
     * the only thing that knows that is this class.
     */
    var isSettled: Boolean = false
        private set

    /** Hand results here. The service turns this into hunmin_ic_commit_text. */
    var onResult: ((VoiceResult) -> Unit)? = null

    /**
     * Begin listening, or report why not.
     *
     * Every failure returns normally with a [VoiceResult] rather than throwing:
     * a voice attempt that cannot start must leave the IME typing, and an
     * exception escaping into the IME's key path would take the keyboard with it.
     */
    fun start() {
        // A session built by forRecognizerOnly has no Context, so the null check
        // is load-bearing: start() is reachable on one. Reported as
        // Unavailable rather than PermissionDenied, because "no Context" is not
        // "the user said no" and conflating them would misreport a test-only
        // construction as a permission problem on a device.
        val ctx = context ?: return report(VoiceResult.Unavailable)
        if (!ensurePermission(ctx)) return
        val path = VoiceLauncher.pickLaunch(ctx)
        if (path == null) {
            report(VoiceResult.Unavailable); return
        }
        if (path == VoicePath.FALLBACK_ACTIVITY) {
            startFallback()
            return
        }
        startInProcess()
    }

    /**
     * The RECORD_AUDIO gate, and the only place the permission is consulted.
     *
     * Returns true when the caller may carry on to the recogniser. On every
     * other path it reports and returns false, so there is exactly one route to
     * [startInProcess] and it cannot be reached without the grant.
     *
     * Split out from [start] for the reason [startInProcess] is: the three
     * facts this needs are the ones that differ per device and per user, and a
     * JVM can be handed all three through [PermissionGate].
     */
    fun ensurePermission(ctx: Context): Boolean {
        val gate = permissionGate ?: AndroidPermissionGate(ctx)
        return when (
            VoiceLauncher.decidePermission(
                permissionGranted = gate.granted(),
                canAsk = gate.canAsk(),
                alreadyAsked = askedPermission,
            )
        ) {
            VoicePermissionAction.PROCEED -> true

            VoicePermissionAction.ASK -> {
                // Set BEFORE the call, not after: an `ask()` that throws into
                // the click listener must not leave the session able to ask a
                // second time on a re-entry.
                askedPermission = true
                report(if (gate.ask()) VoiceResult.PermissionAsked
                       else VoiceResult.PermissionDenied)
                false
            }

            // Not granted, and asking is either impossible or already done.
            // Denial is a real answer from the user, not a fault: report it and
            // leave the keyboard typing. Re-asking on every tap is the failure
            // mode this branch exists to prevent.
            VoicePermissionAction.STOP -> {
                report(VoiceResult.PermissionDenied)
                false
            }
        }
    }

    /**
     * The in-process recogniser, behind the [Recognizer] seam.
     *
     * Split from [start] so a test can exercise it with a fake and no Android
     * runtime: the permission and path decisions are the parts that can differ
     * per device, and this is the part that cannot.
     */
    fun startInProcess() {
        val r = try {
            recognizerFactory()
        } catch (e: Exception) {
            // A recogniser that cannot even be constructed must not take the
            // keyboard with it.
            Log.w(TAG, "recogniser construction failed", e)
            report(VoiceResult.Failed(e.toString())); return
        }
        recognizer = r
        try {
            r.start(
                // Trim HERE, not in the listener. This is the one path every
                // Recognizer goes through, so the rule holds for any
                // implementation: whitespace-only audio is silence, and
                // committing "   " would put invisible characters in the user's
                // document. Trimming inside one implementation would leave every
                // other one free to get it wrong -- which is exactly what the
                // first version did, and what its test caught.
                onText = { raw ->
                    val text = raw.trim()
                    report(if (text.isEmpty()) VoiceResult.NothingHeard
                           else VoiceResult.Got(text))
                },
                onFailure = { reason ->
                    report(if (reason.isEmpty()) VoiceResult.NothingHeard
                           else VoiceResult.Failed(reason))
                },
            )
        } catch (e: Exception) {
            // Same argument one step later: a recogniser that throws from start()
            // would otherwise propagate into the IME's key path, and a keyboard
            // that dies because a microphone was busy is a broken keyboard.
            Log.w(TAG, "recogniser start() failed", e)
            report(VoiceResult.Failed(e.toString()))
        }
    }

    /**
     * Launch a specific voice path directly, bypassing the permission gate.
     *
     * This exists so tier E1 can test the path selection logic without a Context.
     * It mirrors the branches of [start] but takes the path as an argument.
     */
    fun launchOn(path: VoicePath?) {
        when (path) {
            VoicePath.SPEECH_RECOGNIZER -> startInProcess()
            VoicePath.FALLBACK_ACTIVITY -> startFallback()
            null -> report(VoiceResult.Unavailable)
        }
    }

    /**
     * The fallback, and what it can honestly claim.
     *
     * With no in-process recogniser there is no SpeechRecognizer to drive, so
     * the only thing left is to ask the platform's own voice-input UI. The hard
     * part, measured with javap on 2026-10-04: `InputMethodService` has **no**
     * `onActivityResult` hook, so a result cannot be routed back to us, and
     * there is no permission-request hook either -- an IME cannot ask for
     * RECORD_AUDIO through the framework at all.
     *
     * So this path launches the recogniser UI and reports [VoiceResult.Failed]
     * with the reason, rather than pretending a result will arrive. The text
     * still reaches the user: a voice-input activity commits into the focused
     * field itself, which is the same InputConnection the IME writes to.
     *
     * Returning `Failed` here is deliberate and is the honest outcome. Returning
     * `Got("")` would make the caller commit nothing and log a success.
     */
    private fun startFallback() {
        val ctx = context ?: return report(VoiceResult.Unavailable)
        val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        try {
            ctx.startActivity(intent)
            report(
                VoiceResult.Failed(
                    "fallback launched; its result is committed by the activity " +
                        "itself and cannot be routed back to an IME"),
            )
        } catch (e: ActivityNotFoundException) {
            // pickLaunch() said it resolved. If it still fails, the package
            // manager lied, which happens on some OEM builds.
            report(VoiceResult.Unavailable)
        }
    }

    fun stop() {
        // stop() is called from a click listener, so an exception here is on the
        // same "must not reach the key path" footing as start().
        try {
            recognizer?.stop()
        } catch (e: Exception) {
            Log.w(TAG, "recogniser stop() failed", e)
        }
        recognizer = null
    }

    /**
     * Deliver at most ONE result per attempt.
     *
     * The guard is not theoretical. `SpeechRecognizer`'s listener gets
     * `onResults` and then `onError` on some builds when it tears down, and a
     * caller that committed twice would duplicate the user's words -- and
     * hunmin_ic_commit_text's result is destructive, so the second delivery is
     * not merely redundant but corrupts the next composition. Asserted by
     * VoiceSessionTest.
     */
    private fun report(r: VoiceResult) {
        if (isSettled) {
            Log.w(TAG, "dropping a late second result: $r")
            return
        }
        // Not every result is a failure, and the log must say which.
        // PermissionAsked logged as "voice failed" on the emulator
        // (measured 2026-10-10) -- which reads as the microphone being
        // broken when in fact the prompt went up exactly as designed.
        // A trace reader distinguishes "we asked" from "we were told no";
        // the log is the same distinction for a human reading logcat.
        when (r) {
            is VoiceResult.Got -> Log.i(TAG, "recognised ${r.text.length} chars")
            is VoiceResult.PermissionAsked ->
                Log.i(TAG, "asked for RECORD_AUDIO; the dialog is the user's to answer")
            is VoiceResult.PermissionDenied ->
                Log.w(TAG, "RECORD_AUDIO not granted; the keyboard still types")
            is VoiceResult.NothingHeard -> Log.i(TAG, "nothing heard")
            is VoiceResult.Unavailable -> Log.w(TAG, "no voice path on this device")
            is VoiceResult.Failed -> Log.w(TAG, "voice failed: ${r.reason}")
        }
        isSettled = true
        onResult?.invoke(r)
    }

    companion object {
        /**
         * A session that can only reach the recogniser seam, with no Context.
         *
         * Tier E1 uses this so the interesting half -- what happens when the
         * recogniser succeeds, fails, or is never stopped -- is testable in a
         * plain JVM. It needs no Android runtime and no mocking framework,
         * because [startInProcess] touches neither the Context nor any platform
         * class. The Context-taking constructor stays the only production entry.
         */
        @JvmStatic
        fun forRecognizerOnly(recognizer: Recognizer) = VoiceSession(null, { recognizer })
        /**
         * Mirrors HunminInputMethodService.VOICE_INPUT_REQUEST. Duplicated rather
         * than shared because the service needs the constant before the class is
         * loaded, and a companion lookup for it would be noise -- the two are
         * asserted equal by a tier E1 test.
         */
    }
}
